all:
	gcc -Wall -std=c17 -D_POSIX_C_SOURCE=200809L -march=native ./src/ccache.c -o ./bin/ccache

clean:
	rm -f ./bin/ccache
