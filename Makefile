CC = gcc
CFLAGS = -Wall -Wextra -O2 -pthread
TARGET = telnet_server
SOURCES = main.c telnet_server.c telnet_recv.c telnet_proc.c
OBJECTS = $(SOURCES:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS)

%.o: %.c telnet_server.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

debug: CFLAGS += -g -DDEBUG
debug: clean all

install: $(TARGET)
	cp $(TARGET) /usr/local/bin/

.PHONY: all clean debug install