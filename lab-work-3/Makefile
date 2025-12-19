CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99
LDFLAGS = -lm

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = .

SRCS = $(SRC_DIR)/main.c \
       $(SRC_DIR)/stcompress.c \
       $(SRC_DIR)/arithmetic.c \
       $(SRC_DIR)/optarithm.c \
       $(SRC_DIR)/huffman.c \
       $(SRC_DIR)/lz77.c

OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

TARGET = $(BIN_DIR)/stcompress

.PHONY: all clean debug

all: $(TARGET)

debug: CFLAGS = -Wall -Wextra -g -std=c99 -DDEBUG
debug: clean $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(OBJ_DIR) $(TARGET)
	rm -f tmp/*.compressed tmp/*.decompressed

run-arithmetic: $(TARGET)
	./$(TARGET) -a arithmetic -i ./res/model.safetensors

run-optarithm: $(TARGET)
	./$(TARGET) -a optarithm -i ./res/model.safetensors

run-huffman: $(TARGET)
	./$(TARGET) -a huffman -i ./res/model.safetensors

run-lz77: $(TARGET)
	./$(TARGET) -a lz77 -i ./res/model.safetensors

help:
	@echo "Usage:"
	@echo "  make                 - Build the project"
	@echo "  make debug           - Build with debug symbols"
	@echo "  make clean           - Remove build artifacts"
	@echo "  make run-arithmetic  - Run with arithmetic coding"
	@echo "  make run-optarithm   - Run with arithmetic optmized coding"
	@echo "  make run-huffman     - Run with Huffman coding"
	@echo "  make run-lz77        - Run with LZ77"
	@echo ""
	@echo "Program usage:"
	@echo "  ./stcompress -a <algorithm> [-i <input_file>] [-m <memory_mb>]"
	@echo ""
	@echo "Algorithms: arithmetic, optarithm, huffman, lz77"
