CC = gcc
CFLAGS = -W -Wall
TARGET = test

SRCS := $(wildcard *.c)
OBJS := $(SRCS:.c=.o)

all : $(TARGET)

$(TARGET) : $(OBJS)
	$(CC) -static -o $@ $^

.SUFFIXES: .c .o
.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf *.o $(TARGET)
