#include "mem.h"
#include <stdio.h>

void main()
{
    mem_init();
    void * add = mem_alloc(131072);
    printf("add: %x\n", add);
    mem_alloc(2020);
    afficher();
    mem_destroy();
}
