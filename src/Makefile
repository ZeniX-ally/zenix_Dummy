# src/ 基础 C 程序构建
# 用法: make         编译
#       make run     编译并运行
#       make test    等价于 make run
#       make clean   清理

CC      ?= gcc
CFLAGS  ?= -std=c11 -Wall -Wextra -O2 -pedantic
TARGET  := zenix_dummy_demo
SRCS    := main.c kinematics.c protocol.c trajectory.c
OBJS    := $(SRCS:.c=.o)
HEADERS := kinematics.h protocol.h trajectory.h

.PHONY: all run test clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ -lm

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

test: run

clean:
	rm -f $(TARGET) $(OBJS)
