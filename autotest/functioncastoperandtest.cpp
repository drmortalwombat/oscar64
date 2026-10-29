/* Functional casts with dereferences must remain expressions. */
#include <assert.h>

static volatile int source = 37;

template <class T>
int readValue(T *p)
{
    return (int(*p));
}

__noinline int increment(int value)
{
    return value + 1;
}

template <class T>
int invoke(T f, int value)
{
    return ((int (*)(int))f)(value);
}

int main(void)
{
    int value = source;
    int *p = &value;
    int **pp = &p;

    assert((int(*p)) == 37);
    assert((int(*p + 1)) == 38);
    assert((int(**pp)) == 37);
    assert((long(*p)) == 37L);
    assert(sizeof(int(*p)) == sizeof(int));
    assert((int(value) + 1) == 38);
    assert(readValue(p) == 37);
    assert(invoke(increment, source) == 38);

    /* Functional casts still evaluate their operand exactly once. */
    int values[2] = {11, 22};
    p = values;
    assert((int(*p++)) == 11);
    assert(p == values + 1);
    (void(*p++));
    assert(p == values + 2);
    return 0;
}
