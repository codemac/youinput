CC=gcc
CFLAGS=-c -O3
LDFLAGS=-lX11 -lXi
SOURCES=youinput.c
OBJECTS=$(SOURCES:.c=.o)
BINARY=youinput

all: $(SOURCES) $(BINARY)

$(BINARY): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

.c.o:
	$(CC) $(CFLAGS) $< -o $@

.PHONY: clean
clean:
	-rm -v $(OBJECTS) $(BINARY)
