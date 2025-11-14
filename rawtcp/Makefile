all : rawtcp

rawtcp : rawtcp.c httptable.h
	gcc -g -Os -o $@ $< -I..

test : rawtcp
	./rawtcp tap1 -

tablegen : tablegen.c
	gcc -Og -g -o $@ $^

httptable.h : tablegen
	./tablegen > httptable.h

clean :
	rm -rf *.o *~ rawtcp httptable.h tablegen

