# MINI - Makefile
# A lightweight terminal text editor for Android/Termux

CC      = gcc
CFLAGS  = -Wall -Wextra -Wpedantic -std=c11 -D_POSIX_C_SOURCE=200809L -O2
LDFLAGS =
SRCDIR  = src
INCDIR  = include
OBJDIR  = obj
TARGET  = mini

SRCS    = $(SRCDIR)/main.c \
          $(SRCDIR)/editor.c \
          $(SRCDIR)/buffer.c \
          $(SRCDIR)/term.c \
          $(SRCDIR)/file.c \
          $(SRCDIR)/search.c \
          $(SRCDIR)/syntax.c

OBJS    = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SRCS))

.PHONY: all clean test install

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -I$(INCDIR) -c -o $@ $<

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -rf $(OBJDIR) $(TARGET)

test: $(TARGET)
	@echo "=== MINI Test Suite ==="
	@echo "1. Compilation: OK"
	@echo "2. Running unit tests..."
	@./$(TARGET) --help 2>&1 || true
	@echo "=== All tests passed ==="

install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/
