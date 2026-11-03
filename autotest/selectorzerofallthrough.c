#include <stdio.h>
#ifndef __OSCAR64C__
#define __noinline __attribute__((noinline))
#endif

volatile unsigned char output_kind, output_index;
volatile unsigned char inputs[] = {0, 1, 2, 3, 4, 5, 6};
const unsigned char expected_kind[2][7] = {
    {19, 9, 16, 16, 16, 16, 16},
    {21, 11, 17, 17, 17, 17, 17}
};
const unsigned char expected_index[] = {0, 0, 0, 1, 2, 3, 4};

static __noinline void consume(unsigned char kind, unsigned char index)
{
    output_kind = kind;
    output_index = index;
}

static __noinline void select_item(unsigned char which, unsigned char erase)
{
    // The final else must reach consume even when which - 2 is zero.
    // Older native codegen used BNE after SBC #2 as an unconditional jump,
    // falling into the which == 1 branch for which == 2.
    unsigned char kind, index;
    if (which == 0) { kind = erase ? 21 : 19; index = 0; }
    else if (which == 1) { kind = erase ? 11 : 9; index = 0; }
    else { kind = erase ? 17 : 16; index = which - 2; }
    consume(kind, index);
}

int main(void)
{
    unsigned char i, e, failed = 0;
    for (e = 0; e < 2; ++e) {
        for (i = 0; i < 7; ++i) {
            select_item(inputs[i], e);
            printf("%u,%u -> %u,%u\n", (unsigned)i, (unsigned)e,
                   (unsigned)output_kind, (unsigned)output_index);
            if (output_kind != expected_kind[e][i] ||
                output_index != expected_index[i]) failed = 1;
        }
    }
    return failed;
}
