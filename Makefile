CFLAGS=-std=c99 -g3 -Wall -O2
LDLIBS=
CC=gcc

SERVER_NAME=helles
SERVER_SOURCES=$(wildcard src/**/*.c src/*.c)
SERVER_OBJECTS=$(patsubst %.c,%.o,$(SERVER_SOURCES))

all: $(SERVER_NAME)

$(SERVER_NAME): $(SERVER_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

clean:
	rm -rf $(SERVER_NAME) $(SERVER_OBJECTS)
	find . -name "*.dSYM" -print | xargs rm -rf
