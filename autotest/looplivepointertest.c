#include <assert.h>
#define A ((volatile unsigned char *)0x4000)
#define B ((volatile unsigned char *)0x5000)
int main(void)
{
    volatile unsigned char *p = A;
    for (unsigned i = 0; i < 256; i++) {
        p = A + 768 + i;
        *p = 0x5a;
        B[768 + i] = 0xa5;
    }
    *p = 0x7e;
    assert(A[768] == 0x5a);
    assert(A[1023] == 0x7e);
    assert(B[768] == 0xa5);
    assert(B[1023] == 0xa5);
    return 0;
}
