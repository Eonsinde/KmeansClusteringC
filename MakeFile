# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g

# Detect the platform: Windows sets the OS environment variable to Windows_NT
ifeq ($(OS),Windows_NT)
    PLATFORM = Windows
    # MinGW provides the math functions in its C runtime, no extra lib needed
    LDLIBS =
else
    PLATFORM = $(shell uname -s)
    # On Linux/Unix the math library (libm) must be linked explicitly
    LDLIBS = -lm
endif

# Output directory
BUILD_DIR = .
# Name of your final executable
TARGET = $(BUILD_DIR)/kmeans

# Automatically find all .c files in the current directory
SRCS = $(wildcard *.c)

# Map *.c files to build/*.o files (e.g., main.c -> build/main.o)
OBJS = $(patsubst %.c, $(BUILD_DIR)/%.o, $(SRCS))

# Default target
all: $(TARGET)

# Linking the final executable
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDLIBS)

# Compiling source files into object files inside the build directory
# The '| $(BUILD_DIR)' syntax ensures the build directory exists first
$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Rule to create the build directory if it doesn't exist
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Clean up build artifacts by wiping the entire build directory
clean:
	rm -rf $(BUILD_DIR)

# Phony targets
.PHONY: all clean