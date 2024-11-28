CC = gcc
CFLAGS = -Wall -Wextra
TARGET = multicast
SRCS = multicast.c

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

clean:
	rm -f $(TARGET)

.PHONY: all clean run 