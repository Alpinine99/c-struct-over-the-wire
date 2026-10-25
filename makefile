FLAGS := -Iinclude -Wall -Wextra -std=c99

all: bin/server bin/client

bin/client: build/client.o | bin
	gcc -o $@ $<

bin/server: build/server.o | bin
	gcc -o $@ $<

bin:
	mkdir -p $@

build/%.o: src/%.c | build
	gcc -o $@ -c $< $(FLAGS)

build:
	mkdir -p $@

clean:
	$(RM) -r bin build

PHONY: all clean

