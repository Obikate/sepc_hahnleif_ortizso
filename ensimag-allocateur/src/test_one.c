#include "mem.h"
#include <stdio.h>

void main()
{
    mem_init();
    mem_alloc(131072);
    afficher();
    mem_destroy();
}
