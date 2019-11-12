#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <asguard/asguard.h>
#include <asguard/consensus.h>

/* A test case that does nothing and succeeds. */
static void test_become_follower(void **state) {
    (void) state; /* unused */

    check_append_rpc(10, 1, 1, 23);

}
/* A test case that does nothing and succeeds. */
static void hello_world2(void **state) {
    (void) state; /* unused */
    assert_int_equal(1, 0);
}

const struct CMUnitTest hello_world_tests[] = {
    cmocka_unit_test(test_become_follower),
    cmocka_unit_test(hello_world2),
};


int main(void)
{
    return cmocka_run_group_tests(hello_world_tests, NULL, NULL);
}