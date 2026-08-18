# zenix_Dummy 统一任务入口
# 用法: make <target>  （或 make help 查看所有命令）

SHELL := /usr/bin/env bash
TOOLS := $(CURDIR)/tools

.PHONY: help check build flash serial clean patches-export patches-apply

help: ## 显示所有可用命令
	@echo "zenix_Dummy 任务入口"
	@grep -E '^[a-zA-Z_-]+:.*?## ' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "  %-16s %s\n", $$1, $$2}'

check: ## 环境自检（工具链/上游/权限）
	@$(TOOLS)/check_env.sh

build: ## 编译两个固件
	@$(TOOLS)/build.sh

flash: ## 烧录固件（ST-Link）
	@$(TOOLS)/flash.sh

serial: ## 串口连通性自检
	@$(TOOLS)/serial_test.sh

clean: ## 清理上游 build 产物
	@$(TOOLS)/clean.sh

patches-export: ## 导出上游修改为补丁
	@$(TOOLS)/export_patches.sh

patches-apply: ## 应用 patches/ 下的补丁到上游
	@$(TOOLS)/apply_patches.sh
