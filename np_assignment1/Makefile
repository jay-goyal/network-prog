GCCARGS := -O2
LINKERARGS := -lm

build: target

target: signal_tree.c
	gcc $(GCCARGS) -o signal_tree signal_tree.c $(LINKERARGS)

clean:
	rm signal_tree
