#include "mem.h"
#include <stdio.h>

void main()
{
    mem_init();
    void * add0 = mem_alloc(4);
    printf("--------------------\n");
    void * add1 = mem_alloc(16);
    printf("--------------------\n");
    mem_free(add1, 16);
    printf("--------------------\n");
    mem_destroy();
}
