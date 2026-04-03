BUILDTARGETS:=clean list print compile_commands.json all \
  drcpp drc fetch-libffi tests test selftests cpp_test \
  run_cpp_test cc_lex_test run_cc_lex_test cc_test \
  run_cc_test ci_test run_ci_test ci_native_test \
  run_ci_native_test coverage cc_fuzz run_cc_fuzz cc_opt \
  selfhost self_cpp_test self_cc_lex_test self_cc_test \
  self_ci_test self_ci_native_test run_drcpp debug_drcpp \
  run_drc debug_drc repl install tags
UNKNOWN:=$(filter-out $(BUILDTARGETS) build build.exe Makefile,$(MAKECMDGOALS))
.PHONY: $(BUILDTARGETS) $(UNKNOWN)

ifeq ($(OS),Windows_NT)
ifeq ($(origin CC),default)
CC:=$(firstword $(foreach c,cl clang,$(if $(shell where $(c) 2>/dev/null),$(c))))
endif
$(BUILDTARGETS) $(UNKNOWN): | build.exe
	@build $@
build.exe:
ifeq ($(CC),cl)
	$(CC) /nologo /std:c11 /Zc:preprocessor /wd5105 build.c /Fe:$@
else
	$(CC) -march=native build.c -o $@
endif
	./build -b Bin
else
$(BUILDTARGETS) $(UNKNOWN): | build
	@./build $@
build:
	$(CC) -march=native build.c -o $@
	./build -b Bin
endif
.DEFAULT_GOAL:=all
