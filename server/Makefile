CC = $(CROSS_COMPILE)gcc
CFLAGS ?= -Wall -Werror -Wextra -pedantic -g
LDFLAGS ?=

TARGET = aesdsocket
SOURCES = aesdsocket.c
OBJECTS = $(SOURCES:.c=.o)

.PHONY: all default clean

all: $(TARGET)

default: all

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) $(OBJECTS)
