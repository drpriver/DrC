BUILDTARGETS:=clean list print compile_commands.json all Bin/cpp cpp Bin/cc cc tests \
    test Bin/cpp_test cpp_test run_cpp_test Bin/cc_lex_test cc_lex_test run_cc_lex_test \
    Bin/cc_test cc_test run_cc_test run debug tags
.PHONY: $(BUILDTARGETS)
$(BUILDTARGETS): | build
	@./build $@

build:
	$(CC) build.c -o $@
	./build -b Bin
.DEFAULT_GOAL:=all
