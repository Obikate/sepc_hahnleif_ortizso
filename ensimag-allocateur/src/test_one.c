#include "mem.h"
#include <stdio.h>

void main()
{
    mem_init();
    mem_alloc(262144);
    mem_destroy();
}
