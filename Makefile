BUILDTARGETS:=clean list print compile_commands.json all \
  soft_float drcpp drc fetch-libffi native-tests tests test \
  self-tests cc_opt softfloat_primitives_test \
  run-softfloat_primitives_test cpp_test run-cpp_test \
  cc_lex_test run-cc_lex_test cc_test run-cc_test ci_test \
  run-ci_test ci_oom_test run-ci_oom_test ci_native_test \
  run-ci_native_test ci_concurrent_test \
  run-ci_concurrent_test drc_test run-drc_test coverage \
  cc_fuzz run_cc_fuzz selfhost self-cpp_test \
  self-cc_lex_test self-cc_test self-ci_test \
  self-ci_native_test self-ci_concurrent_test self-drc_test \
  run_drcpp debug_drcpp run_drc debug_drc repl install tags \
  docs
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
	./build -b builddir
else
$(BUILDTARGETS) $(UNKNOWN): | build
	@./build $@
build:
	$(CC) -march=native build.c -o $@
	./build -b builddir
endif
.DEFAULT_GOAL:=all
