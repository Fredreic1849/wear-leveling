# ssdsim linux support
all:ssd 
	
clean:
	rm -f ssd *.o *~
.PHONY: clean

ssd: ssd.o avlTree.o flash.o initialize.o pagemap.o WL.o
	clang -g -o ssd ssd.o avlTree.o flash.o initialize.o pagemap.o WL.o
ssd.o: flash.h initialize.h pagemap.h
	clang -c -g ssd.c
flash.o: pagemap.h
	clang -c -g flash.c
initialize.o: avlTree.h pagemap.h
	clang -c -g initialize.c
pagemap.o: initialize.h
	clang -c -g pagemap.c
avlTree.o: 
	clang -c -g avlTree.c
WL.o:
	clang -c -g WL.c
