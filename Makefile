CC         := clang
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

.PHONY: all release format lint test clean

all: build/engine.wasm

build:
	mkdir -p build

build/engine.wasm: engine.c build
	$(CC) --target=$(WASM_TARGET) -std=c23 -nostdlib $(WARNINGS) -O3 -c $< -o $@

format:
	clang-format -Wno-error=unknown -i engine.c
	clang-format -Wno-error=unknown -i engine_tests.c

check-format:
	clang-format -Wno-error=unknown --dry-run --Werror engine.c
	clang-format -Wno-error=unknown --dry-run --Werror engine_tests.c

lint: engine.c
	clang-tidy engine.c -- -std=c23 -nostdlib $(WARNINGS) -O0

build/unit_tests: engine.c engine_tests.c build
	$(CC) -std=c23 $(WARNINGS) -O0 -g -fsanitize=address -fno-omit-frame-pointer engine_tests.c unity.c -o $@

test: check-format build/unit_tests
	./build/unit_tests

clean:
	rm -rf build/
