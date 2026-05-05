CC = gcc
CFLAGS = -D_FILE_OFFSET_BITS=64 -Wall -Wextra -Wno-unused-parameter -g -Iinclude $(shell pkg-config fuse3 --cflags 2>/dev/null || pkg-config fuse --cflags)
LDFLAGS = $(shell pkg-config fuse3 --libs 2>/dev/null || pkg-config fuse --libs) -ljansson

SRCDIR = src
INCDIR = include
OBJDIR = objects
BINDIR = bin

SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
TARGET = $(BINDIR)/jsonfs

$(shell mkdir -p $(OBJDIR) $(BINDIR))

.PHONY: all clean install test

all: $(TARGET)d

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c $(INCDIR)/jsonfs.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJDIR)/*.o $(TARGET)

install: $(TARGET)
	sudo cp $(TARGET) /usr/local/bin/

test: $(TARGET)
	@echo "=== JSONFS Test ==="
	@rm -rf /tmp/test.json /tmp/test_mnt
	@echo '{"test":"hello","number":42,"flag":true}' > /tmp/test.json
	@mkdir -p /tmp/test_mnt
	@echo "Starting JSONFS..."
	@$(TARGET) /tmp/test.json /tmp/test_mnt &
	@sleep 2
	@echo "=== Reading ==="
	@cat /tmp/test_mnt/test
	@echo "\n=== Writing with type detection ==="
	@echo "123" > /tmp/test_mnt/number
	@echo "false" > /tmp/test_mnt/flag
	@echo "null" > /tmp/test_mnt/null_val
	@echo "=== Modified status ==="
	@cat /tmp/test_mnt/.modified
	@echo "\n=== Saving ==="
	@echo "save" > /tmp/test_mnt/.save
	@echo "=== Status after save ==="
	@cat /tmp/test_mnt/.modified
	@echo "\n=== Unmounting ==="
	@fusermount3 -u /tmp/test_mnt 2>/dev/null
	@echo "=== Final JSON ==="
	@cat /tmp/test.json
	@rm -rf /tmp/test.json /tmp/test_mnt