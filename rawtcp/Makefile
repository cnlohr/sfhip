all : rawtcp

rawtcp : rawtcp.c
	gcc -o $@ $^ -I..

test : rawtcp
	./rawtcp tap1 -

tablegen : tablegen.c
	gcc -o $@ $^

httptable.h : tablegen
	./tablegen > httptable.h

clean :
	rm -rf *.o *~ rawtcp

