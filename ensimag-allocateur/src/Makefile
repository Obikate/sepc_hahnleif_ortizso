mem:
	gcc -c mem.c -o mem.o

test_one: mem
	gcc test_one.c mem.o -o test_one

.PHONY: clean
clean:
	rm -f mem.o test_one.o test_one
