# qdrant-knowledge-mcp

将现有 **Qdrant 向量库 + Ollama bge-m3 嵌入** 打包为 **opencode local MCP Server**（stdio）。

- Qdrant `http://192.168.1.16:6333`（API_KEY 鉴权，v1.15.5，4 collections：`profile 86 / task 20 / pdf_knowledge 1316 / memory 675`，1024 维 Cosine）
- Ollama `http://192.168.1.232:11434`，模型 `bge-m3:latest`，接口 `POST /api/embed {"model":"bge-m3:latest","input":"text"}`
- 传输：`stdio`，名称 `qdrant-knowledge-mcp`，基于 `from mcp.server.fastmcp import FastMCP`

---

## 目录结构

```
mcp-qdrant/
├── server.py        # MCP Server 主程序（FastMCP，8 工具，lifespan 启动自检）
├── pyproject.toml   # 依赖声明（mcp / qdrant-client / httpx）
├── requirements.txt # 同上（pip install -r）
├── .env.example     # 环境变量示例
└── README.md        # 本文件
```

---

## 快速开始

### 1. 安装依赖

```powershell
pip install -r mcp-qdrant/requirements.txt
# 或
pip install -e mcp-qdrant
```

已验证版本：`python 3.14 / mcp 1.27.1 / qdrant-client 1.19.0 / httpx 0.28.1`。

### 2. 配置环境变量（可选）

默认值已内置，无需配置即可连接当前内网环境；如需覆盖：

```powershell
$env:QDRANT_URL="http://192.168.1.16:6333"
$env:QDRANT_API_KEY="changeme-replace-with-real-key"  # 勿提交真实 Key，见 .env.example
$env:OLLAMA_URL="http://192.168.1.232:11434"
$env:OLLAMA_MODEL="bge-m3:latest"
```

或复制 `.env.example` 为 `.env` 并自行加载（`server.py` 读取 `os.getenv`，不自动加载 `.env`，可配合 `python-dotenv` 或在启动前 `export`）。

| 变量 | 默认值 | 说明 |
|------|--------|------|
| `QDRANT_URL` | `http://192.168.1.16:6333` | Qdrant HTTP 地址 |
| `QDRANT_API_KEY` | `changeme` | 鉴权 Key（见 `.env.example`） |
| `OLLAMA_URL` | `http://192.168.1.232:11434` | Ollama 服务地址 |
| `OLLAMA_MODEL` | `bge-m3:latest` | 嵌入模型（需 `ollama pull bge-m3:latest`） |

### 3. 本地自检

```powershell
python mcp-qdrant/server.py --help
python mcp-qdrant/server.py --help-tools
# 启动后会在 stderr 打印 Qdrant / Ollama 连通性
```

### 4. 对接 opencode（local MCP）

`opencode.json`（**本任务不自动修改**，需你手动添加）：

```json
{
  "mcp": {
    "qdrant-knowledge": {
      "type": "local",
      "command": ["python", "F:\\LinuxBackup_20260820\\zenix_Dummy\\mcp-qdrant\\server.py"],
      "enabled": true
    }
  }
}
```

路径需为绝对路径，Windows 注意 `\\` 转义。支持传入环境变量覆盖默认值时，可在启动 opencode 前 `set`/`$env:`。

重启 opencode 后，模型即可调用以下工具。

---

## 工具列表（8 个）

> 每个工具均有中英双语 `description`，返回文本化 JSON（`ensure_ascii=False, indent=2`），错误时返回 `{error, type, hint}` 友好提示。

| 工具 | 签名 | 说明 |
|------|------|------|
| `list_collections` | `list_collections() -> str` | 列出所有 collections 及 `points_count` / `vectors_count` / `status` |
| `collection_info` | `collection_info(collection_name: str) -> str` | 指定 collection 详情（含向量配置、索引、shard 等） |
| `embed_text` | `embed_text(text: str \| list[str]) -> str` | 调用 Ollama `/api/embed` 向量化；单条返回 `{embedding, dimension}`，批量返回 `{embeddings, count}`，维度 1024 |
| `search` | `search(collection: str, query: str, limit: int=5, with_payload: bool=True) -> str` | 语义搜索：先 embed query，再 `query_points`，返回 `score+payload` 排行 |
| `scroll` | `scroll(collection: str, limit: int=10, offset: int\|str\|None=None, with_vector: bool=False) -> str` | 分页浏览 collection，返回 `points` 与 `next_offset`（`offset` 透传给 Qdrant） |
| `upsert` | `upsert(collection: str, texts: list[str], payloads: list[dict]\|None=None, ids: list[int\|str]\|None=None) -> str` | 批量 embed 后 `upsert`；`payloads`/`ids` 长度需与 `texts` 一致，未提供 `ids` 则自动 `uuid4`，`payload` 自动注入 `text` 字段 |
| `delete` | `delete(collection: str, ids: list[int\|str]) -> str` | 按 `ids` 删除点 |
| `create_collection` | `create_collection(collection: str, distance: str="Cosine") -> str` | 创建 1024 维新表；`distance` 可选 `Cosine/Euclid/Dot/Manhattan`（大小写不敏感） |

### 返回示例

**list_collections**
```json
{
  "collections": [
    { "name": "pdf_knowledge", "points_count": 1316, "vectors_count": 1316, "status": "green" }
  ],
  "total": 4
}
```

**search**
```json
{
  "collection": "pdf_knowledge",
  "query": "谐波减速器选型",
  "limit": 5,
  "count": 5,
  "results": [
    { "id": "uuid-...", "score": 0.82, "payload": { "text": "...", "source": "..." } }
  ]
}
```

**scroll**
```json
{
  "collection": "memory",
  "limit": 10,
  "offset": null,
  "next_offset": "42",
  "count": 10,
  "points": [{ "id": "1", "payload": {...}, "vector": null }]
}
```

**错误**
```json
{
  "error": "Unauthorized (401)...",
  "type": "UnexpectedResponse",
  "context": "list_collections",
  "hint": "鉴权失败：请检查 QDRANT_API_KEY 是否正确 / 是否过期"
}
```

---

## 实现细节

- **FastMCP**：`from mcp.server.fastmcp import FastMCP`，`name="qdrant-knowledge-mcp"`，`lifespan` 启动时测试 Qdrant `get_collections` 与 Ollama `embed("ping")`，结果打印到 `stderr`（不污染 stdio）。
- **Qdrant 客户端**：`QdrantClient(url, api_key, timeout=30, check_compatibility=False)`，每次工具调用新建客户端并 `close()`，避免复用失效；`query_points` / `scroll` / `upsert` / `delete` / `create_collection` 均透传 `wait=True`。
- **Ollama 嵌入**：`httpx.Client(timeout=60).post(f"{OLLAMA_URL}/api/embed", json={"model": OLLAMA_MODEL, "input": text_or_list})`，兼容单条 `string` 与批量 `string[]`；校验维度 1024，维度不符仅告警。
- **类型提示**：全工具带 `str | list[str]` / `Optional` 等注解，便于 opencode 推断与校验。
- **错误处理**：统一捕获 `Exception`，按 `401/403/connect/timeout/not found` 给出中文 `hint`；鉴权/连接失败时明确提示排查点。
- **向量维度**：固定 `VECTOR_SIZE=1024`，`create_collection` 默认 `Cosine`，与现有 4 表一致。
- **IDs**：`upsert` 未传 `ids` 时 `uuid4()`；`delete`/`scroll` 支持 `int` 与 `uuid str`；空字符串 `offset` 视作 `None`。

---

## 运维

- **Qdrant 升级**：`pip install -U qdrant-client`，本服务 `check_compatibility=False` 兼容 1.15.5+。
- **Ollama 模型**：`ollama pull bge-m3:latest`，`ollama list` 确认存在；若更换模型，需保证维度仍为 1024 或同步修改 `VECTOR_SIZE` 与重建 collection。
- **日志**：所有状态日志走 `stderr`，`stdout` 仅用于 MCP JSON-RPC（stdio 规范），切勿在工具中 `print` 到 `stdout`。
- **排错**：
  - `Qdrant connect failed / hint: 检查 QDRANT_API_KEY` → 校验 `curl -H "api-key: $KEY" http://192.168.1.16:6333/collections`
  - `Ollama connect failed` → `curl http://192.168.1.232:11434/api/embed -d '{"model":"bge-m3:latest","input":"test"}' -H "Content-Type: application/json"`
  - `collection 已存在` → `create_collection` 需换名或先 `client.delete_collection`

---

## 许可

MIT
