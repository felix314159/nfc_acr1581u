# Compiler and flags
CC = clang
CFLAGS = -Wall -Wextra -std=c99 -I.

# Detect platform
UNAME_S := $(shell uname -s)

# Platform-specific includes and libraries
ifeq ($(UNAME_S),Linux)
    CFLAGS += -I/usr/local/include/PCSC # built from source: -I/usr/local/include/PCSC, apt version: -I/usr/include/PCSC
    LDFLAGS = -lpcsclite
else ifeq ($(UNAME_S),Darwin)
    LDFLAGS = -framework PCSC
endif

# Source files and output
SRC = main.c ndef.c em-4423.c
OBJ = $(SRC:.c=.o)
TARGET = main

# Default rule
all: $(TARGET)

# Linking the executable
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Compiling object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean rule
clean:
	rm -f $(OBJ) $(TARGET)

# Phony targets
.PHONY: all clean
