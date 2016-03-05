mem:
	gcc -std=c99 -c mem.c -o mem.o

test_one: mem
	gcc -std=c99 test_one.c mem.o -o test_one

.PHONY: clean
clean:
	rm -f mem.o test_one.o test_one
