BUILDTARGETS:=clean list print compile_commands.json all cpp cc tests test cpp_test \
    run_cpp_test cc_lex_test run_cc_lex_test cc_test run_cc_test run_cpp debug_cpp \
    run_cc debug_cc repl tags
.PHONY: $(BUILDTARGETS)

ifeq ($(OS),Windows_NT)
ifeq ($(origin CC),default)
CC:=$(firstword $(foreach c,clang gcc cl,$(if $(shell where $(c) 2>nul),$(c))))
endif
$(BUILDTARGETS): | build.exe
	@build $@
build.exe:
ifeq ($(CC),cl)
	$(CC) build.c /Fe:$@
else
	$(CC) build.c -o $@
endif
	./build -b Bin
else
$(BUILDTARGETS): | build
	@./build $@
build:
	$(CC) build.c -o $@
	./build -b Bin
endif
.DEFAULT_GOAL:=all
