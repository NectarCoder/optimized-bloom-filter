CC := gcc
CFLAGS := -O3 -Wall -Wextra -std=c11
LDFLAGS := -luuid
UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Darwin)
    # MacPorts typically installs to /opt/local
    ifneq ($(wildcard /opt/local/include),)
        CFLAGS += -I/opt/local/include
    endif
    ifneq ($(wildcard /opt/local/lib),)
        LDFLAGS := -L/opt/local/lib $(LDFLAGS)
	endif

    # Intel Homebrew typically installs to /usr/local
    ifneq ($(wildcard /usr/local/include),)
        CFLAGS += -I/usr/local/include
    endif
    ifneq ($(wildcard /usr/local/lib),)
        LDFLAGS := -L/usr/local/lib $(LDFLAGS)
    endif

    # Apple Silicon Homebrew installs to /opt/homebrew
    ifneq ($(wildcard /opt/homebrew/include),)
        CFLAGS += -I/opt/homebrew/include
    endif
    ifneq ($(wildcard /opt/homebrew/lib),)
        LDFLAGS := -L/opt/homebrew/lib $(LDFLAGS)
    endif
endif

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