all : rawtcp

rawtcp : rawtcp.c
	gcc -o $@ $^ -I..

test : rawtcp
	./rawtcp tap1 -

clean :
	rm -rf *.o *~ rawtcp

