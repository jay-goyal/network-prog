GCCARGS := -O2
LINKERARGS := -lm

build: target

target: pipes.c
	gcc $(GCCARGS) -o pipes pipes.c $(LINKERARGS)

clean:
	rm pipes
