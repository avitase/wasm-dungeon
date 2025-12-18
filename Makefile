CC            := clang
CLANG_FORMAT  ?= clang-format
CLANG_TIDY    ?= clang-tidy
LLVM_PROFDATA ?= llvm-profdata
LLVM_COV      ?= llvm-cov
WASM_TARGET = wasm32-unknown-unknown

WARNINGS = -Werror \
           -Wall \
		   -Wextra \
		   -Wpedantic \
		   -Wconversion \
		   -Wsign-conversion \
           -Wcast-qual \
		   -Wformat=2 \
		   -Wundef \
		   -Werror=float-equal \
		   -Wshadow \
           -Wcast-align \
		   -Wunused \
		   -Wnull-dereference \
		   -Wdouble-promotion \
           -Wimplicit-fallthrough \
		   -Werror=strict-prototypes \
		   -Wwrite-strings

.PHONY: all format lint test coverage clean

all: build/engine.wasm

coverage: build/coverage.lcov build/coverage.txt

build:
	mkdir -p build

build/engine.wasm: engine.c | build
	$(CC) --target=$(WASM_TARGET) -std=c23 -nostdlib $(WARNINGS) -O3 -c $< -o $@

build/unit_tests: engine.c engine_tests.c | build
	$(CC) -std=c23 $(WARNINGS) -O0 -g -fsanitize=address,undefined -fno-omit-frame-pointer engine_tests.c unity.c -o $@

build/unit_tests_cov: engine.c engine_tests.c | build
	$(CC) -std=c23 $(WARNINGS) -O0 -g -fprofile-instr-generate -fcoverage-mapping engine_tests.c unity.c -o $@

build/coverage.profdata: build/unit_tests_cov
	LLVM_PROFILE_FILE=build/coverage.profraw ./build/unit_tests_cov
	$(LLVM_PROFDATA) merge -sparse build/coverage.profraw -o $@

build/coverage.lcov: build/coverage.profdata
	$(LLVM_COV) export -format=lcov -instr-profile=build/coverage.profdata ./build/unit_tests_cov engine.c > $@

build/coverage.txt: build/coverage.profdata
	$(LLVM_COV) report -instr-profile=build/coverage.profdata ./build/unit_tests_cov engine.c > $@

format:
	$(CLANG_FORMAT) -Wno-error=unknown -i engine.c
	$(CLANG_FORMAT) -Wno-error=unknown -i engine_tests.c

check-format:
	$(CLANG_FORMAT) -Wno-error=unknown --dry-run --Werror engine.c
	$(CLANG_FORMAT) -Wno-error=unknown --dry-run --Werror engine_tests.c

build/lint.stamp: engine.c | build
	$(CLANG_TIDY) engine.c -- -std=c23 -nostdlib $(WARNINGS) -O0
	touch $@

lint: build/lint.stamp

test: check-format lint build/engine.wasm build/unit_tests
	./build/unit_tests

clean:
	rm -rf build/
