#include <stdarg.h>
#include <stdio.h>

void simple_printf(const char *fmt, ...)
{
    va_list args;

    for (va_start(args, fmt); *fmt != '\0'; ++fmt)
    {
        switch (*fmt)
        {
        case 'd': {
            int i = va_arg(args, int);
            printf("%d\n", i);
            break;
        }
        case 'c': {
            int c = va_arg(args, int);
            printf("%c\n", c);
            break;
        }
        case 'f': {
            float d = va_arg(args, float);
            printf("%f\n", d);
            break;
        }
        default:
            puts("unknown formatter!");
            goto END;
        }
    }
END:
    va_end(args);
}

// Basic Variadic Macro Patterns

#define PRINT(fmt, ...) printf(fmt "\n", __VA_ARGS__)

#define ASSERT(cond, fmt, ...)  \
    if (!(cond))  \
    {  \
        printf(p"FAIL at %s:%d - " fmt "\n", __FILE__, __LINE__, __VA_ARGS__);  \
    }

#define TEST_CODE(name, code, expected, ...)  \
    do  \
    {  \
        printf(p"[%s] ", name);  \
        int result = (code);  \
        printf(p"result=%d, expected=%d " __VA_ARGS__ "\n", result, expected);  \
    } while (0)

int add(const int a, const int b)
{
    return a + b;
}

int multiply(const int a, const int b)
{
    return a * b;
}

int main(void)
{
    // Test 1: Basic variadic usage with different argument counts
    PRINT(p"test 1: %d + %d = %d", 5, 3, add(5, 3));
    PRINT(p"test %d: %d * %d = %d", 2, 4, 7, multiply(4, 7));

    // Test 2: Assertion with custom messages
    ASSERT(add(2, 2) == 4, p"addition check: 2+2=%d", add(2, 2));
    ASSERT(multiply(3, 3) == 9, p"multiplication: 3*3=%d", multiply(3, 3));

    // Test 3: Code generation testing
    TEST_CODE(p"add_test", add(10, 20), 30, p"(10+20)");
    TEST_CODE(p"mul_test", multiply(5, 6), 30, p"(5*6)");

    simple_printf("dcff", 3, p'a', 1.969f, 42.5f);

    return 0;
}
