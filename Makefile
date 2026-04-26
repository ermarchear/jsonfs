CC = gcc
CFLAGS = -Wall -g -D_GNU_SOURCE
PKG_CFLAGS = $(shell pkg-config --cflags fuse json-c)
PKG_LIBS = $(shell pkg-config --libs fuse json-c)
INCLUDES = -Iinclude

SRCDIR = src
OBJDIR = obj
BINDIR = bin

SOURCES = $(SRCDIR)/main.c \
          $(SRCDIR)/fuse_ops.c \
          $(SRCDIR)/json_utils.c \
          $(SRCDIR)/file_time.c \
          $(SRCDIR)/save.c

OBJECTS = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(SOURCES))

TARGET = $(BINDIR)/jsonfs

all: $(TARGET)

$(TARGET): $(OBJECTS)
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^ $(PKG_LIBS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c include/jsonfs.h
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) $(PKG_CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(OBJDIR)

distclean: clean
	rm -rf $(BINDIR)

run: $(TARGET)
	@mkdir -p mnt
	@echo '{"test": "hello", "folder": {}}' > test.json
	./$(TARGET) test.json mnt -f

test: $(TARGET)
	@mkdir -p mnt
	@echo '{"test": "hello", "folder": {"num": 42}}' > test.json
	@echo "=== Монтируем в фоне ==="
	./$(TARGET) test.json mnt &
	@sleep 2