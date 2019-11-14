#include <kunit/test.h>
#include <linux/timer.h>
#include <linux/errno.h>

#include <asguard/consensus.h>
#include <asguard/asguard.h>
#include <asguard/logger.h>
#include <asguard/payload_helper.h>

/* Just a test to setup the test system.. nothing fancy tested here*/
static void test_asguard_ip_convert(struct kunit *test)
{

    KUNIT_EXPECT_EQ(test, 0xC0A80001, asguard_ip_convert("192.168.0.1"));
    KUNIT_EXPECT_EQ(test, 0x0A40874B, asguard_ip_convert("10.64.135.75"));
    KUNIT_EXPECT_EQ(test, 0x7F000001, asguard_ip_convert("127.0.0.1"));
    KUNIT_EXPECT_EQ(test, 0x00000000, asguard_ip_convert("0.0.0.0"));
    KUNIT_EXPECT_EQ(test, -EINVAL, asguard_ip_convert("some random string"));
    KUNIT_EXPECT_EQ(test, 0xC0A80001, asguard_ip_convert("192.168.0.1"));
    KUNIT_EXPECT_EQ(test, 0xC0A80001, asguard_ip_convert("192.168.0.1"));

}

static struct kunit_case asguard_net_test_cases[] = {
    KUNIT_CASE(test_asguard_ip_convert),
    {}
};

int asguard_test_net_init(struct kunit *test)
{
    return 0;
}

void asguard_test_net_exit(struct kunit *test)
{

}

static struct kunit_suite asguard_test_net_test_suite = {
    .name = "asguard_net_helper",
    .init = asguard_test_net_init,
    .exit = asguard_test_net_exit,
    .test_cases = asguard_net_test_cases,
};
kunit_test_suite(asguard_test_net_test_suite);
