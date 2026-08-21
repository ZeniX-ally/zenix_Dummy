#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Qdrant Knowledge MCP Server — 将 Qdrant 向量库打包给 opencode 使用

- stdio 传输，适配 opencode local MCP:  command ["python", "path/to/server.py"]
- 名称: qdrant-knowledge-mcp
- 依赖: mcp, qdrant-client, httpx
- 环境变量: QDRANT_URL, QDRANT_API_KEY, OLLAMA_URL, OLLAMA_MODEL（均有默认值）

工具一览（8 个）:
  list_collections | collection_info | embed_text | search | scroll | upsert | delete | create_collection
"""

from __future__ import annotations

import argparse
import json
import os
import sys
import uuid
from contextlib import asynccontextmanager
from typing import Any, AsyncIterator, Optional, Union

import httpx
from qdrant_client import QdrantClient
from qdrant_client.models import Distance, PointStruct, VectorParams

from mcp.server.fastmcp import FastMCP

# ---------------------------------------------------------------------------
# 配置（环境变量覆盖，默认值如任务所述）
# ---------------------------------------------------------------------------
QDRANT_URL: str = os.getenv("QDRANT_URL", "http://192.168.1.16:6333")
QDRANT_API_KEY: str = os.getenv("QDRANT_API_KEY", "")
OLLAMA_URL: str = os.getenv("OLLAMA_URL", "http://192.168.1.232:11434")
OLLAMA_MODEL: str = os.getenv("OLLAMA_MODEL", "bge-m3:latest")

VECTOR_SIZE: int = 1024  # bge-m3 / qdrant 固定维度
OLLAMA_EMBED_PATH: str = "/api/embed"
DEFAULT_TIMEOUT: int = 60  # Ollama embedding 可能较慢

# 启动时打印配置（便于排查）
def _get_qdrant_url() -> str:
    return os.getenv("QDRANT_URL", QDRANT_URL)

def _get_qdrant_api_key() -> str:
    return os.getenv("QDRANT_API_KEY", QDRANT_API_KEY)

def _get_ollama_url() -> str:
    return os.getenv("OLLAMA_URL", OLLAMA_URL)

def _get_ollama_model() -> str:
    return os.getenv("OLLAMA_MODEL", OLLAMA_MODEL)


def _qdrant_client() -> QdrantClient:
    """创建 Qdrant 客户端（每次调用新建，避免长连接复用问题）"""
    return QdrantClient(
        url=_get_qdrant_url(),
        api_key=_get_qdrant_api_key() or None,
        timeout=30,
        check_compatibility=False,
    )


def _json(data: Any) -> str:
    """统一 JSON 文本化输出（ensure_ascii=False, indent=2）"""
    return json.dumps(data, ensure_ascii=False, indent=2, default=str)


def _error_response(exc: Exception, context: str = "") -> str:
    """友好错误 JSON"""
    # 针对常见鉴权/连接错误给出中文提示
    msg = str(exc)
    err_type = type(exc).__name__
    hint = ""
    lower = msg.lower()
    if "unauthorized" in lower or "403" in msg or "401" in msg or "forbidden" in lower:
        hint = "鉴权失败：请检查 QDRANT_API_KEY 是否正确 / 是否过期"
    elif "connect" in lower or "connection" in lower or "refused" in lower:
        hint = "连接失败：请检查网络/地址/端口是否可达，防火墙是否放行"
    elif "not found" in lower or "doesn't exist" in lower or "404" in msg:
        hint = "资源不存在：请检查 collection 名称是否正确"
    elif "timeout" in lower:
        hint = "请求超时：服务响应较慢，可稍后重试"

    payload: dict[str, Any] = {
        "error": msg,
        "type": err_type,
    }
    if context:
        payload["context"] = context
    if hint:
        payload["hint"] = hint
    return _json(payload)


# ---------------------------------------------------------------------------
# Ollama embedding 内部封装
# ---------------------------------------------------------------------------
def _embed_sync(texts: list[str], model: str | None = None, ollama_url: str | None = None) -> list[list[float]]:
    """
    同步调用 Ollama /api/embed 批量嵌入
    :param texts: 待嵌入文本列表（非空）
    :return: 向量列表 (len == len(texts), dim == 1024)
    :raises: RuntimeError / httpx.* on failure
    """
    if not texts:
        raise ValueError("texts 不能为空")

    _model = model or _get_ollama_model()
    _url = (ollama_url or _get_ollama_url()).rstrip("/") + OLLAMA_EMBED_PATH

    # Ollama /api/embed 支持 input 为 string 或 string[]
    # 为保持批量语义：单条用 string，多条用 list
    # 但统一用 list 也兼容，取决于 Ollama 版本；这里做分支兼容
    if len(texts) == 1:
        payload_input: str | list[str] = texts[0]
    else:
        payload_input = texts

    payload = {"model": _model, "input": payload_input}

    try:
        with httpx.Client(timeout=DEFAULT_TIMEOUT) as client:
            resp = client.post(_url, json=payload)
    except httpx.ConnectError as e:
        raise RuntimeError(f"Ollama 连接失败 ({_url}): {e}") from e
    except httpx.TimeoutException as e:
        raise RuntimeError(f"Ollama 请求超时 ({_url}, {DEFAULT_TIMEOUT}s): {e}") from e
    except Exception as e:
        raise RuntimeError(f"Ollama 请求异常: {e}") from e

    if resp.status_code != 200:
        # 尝试解析错误体
        body = resp.text[:2000]
        raise RuntimeError(f"Ollama 返回异常 {resp.status_code}: {body}")

    try:
        data = resp.json()
    except Exception as e:
        raise RuntimeError(f"Ollama 返回非 JSON: {resp.text[:500]}") from e

    embeddings = data.get("embeddings")
    if not isinstance(embeddings, list) or not embeddings:
        raise RuntimeError(f"Ollama 返回格式异常，缺少 embeddings 字段: {data}")

    # embeddings 应为 list[list[float]]
    # 若 Ollama 对单条 input 仍返回一维嵌套，需确保外层长度与 texts 一致
    # 某些版本单条返回 [[...]]，符合预期
    if len(embeddings) != len(texts):
        # 兼容：若请求单条但返回单向量被包了一层，仍视为正常
        # 若确实不一致，抛出警告性错误
        if len(texts) == 1 and len(embeddings) == 1:
            pass
        else:
            # 尝试容错：若 embeddings[0] 是 float（说明返回扁平），则包一层
            if embeddings and isinstance(embeddings[0], (int, float)):
                embeddings = [embeddings]  # type: ignore

    # 校验维度
    for idx, vec in enumerate(embeddings):
        if not isinstance(vec, list):
            raise RuntimeError(f"向量格式异常 index={idx}: {type(vec)}")
        if len(vec) != VECTOR_SIZE:
            # 仅警告，不阻断（模型可能变更），但记录维度
            print(
                f"[warn] embedding dim mismatch index={idx} expected={VECTOR_SIZE} got={len(vec)}",
                file=sys.stderr,
            )
    return embeddings  # type: ignore


# ---------------------------------------------------------------------------
# lifespan — 启动时测试连接并在 stderr 打印状态
# ---------------------------------------------------------------------------
@asynccontextmanager
async def lifespan(server: FastMCP) -> AsyncIterator[dict[str, Any]]:
    print("[qdrant-knowledge-mcp] starting...", file=sys.stderr)
    print(
        f"[qdrant-knowledge-mcp] QDRANT_URL={_get_qdrant_url()} OLLAMA_URL={_get_ollama_url()} OLLAMA_MODEL={_get_ollama_model()} VECTOR_SIZE={VECTOR_SIZE}",
        file=sys.stderr,
    )

    # 测试 Qdrant
    try:
        client = _qdrant_client()
        cols = client.get_collections()
        names = [c.name for c in cols.collections]
        print(
            f"[qdrant-knowledge-mcp] Qdrant OK — collections={len(names)} {names}",
            file=sys.stderr,
        )
        client.close()
    except Exception as e:
        print(f"[qdrant-knowledge-mcp] Qdrant connect failed: {e}", file=sys.stderr)
        # 进一步分类提示
        if "401" in str(e) or "403" in str(e) or "Unauthorized" in str(e):
            print("[qdrant-knowledge-mcp] hint: 检查 QDRANT_API_KEY", file=sys.stderr)

    # 测试 Ollama（轻量 embed）
    try:
        # 同步测试即可，放在异步 lifespan 中用线程池或直接 httpx 同步
        vecs = _embed_sync(["ping"])
        print(
            f"[qdrant-knowledge-mcp] Ollama OK — model={_get_ollama_model()} dim={len(vecs[0]) if vecs else '?'}",
            file=sys.stderr,
        )
    except Exception as e:
        print(f"[qdrant-knowledge-mcp] Ollama connect failed: {e}", file=sys.stderr)
        print(
            f"[qdrant-knowledge-mcp] hint: 检查 OLLAMA_URL={_get_ollama_url()} / 模型是否已 pull (ollama pull {_get_ollama_model()})",
            file=sys.stderr,
        )

    try:
        yield {}
    finally:
        print("[qdrant-knowledge-mcp] shutdown", file=sys.stderr)


# ---------------------------------------------------------------------------
# FastMCP 实例
# ---------------------------------------------------------------------------
mcp = FastMCP(
    name="qdrant-knowledge-mcp",
    instructions=(
        "Qdrant 向量知识库 MCP — 提供 collections 管理、Ollama bge-m3 嵌入、语义搜索、浏览、写入等能力。"
        " 默认 Qdrant=http://192.168.1.16:6333 (1024d Cosine, 4 collections), Ollama=http://192.168.1.232:11434 bge-m3:latest。"
    ),
    lifespan=lifespan,
)


# ---------------------------------------------------------------------------
# 工具实现
# ---------------------------------------------------------------------------
@mcp.tool(
    description=(
        "列出所有 collections 及 points 数量 | List all Qdrant collections with points count. "
        "无需参数。返回 name, points_count, vectors_count 等概览。"
    )
)
def list_collections() -> str:
    try:
        client = _qdrant_client()
        try:
            resp = client.get_collections()
            result: list[dict[str, Any]] = []
            for col in resp.collections:
                # col 仅含 name，points_count 需 get_collection 再取；为提高可用性，批量获取详情
                try:
                    info = client.get_collection(col.name)
                    # info.points_count 在新版本可能为 None，尝试多路径
                    points_count = getattr(info, "points_count", None)
                    vectors_count = getattr(info, "vectors_count", None)
                    status = getattr(info, "status", None)
                    # 新版：info.result? 兼容处理
                    result.append(
                        {
                            "name": col.name,
                            "points_count": points_count,
                            "vectors_count": vectors_count,
                            "status": str(status) if status else None,
                        }
                    )
                except Exception as e_inner:
                    # 降级：仅返回 name
                    result.append({"name": col.name, "error": str(e_inner)})
            return _json({"collections": result, "total": len(result)})
        finally:
            client.close()
    except Exception as e:
        return _error_response(e, "list_collections")


@mcp.tool(
    description=(
        "获取指定 collection 详情 | Get detailed info for a Qdrant collection. "
        "参数 collection_name: 集合名称（必传）。"
    )
)
def collection_info(collection_name: str) -> str:
    if not collection_name or not collection_name.strip():
        return _json({"error": "collection_name 不能为空", "hint": "请传入存在的 collection 名称，如 pdf_knowledge / memory / profile / task"})
    try:
        client = _qdrant_client()
        try:
            info = client.get_collection(collection_name.strip())
            # CollectionInfo 可能包含嵌套对象，转为可序列化 dict
            # 优先用 model_dump / dict / json
            data: dict[str, Any]
            if hasattr(info, "model_dump"):
                data = info.model_dump(mode="json")  # pydantic v2
            elif hasattr(info, "dict"):
                data = info.dict()  # pydantic v1
            else:
                # 回退：用 json 序列化再解析
                data = json.loads(json.dumps(info, default=str))
            return _json(data)
        finally:
            client.close()
    except Exception as e:
        return _error_response(e, f"collection_info collection={collection_name}")


@mcp.tool(
    description=(
        "文本向量化 | Embed text via Ollama bge-m3. "
        "参数 text: str 或 list[str]（单条或批量）。返回向量 embeddings，维度 1024。"
    )
)
def embed_text(text: Union[str, list[str]]) -> str:
    try:
        if isinstance(text, str):
            if not text.strip():
                return _json({"error": "text 不能为空字符串"})
            texts = [text]
            single = True
        elif isinstance(text, list):
            if not text:
                return _json({"error": "text 列表不能为空"})
            # 校验元素类型
            for idx, item in enumerate(text):
                if not isinstance(item, str):
                    return _json({"error": f"text[{idx}] 必须为 string，实际为 {type(item).__name__}"})
                if not item.strip():
                    return _json({"error": f"text[{idx}] 不能为空字符串"})
            texts = text
            single = False
        else:
            return _json({"error": f"text 必须为 str 或 list[str]，实际为 {type(text).__name__}"})

        embeddings = _embed_sync(texts)

        if single:
            return _json(
                {
                    "model": _get_ollama_model(),
                    "dimension": len(embeddings[0]) if embeddings else 0,
                    "embedding": embeddings[0],
                    "text": texts[0][:500] + ("..." if len(texts[0]) > 500 else ""),
                }
            )
        else:
            # 批量时为避免输出过大，默认返回维度信息与截断预览，完整向量仍返回（LLM 需要）
            # 若文本过多导致输出超长，客户端可分页调用
            return _json(
                {
                    "model": _get_ollama_model(),
                    "dimension": len(embeddings[0]) if embeddings else 0,
                    "count": len(embeddings),
                    "embeddings": embeddings,
                    "texts_preview": [t[:200] + ("..." if len(t) > 200 else "") for t in texts],
                }
            )
    except Exception as e:
        return _error_response(e, "embed_text")


@mcp.tool(
    description=(
        "语义搜索 | Semantic search: embed query with Ollama then query Qdrant. "
        "参数: collection (str 必传), query (str 必传), limit (int 默认5, 1-100), with_payload (bool 默认true)。"
        " 返回 score+payload 列表，按相似度降序。"
    )
)
def search(collection: str, query: str, limit: int = 5, with_payload: bool = True) -> str:
    if not collection or not collection.strip():
        return _json({"error": "collection 不能为空"})
    if not query or not query.strip():
        return _json({"error": "query 不能为空"})
    if not isinstance(limit, int) or not (1 <= limit <= 100):
        return _json({"error": "limit 必须为 1-100 的整数"})
    if not isinstance(with_payload, bool):
        return _json({"error": "with_payload 必须为 bool"})

    try:
        # 1. embed query
        try:
            vecs = _embed_sync([query])
            q_vector = vecs[0]
        except Exception as e_emb:
            return _error_response(e_emb, "search: Ollama embed failed")

        # 2. query Qdrant
        client = _qdrant_client()
        try:
            resp = client.query_points(
                collection_name=collection.strip(),
                query=q_vector,
                limit=limit,
                with_payload=with_payload,
            )
            # resp 可能是 QueryResponse，其 points 在 resp.points
            points = getattr(resp, "points", resp)
            out: list[dict[str, Any]] = []
            for p in points or []:
                # ScoredPoint: id, score, payload, vector, version
                if hasattr(p, "model_dump"):
                    d = p.model_dump(mode="json")
                elif hasattr(p, "dict"):
                    d = p.dict()
                else:
                    d = {
                        "id": getattr(p, "id", None),
                        "score": getattr(p, "score", None),
                        "payload": getattr(p, "payload", None),
                        "vector": getattr(p, "vector", None),
                    }
                # 确保包含关键字段
                out.append(
                    {
                        "id": d.get("id"),
                        "score": d.get("score"),
                        "payload": d.get("payload"),
                        # vector 默认不返回，除非需要
                        "vector": d.get("vector"),
                    }
                )
            return _json(
                {
                    "collection": collection.strip(),
                    "query": query,
                    "limit": limit,
                    "count": len(out),
                    "results": out,
                }
            )
        finally:
            client.close()
    except Exception as e:
        return _error_response(e, f"search collection={collection} query={query[:50]}")


@mcp.tool(
    description=(
        "浏览 collection 内容 | Scroll / browse points in a collection. "
        "参数: collection (str 必传), limit (int 默认10, 1-100), offset (int|str|None 分页游标), with_vector (bool 默认false)。"
        " 返回 points 及 next_offset，可用于分页遍历。"
    )
)
def scroll(
    collection: str,
    limit: int = 10,
    offset: Optional[Union[int, str]] = None,
    with_vector: bool = False,
) -> str:
    if not collection or not collection.strip():
        return _json({"error": "collection 不能为空"})
    if not isinstance(limit, int) or not (1 <= limit <= 100):
        return _json({"error": "limit 必须为 1-100 的整数"})
    if not isinstance(with_vector, bool):
        return _json({"error": "with_vector 必须为 bool"})

    # offset 校验：允许 int, str, None
    # Qdrant offset 为 PointId（int 或 uuid str）
    try:
        client = _qdrant_client()
        try:
            # 将 offset 为空字符串视作 None
            _offset = None if offset == "" else offset
            records, next_offset = client.scroll(
                collection_name=collection.strip(),
                limit=limit,
                offset=_offset,
                with_payload=True,
                with_vectors=with_vector,
            )
            out: list[dict[str, Any]] = []
            for r in records or []:
                if hasattr(r, "model_dump"):
                    d = r.model_dump(mode="json")
                elif hasattr(r, "dict"):
                    d = r.dict()
                else:
                    d = {
                        "id": getattr(r, "id", None),
                        "payload": getattr(r, "payload", None),
                        "vector": getattr(r, "vector", None),
                    }
                out.append(
                    {
                        "id": d.get("id"),
                        "payload": d.get("payload"),
                        "vector": d.get("vector") if with_vector else None,
                    }
                )
            return _json(
                {
                    "collection": collection.strip(),
                    "limit": limit,
                    "offset": _offset,
                    "next_offset": next_offset,
                    "count": len(out),
                    "points": out,
                }
            )
        finally:
            client.close()
    except Exception as e:
        return _error_response(e, f"scroll collection={collection} offset={offset}")


@mcp.tool(
    description=(
        "批量写入 | Upsert texts into collection: embed then upsert. "
        "参数: collection (str 必传), texts (list[str] 必传, 非空), "
        "payloads (list[dict]|None 可选, 长度需与 texts 一致), "
        "ids (list[int|str]|None 可选, 长度需与 texts 一致，未提供则自动生成 uuid)。"
        " 自动 embed 后写入 Qdrant，返回写入结果。"
    )
)
def upsert(
    collection: str,
    texts: list[str],
    payloads: Optional[list[dict]] = None,
    ids: Optional[list[Union[int, str]]] = None,
) -> str:
    if not collection or not collection.strip():
        return _json({"error": "collection 不能为空"})
    if not isinstance(texts, list) or not texts:
        return _json({"error": "texts 必须为非空 list[str]"})
    for idx, t in enumerate(texts):
        if not isinstance(t, str) or not t.strip():
            return _json({"error": f"texts[{idx}] 必须为非空字符串"})
    if payloads is not None:
        if not isinstance(payloads, list) or len(payloads) != len(texts):
            return _json({"error": "payloads 若提供，长度必须与 texts 一致，且为 list[dict]"})
        for idx, p in enumerate(payloads):
            if not isinstance(p, dict):
                return _json({"error": f"payloads[{idx}] 必须为 dict"})
    if ids is not None:
        if not isinstance(ids, list) or len(ids) != len(texts):
            return _json({"error": "ids 若提供，长度必须与 texts 一致，且为 list[int|str]"})
        for idx, _id in enumerate(ids):
            if not isinstance(_id, (int, str)):
                return _json({"error": f"ids[{idx}] 必须为 int 或 str"})
            if isinstance(_id, str) and not _id.strip():
                return _json({"error": f"ids[{idx}] 字符串不能为空"})

    # 生成 ids 若未提供
    if ids is None:
        ids = [str(uuid.uuid4()) for _ in texts]
    else:
        # 规范化：去除空白
        ids = [str(i).strip() if isinstance(i, str) else i for i in ids]  # type: ignore

    try:
        # 1. embed
        try:
            vectors = _embed_sync(texts)
        except Exception as e_emb:
            return _error_response(e_emb, "upsert: Ollama embed failed")

        # 2. 构造 points
        points: list[PointStruct] = []
        for idx, (txt, vec) in enumerate(zip(texts, vectors)):
            _id = ids[idx]  # type: ignore
            # 尝试将数字字符串转 int？保持原样，Qdrant 支持 int/str uuid
            # payload 合并：用户提供的 payload + 文本
            base_payload: dict[str, Any] = {}
            if payloads is not None and payloads[idx]:
                base_payload.update(payloads[idx])
            # 保留原始文本，字段名为 text（若用户未指定）
            if "text" not in base_payload:
                base_payload["text"] = txt
            # 若需要，可加 content 兼容
            points.append(PointStruct(id=_id, vector=vec, payload=base_payload))

        # 3. upsert
        client = _qdrant_client()
        try:
            res = client.upsert(collection_name=collection.strip(), points=points, wait=True)
            # res 含 operation_id, status
            if hasattr(res, "model_dump"):
                res_data = res.model_dump(mode="json")
            elif hasattr(res, "dict"):
                res_data = res.dict()
            else:
                res_data = {"result": str(res)}
            return _json(
                {
                    "collection": collection.strip(),
                    "upserted": len(points),
                    "ids": ids,
                    "result": res_data,
                }
            )
        finally:
            client.close()
    except Exception as e:
        return _error_response(e, f"upsert collection={collection} count={len(texts)}")


@mcp.tool(
    description=(
        "删除 points | Delete points by ids. "
        "参数: collection (str 必传), ids (list[int|str] 必传, 非空)。"
    )
)
def delete(collection: str, ids: list[Union[int, str]]) -> str:
    if not collection or not collection.strip():
        return _json({"error": "collection 不能为空"})
    if not isinstance(ids, list) or not ids:
        return _json({"error": "ids 必须为非空 list[int|str]"})
    for idx, _id in enumerate(ids):
        if not isinstance(_id, (int, str)):
            return _json({"error": f"ids[{idx}] 必须为 int 或 str"})
        if isinstance(_id, str) and not _id.strip():
            return _json({"error": f"ids[{idx}] 字符串不能为空"})

    try:
        client = _qdrant_client()
        try:
            # 规范化 ids：去除空白
            norm_ids: list[Union[int, str]] = []
            for _id in ids:
                if isinstance(_id, str):
                    norm_ids.append(_id.strip())
                else:
                    norm_ids.append(_id)
            res = client.delete(collection_name=collection.strip(), points_selector=norm_ids, wait=True)
            if hasattr(res, "model_dump"):
                res_data = res.model_dump(mode="json")
            elif hasattr(res, "dict"):
                res_data = res.dict()
            else:
                res_data = {"result": str(res)}
            return _json(
                {
                    "collection": collection.strip(),
                    "deleted_ids": norm_ids,
                    "result": res_data,
                }
            )
        finally:
            client.close()
    except Exception as e:
        return _error_response(e, f"delete collection={collection} ids={ids}")


@mcp.tool(
    description=(
        "创建 collection | Create a new Qdrant collection with 1024-dim vectors. "
        "参数: collection (str 必传, 新集合名), distance (str 可选, 默认 Cosine；可选 Cosine/Euclid/Dot/Manhattan)。"
    )
)
def create_collection(collection: str, distance: str = "Cosine") -> str:
    if not collection or not collection.strip():
        return _json({"error": "collection 不能为空"})
    name = collection.strip()
    if not isinstance(distance, str):
        return _json({"error": "distance 必须为字符串"})

    # distance 映射（大小写不敏感）
    dist_map: dict[str, Distance] = {
        "cosine": Distance.COSINE,
        "euclid": Distance.EUCLID,
        "euclidean": Distance.EUCLID,
        "dot": Distance.DOT,
        "manhattan": Distance.MANHATTAN,
    }
    key = distance.strip().lower()
    if key not in dist_map:
        return _json(
            {
                "error": f"不支持的 distance: {distance}",
                "hint": "可选值: Cosine / Euclid / Dot / Manhattan（大小写不敏感）",
            }
        )
    dist = dist_map[key]

    try:
        client = _qdrant_client()
        try:
            # 先检查是否已存在
            try:
                if client.collection_exists(collection_name=name):
                    return _json(
                        {
                            "error": f"collection '{name}' 已存在",
                            "hint": "请更换名称或先删除现有 collection",
                        }
                    )
            except Exception:
                # collection_exists 在某些版本可能抛异常，忽略
                pass

            ok = client.create_collection(
                collection_name=name,
                vectors_config=VectorParams(size=VECTOR_SIZE, distance=dist),
            )
            return _json(
                {
                    "collection": name,
                    "distance": distance,
                    "vector_size": VECTOR_SIZE,
                    "created": bool(ok),
                }
            )
        finally:
            client.close()
    except Exception as e:
        return _error_response(e, f"create_collection name={name} distance={distance}")


# ---------------------------------------------------------------------------
# 入口：支持 --help 及 stdio 启动
# ---------------------------------------------------------------------------
def _build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="server.py",
        description="qdrant-knowledge-mcp — Qdrant 向量库 MCP Server (stdio)",
        formatter_class=argparse.RawTextHelpFormatter,
        epilog=(
            "环境变量:\n"
            "  QDRANT_URL      Qdrant 地址 (默认 http://192.168.1.16:6333)\n"
            "  QDRANT_API_KEY  Qdrant API Key\n"
            "  OLLAMA_URL      Ollama 地址 (默认 http://192.168.1.232:11434)\n"
            "  OLLAMA_MODEL    Embedding 模型 (默认 bge-m3:latest)\n"
            "\n"
            "opencode 本地 MCP 配置示例 (opencode.json):\n"
            '  { "mcp": { "qdrant-knowledge": { "type": "local", "command": ["python", "./mcp-qdrant/server.py"], "enabled": true } } }\n'
            "\n"
            "工具列表:\n"
            "  list_collections | collection_info | embed_text | search | scroll | upsert | delete | create_collection\n"
        ),
    )
    parser.add_argument(
        "--transport",
        choices=["stdio", "sse", "streamable-http"],
        default="stdio",
        help="传输协议 (默认 stdio，适配 opencode local MCP)",
    )
    parser.add_argument("--host", default="127.0.0.1", help="SSE/HTTP 时的 host (默认 127.0.0.1)")
    parser.add_argument("--port", type=int, default=8000, help="SSE/HTTP 时的 port (默认 8000)")
    parser.add_argument("--help-tools", action="store_true", help="展示工具简介后退出")
    return parser


def main() -> None:
    parser = _build_arg_parser()
    args = parser.parse_args()

    if args.help_tools:
        print("Tools for qdrant-knowledge-mcp:", file=sys.stdout)
        # 从 mcp 实例获取已注册工具
        # FastMCP 内部 tool manager 可能不同版本字段不同，兼容处理
        try:
            tools = []
            # 尝试多种获取方式
            if hasattr(mcp, "_tool_manager"):
                tm = getattr(mcp, "_tool_manager")
                if hasattr(tm, "_tools"):
                    tools = list(getattr(tm, "_tools").keys())  # type: ignore
                elif hasattr(tm, "tools"):
                    tools = list(getattr(tm, "tools").keys())  # type: ignore
            if not tools:
                # 回退硬编码
                tools = [
                    "list_collections",
                    "collection_info",
                    "embed_text",
                    "search",
                    "scroll",
                    "upsert",
                    "delete",
                    "create_collection",
                ]
            for t in tools:
                print(f" - {t}", file=sys.stdout)
        except Exception:
            pass
        sys.exit(0)

    # 兼容：python server.py --help 已由 argparse 处理
    # 启动 MCP
    try:
        if args.transport == "stdio":
            mcp.run(transport="stdio")
        elif args.transport == "sse":
            mcp.run(transport="sse", host=args.host, port=args.port)  # type: ignore
        else:
            mcp.run(transport="streamable-http", host=args.host, port=args.port)  # type: ignore
    except KeyboardInterrupt:
        print("\n[qdrant-knowledge-mcp] interrupted", file=sys.stderr)
        sys.exit(0)
    except Exception as e:
        print(f"[qdrant-knowledge-mcp] fatal: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
