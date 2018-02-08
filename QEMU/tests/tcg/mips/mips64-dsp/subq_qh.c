#include "io.h"

int main(void)
{
    long long rd, rs, rt, result, dspreg, dspresult;
    rt = 0x123456789ABCDEF0;
    rs = 0x123456789ABCDEF0;
    result = 0x0;
    dspresult = 0x0;

    __asm
        ("subq.qh %0, %2, %3\n\t"
         "rddsp %1\n\t"
         : "=r"(rd), "=r"(dspreg)
         : "r"(rs), "r"(rt)
        );
    dspreg = (dspreg >> 20) & 0x1;
    if ((rd != result) || (dspreg != dspresult)) {
        printf("subq.qh error\n\t");

        return -1;
    }

    return 0;
}

