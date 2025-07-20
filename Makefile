all:
	gcc -Wall ./src/ccache.c -o ./bin/ccache

clean:
	rm -f ./bin/ccache
