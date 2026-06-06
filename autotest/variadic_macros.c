#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>


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
            assert(i == 3);
            break;
        }
        case 'c': {
            int c = va_arg(args, int);
            printf("%c\n", c);
            assert(c == p'a');
            break;
        }
        case 'f': {
            float d = va_arg(args, float);
            printf("%f\n", d);
            assert(d == 3.14f || d == 42.5f);
            break;
        }
        default:
            puts(p"unknown formatter!");
            assert(false);
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
        assert(cond); \
    }

#define TEST_CODE(name, code, expected, ...)  \
    do  \
    {  \
        printf(p"[%s] ", name);  \
        assert(  \
            strcmp(name, p"add_test") == 0  \
            || strcmp(name, p"mul_test") == 0  \
        );  \
        int result = (code);  \
        printf(p"result=%d, expected=%d " __VA_ARGS__ "\n", result, expected);  \
        assert (result == expected);  \
    } while (0)

int add(const int a, const int b)
{
    return a + b;
}

int multiply(const int a, const int b)
{
    return a * b;
}

#define CALCULATE_SUM(sum, ...)  \
    do { \
        int vals[] = { __VA_ARGS__ }; \
        for (int i = 0; i < (int)(sizeof(vals) / sizeof(vals[0])); i++) \
            sum += vals[i]; \
    } while (0)


int main(void)
{
    // Test 1: Basic variadic usage with different argument counts
    PRINT(p"test 1: %d + %d = %d", 5, 3, add(5, 3));
    PRINT(p"test %d: %d * %d = %d", 2, 4, 7, multiply(4, 7));
    int sum = 0;
    CALCULATE_SUM(sum, 1, 2, 3, 4, 5);
    PRINT(p"sum is %d", sum);
    assert (sum == 15);

    // Test 2: Assertion with custom messages
    ASSERT(add(2, 2) == 4, p"addition check: 2+2=%d", add(2, 2));
    ASSERT(multiply(3, 3) == 9, p"multiplication: 3*3=%d", multiply(3, 3));

    // Test 3: Code generation testing
    TEST_CODE(p"add_test", add(10, 20), 30, p"(10+20)");
    TEST_CODE(p"mul_test", multiply(5, 6), 30, p"(5*6)");

    simple_printf("dcff", 3, p'a', 3.14f, 42.5f);

    return 0;
}
