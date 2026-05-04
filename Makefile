CC = gcc
CFLAGS = -D_FILE_OFFSET_BITS=64 -Wall -Wextra -g -Iinclude $(shell pkg-config fuse3 --cflags)
LDFLAGS = $(shell pkg-config fuse3 --libs) -ljansson -lpthread

SRC_DIR = src
INC_DIR = include
OBJ_DIR = objects
BIN_DIR = bin

SOURCES = $(SRC_DIR)/main.c $(SRC_DIR)/jsonfs.c $(SRC_DIR)/jsonfs_ops.c
OBJECTS = $(OBJ_DIR)/main.o $(OBJ_DIR)/jsonfs.o $(OBJ_DIR)/jsonfs_ops.o
TARGET = $(BIN_DIR)/jsonfs

all: $(OBJ_DIR) $(BIN_DIR) $(TARGET)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(TARGET): $(OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS)
	@echo "Build complete: $(TARGET)"

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(INC_DIR)/jsonfs.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ_DIR)/*.o $(TARGET)

cleanall: clean
	rm -rf $(OBJ_DIR) $(BIN_DIR)

run: $(TARGET)
	./$(TARGET) Json_Files/data.json mnt

.PHONY: all clean cleanall run