CC = gcc
CFLAGS = -Wall -g -D_GNU_SOURCE
PKG_CFLAGS = $(shell pkg-config --cflags fuse3 jansson)
PKG_LIBS = $(shell pkg-config --libs fuse3 jansson)
INCLUDES = -Iinclude

SRCDIR = src
OBJDIR = obj
BINDIR = bin

SOURCES = $(SRCDIR)/main.c \
          $(SRCDIR)/fuse_callbacks.c \
          $(SRCDIR)/handlers.c \
          $(SRCDIR)/json_operations.c \
          $(SRCDIR)/file_time.c

OBJECTS = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(SOURCES))

TARGET = $(BINDIR)/jsonfs

all: $(TARGET)

$(TARGET): $(OBJECTS)
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^ $(PKG_LIBS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) $(PKG_CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(OBJDIR)

distclean: clean
	rm -rf $(BINDIR)

run: $(TARGET)
	mkdir -p mnt
	./$(TARGET) test.json mnt -f

.PHONY: all clean distclean run