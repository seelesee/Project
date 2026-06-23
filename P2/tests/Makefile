# ============================================
#  STM32 温湿度监测系统 — 单元测试 Makefile
# ============================================
# 在 PC 上编译和运行嵌入式 C 代码的单元测试
#
# 前提: 需要安装 MinGW-w64 或 GCC
#   Windows: 下载 https://winlibs.com 解压到 C:\mingw64
#   MSYS2:   pacman -S mingw-w64-x86_64-gcc
#
# 使用方法:
#   make              → 编译并运行所有测试
#   make test         → 仅运行测试 (已编译)
#   make clean        → 清理编译产物
#

# 编译器设置
CC       = gcc
CFLAGS   = -Wall -Wextra -std=c11 -O0 -g
LDFLAGS  = -lm

# 测试源文件
TEST_SRC = test_dht22.c
TEST_BIN = test_runner.exe

# 头文件路径
INCLUDES = -I. -I../Inc -I../

.PHONY: all test clean

# 默认目标: 编译+运行
all: $(TEST_BIN)
	@echo ""
	@echo "========== 编译完成，开始运行测试 =========="
	@./$(TEST_BIN)
	@echo ""
	@echo "========== 测试运行完毕 =========="

# 编译测试程序
$(TEST_BIN): $(TEST_SRC) unity.h mock_hal.h
	@echo "正在编译测试程序..."
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(TEST_SRC) $(LDFLAGS)
	@echo "编译成功 → $(TEST_BIN)"

# 仅运行 (不重新编译)
test: $(TEST_BIN)
	@./$(TEST_BIN)

# 清理
clean:
	@rm -f $(TEST_BIN)
	@echo "已清理编译产物"
