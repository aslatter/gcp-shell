
bin/main: src/main.c Makefile
	@ mkdir -p bin
	@ $(CC) -o bin/gcp-shell -Wall -Wextra -Werror src/main.c

format:
	@ clang-format -i src/*.c

clean:
	@ rm -rf bin

.PHONY: format clean

