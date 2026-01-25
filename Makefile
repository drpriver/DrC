.PHONY: all cpp
cpp all: | build
	./build $@

build: 
	$(CC) build.c -o $@
	./build -b Bin
