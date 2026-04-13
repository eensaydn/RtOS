# RtOS - Cooperative Task Scheduler
# Build system with debug/release targets

CC      = gcc
CFLAGS  = -Wall -Wextra -pedantic -std=c11 -Iinclude

# Source files
SRCS    = src/scheduler.c src/task.c src/context_switch.c examples/main.c
BUILDDIR = build

# Default target: debug build
all: debug

# Debug build: no optimization, debug symbols
debug: CFLAGS += -g -O0 -DDEBUG
debug: $(BUILDDIR)/rtos_demo.exe

# Release build: optimized, no debug
release: CFLAGS += -O2 -DNDEBUG
release: $(BUILDDIR)/rtos_demo.exe

$(BUILDDIR)/rtos_demo.exe: $(SRCS) | $(BUILDDIR)
	$(CC) $(CFLAGS) -o $@ $(SRCS)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

clean:
	rm -rf $(BUILDDIR)

run: debug
	./$(BUILDDIR)/rtos_demo.exe

.PHONY: all debug release clean run
