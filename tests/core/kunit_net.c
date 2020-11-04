#include <kunit/test.h>
#include <linux/timer.h>
#include <linux/errno.h>

#include <asgard/consensus.h>
#include <asgard/asgard.h>
#include <asgard/logger.h>
#include <asgard/payload_helper.h>

/* Just a test to setup the test system.. nothing fancy tested here*/
static void test_asgard_ip_convert(struct kunit *test)
{

    KUNIT_EXPECT_EQ(test, (u32) 0xC0A80001, asgard_ip_convert("192.168.0.1"));
    KUNIT_EXPECT_EQ(test, (u32) 0x0A40874B, asgard_ip_convert("10.64.135.75"));
    KUNIT_EXPECT_EQ(test, (u32) 0x7F000001, asgard_ip_convert("127.0.0.1"));
    KUNIT_EXPECT_EQ(test, (u32) 0x00000000, asgard_ip_convert("0.0.0.0"));
    KUNIT_EXPECT_EQ(test, (u32) -EINVAL, asgard_ip_convert("some random string"));
    KUNIT_EXPECT_EQ(test, (u32) 0xC0A80001, asgard_ip_convert("192.168.0.1"));
    KUNIT_EXPECT_EQ(test, (u32) 0xC0A80001, asgard_ip_convert("192.168.0.1"));

}

static struct kunit_case asgard_net_test_cases[] = {
    KUNIT_CASE(test_asgard_ip_convert),
    {}
};

int asgard_test_net_init(struct kunit *test)
{
    return 0;
}

void asgard_test_net_exit(struct kunit *test)
{

}

static struct kunit_suite asgard_test_net_test_suite = {
    .name = "asgard_net_helper",
    .init = asgard_test_net_init,
    .exit = asgard_test_net_exit,
    .test_cases = asgard_net_test_cases,
};
kunit_test_suite(asgard_test_net_test_suite);
