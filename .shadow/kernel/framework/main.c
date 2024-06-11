// DO NOT MODIFY: Will be reverted by the Online Judge.

#include <kernel.h>
#include <klib.h>

int main() {
    ioe_init();
    cte_init(os->trap);
    os->init();
    while(1);
    mpe_init(os->run);
    return 1;
}
