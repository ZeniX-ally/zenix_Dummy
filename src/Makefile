# src/ 基础 C 程序构建
# 用法: make         编译
#       make run     编译并运行
#       make clean   清理

CC      ?= gcc
CFLAGS  ?= -std=gnu11 -Wall -Wextra -O2
TARGET  := zenix_dummy_demo
SRCS    := main.c kinematics.c
OBJS    := $(SRCS:.c=.o)

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ -lm

%.o: %.c kinematics.h
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(OBJS)
