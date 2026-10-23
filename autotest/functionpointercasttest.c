/* Anonymous function-pointer types must work as cast targets. */
#include <assert.h>

static volatile unsigned calls;
static volatile int first, second;
static volatile unsigned wideValue;

__noinline void countCall(void)
{
    ++calls;
}

__noinline int add(int a, int b)
{
    ++calls;
    return a + b;
}

__noinline unsigned long widen(unsigned value)
{
    ++calls;
    return 0x12340000ul + value;
}

typedef void (*VoidFunction)(void);
typedef int (*BinaryFunction)(int, int);
typedef unsigned long (*WideFunction)(unsigned);

#ifdef TYPEDEF_CONTROL
#define VOID_FUNCTION(address) ((VoidFunction)(address))
#define CONST_VOID_FUNCTION(address) ((VoidFunction)(address))
#define VOLATILE_VOID_FUNCTION(address) ((VoidFunction)(address))
#define VOID_FUNCTION_SIZE sizeof(VoidFunction)
#define BINARY_FUNCTION(address) ((BinaryFunction)(address))
#define WIDE_FUNCTION(address) ((WideFunction)(address))
#else
#define VOID_FUNCTION(address) ((void (*)(void))(address))
#define CONST_VOID_FUNCTION(address) ((void (* const)(void))(address))
#define VOLATILE_VOID_FUNCTION(address) ((void (* volatile)(void))(address))
#define VOID_FUNCTION_SIZE sizeof(void (*)(void))
#define BINARY_FUNCTION(address) ((int (*)(int, int))(address))
#define WIDE_FUNCTION(address) ((unsigned long (*)(unsigned))(address))
#endif

int main(void)
{
    /* Volatile addresses exercise indirect calls as well as constant targets. */
    volatile VoidFunction voidAddress = countCall;
    volatile BinaryFunction binaryAddress = add;
    volatile WideFunction wideAddress = widen;

    VoidFunction fixedAddress = VOID_FUNCTION(0x8004);
    assert((unsigned)fixedAddress == 0x8004);
    assert(VOID_FUNCTION_SIZE == sizeof(VoidFunction));
    assert((unsigned)CONST_VOID_FUNCTION(0x8004) == 0x8004);
    assert((unsigned)VOLATILE_VOID_FUNCTION(0x8004) == 0x8004);

    calls = 0;
    VOID_FUNCTION(countCall)();
    VOID_FUNCTION(voidAddress)();
    assert(calls == 2);

    first = -123; second = 456;
    assert(BINARY_FUNCTION(add)(first, second) == 333);
    first = -321; second = -456;
    assert(BINARY_FUNCTION(binaryAddress)(first, second) == -777);
    assert(calls == 4);

    wideValue = 0xabcd;
    assert(WIDE_FUNCTION(widen)(wideValue) == 0x1234abcdul);
    wideValue = 0xffff;
    assert(WIDE_FUNCTION(wideAddress)(wideValue) == 0x1234fffful);
    assert(calls == 6);

    /* A stored callback and the typedef spelling remain equivalent. */
    BinaryFunction callback = BINARY_FUNCTION(binaryAddress);
    first = 300; second = 700;
    assert(callback(first, second) == 1000);
    first = -300; second = 100;
    assert(((BinaryFunction)binaryAddress)(first, second) == -200);
    assert(calls == 8);
    return 0;
}
