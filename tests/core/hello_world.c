#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

/* A test case that does nothing and succeeds. */
static void hello_world(void **state) {
    (void) state; /* unused */
}
/* A test case that does nothing and succeeds. */
static void hello_world2(void **state) {
    (void) state; /* unused */
    assert_int_equal(1, 0);
}

const struct CMUnitTest hello_world_tests[] = {
    cmocka_unit_test(hello_world),
    cmocka_unit_test(hello_world2),
};


int main(void)
{
    return cmocka_run_group_tests(hello_world_tests, NULL, NULL);
}