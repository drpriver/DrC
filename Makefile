BUILDTARGETS:=all cpp debug run tags tests test cpp_test run_cpp_test cc cc_test run_cc_test cc_lex_test run_cc_lex_test
.PHONY: $(BUILDTARGETS)
$(BUILDTARGETS): | build
	@./build $@

build: 
	$(CC) build.c -o $@
	./build -b Bin
.DEFAULT_GOAL:=all
