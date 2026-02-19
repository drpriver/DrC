BUILDTARGETS:=all cpp debug run tags tests test cpp_test run_cpp_test cc
.PHONY: $(BUILDTARGETS)
$(BUILDTARGETS): | build
	@./build $@

build: 
	$(CC) build.c -o $@
	./build -b Bin
.DEFAULT_GOAL:=all
