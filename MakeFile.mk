# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g

# Output directory
BUILD_DIR = build
# Name of your final executable
TARGET = $(BUILD_DIR)/kmeans

# Automatically find all .c files in the current directory
SRCS = $(wildcard src/*.c)

# Map src/*.c files to build/*.o files (e.g., src/main.c -> build/main.o)
OBJS = $(patsubst src/%.c, $(BUILD_DIR)/%.o, $(SRCS))

# Default target
all: $(TARGET)

# Linking the final executable
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@

# Compiling source files into object files inside the build directory
# The '| $(BUILD_DIR)' syntax ensures the build directory exists first
$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Rule to create the build directory if it doesn't exist
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Clean up build artifacts by wiping the entire build directory
clean:
	rm -rf $(BUILD_DIR)

# Phony targets
.PHONY: all clean