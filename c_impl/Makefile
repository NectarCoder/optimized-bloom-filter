CC := gcc
CFLAGS := -O3 -Wall -Wextra -std=c11
LDFLAGS := -luuid

SRCS := test_bloom_filters.c bloom_filter.c lightweight_bloom_filter.c \
	murmurhash.c/murmurhash.c xxHash/xxhash.c
OBJS := $(SRCS:.c=.o)
TARGET := bloom_bench

.PHONY: all clean run help

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	@echo "Running $(TARGET)..."
	@./$(TARGET)

clean:
	@echo "Cleaning..."
	@rm -f $(OBJS) $(TARGET)

help:
	@echo "Usage: make [target]"
	@echo "Targets:" 
	@echo "  all (default) - build $(TARGET)"
	@echo "  run           - build and run $(TARGET)"
	@echo "  clean         - remove the built files"
