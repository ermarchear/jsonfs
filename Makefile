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

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c $(INCDIR)/jsonfs.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJDIR)/*.o $(TARGET)

install: $(TARGET)
	sudo cp $(TARGET) /usr/local/bin/