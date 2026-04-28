# Compiler (supports cross-compilation)
CC ?= $(CROSS_COMPILE)gcc

# Compiler flags
CFLAGS ?= -Wall -Werror -g
LDFLAGS ?=

CFLAGS += -DUSE_AESD_CHAR_DEVICE=1

# Target executable
TARGET := aesdsocket

# Source files
SRC := aesdsocket.c

# Object files
OBJ := $(SRC:.c=.o)

# Default target
all: $(TARGET)

# Link step
$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) $(OBJ) -o $(TARGET)

# Compile step
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean target
clean:
	rm -f $(TARGET) $(OBJ)
