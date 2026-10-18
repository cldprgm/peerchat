#ifndef TEST_SUPPORT_H
#define TEST_SUPPORT_H

#define RUN_TEST(test_function, passed, total) \
    do                                         \
    {                                          \
        test_function();                       \
        ++(passed);                            \
        ++(total);                             \
    } while (0)

#define PRINT_TEST_RESULT(passed, total) \
    printf("\nTEST_RESULT %u/%u\n\n", (passed), (total))

#endif
