BUILDTARGETS:=all cpp debug run tags
.PHONY: $(BUILDTARGETS)
$(BUILDTARGETS): | build
	@./build $@

build: 
	$(CC) build.c -o $@
	./build -b Bin
