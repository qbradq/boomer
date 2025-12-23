CC = clang-20
CFLAGS = -std=c23 -Wall -Wextra -Wpedantic -g
LDFLAGS =

SRC_DIR = src
BUILD_DIR = build
TARGET = $(BUILD_DIR)/boomer

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

.PHONY: all clean lint check

all: $(TARGET)

$(TARGET): $(OBJS) | $(BUILD_DIR)
	$(CC) $(LDFLAGS) -o $@ $^

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR):
	mkdir -p $@

clean:
	rm -rf $(BUILD_DIR)

lint:
	clang-tidy-20 $(SRCS) -- -std=c23

check:
	cppcheck --enable=all --std=c23 --suppress=missingIncludeSystem $(SRC_DIR)
