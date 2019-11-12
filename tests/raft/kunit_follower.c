#include <kunit/test.h>
#include <asguard/consensus.h>
#include <asguard/asguard.h>
#include <asguard/logger.h>


static void asguard_test_check_append_rpc(struct kunit *test)
{
    //pkt_size, prev_log_term, prev_log_idx, max_entries_per_pkt

    /* If pkt size is -1, do not append */
    KUNIT_EXPECT_EQ(test, 1, _check_append_rpc(-1, 1, 1, 10));

    /* If log is full, do not append */
    KUNIT_EXPECT_EQ(test, 1, _check_append_rpc(10, 1, MAX_CONSENSUS_LOG, 10));

    /* pkt size within limit */
    KUNIT_EXPECT_EQ(test, 0,
    _check_append_rpc(AE_ENTRY_SIZE * 100 + ASGUARD_PROTO_CON_AE_BASE_SZ, 1, 5, 100));

    /* pkt size exceeds limit */
    KUNIT_EXPECT_EQ(test, 1,
    _check_append_rpc(AE_ENTRY_SIZE * 100 + ASGUARD_PROTO_CON_AE_BASE_SZ + 1, 1, 5, 100));

}



static struct kunit_case asguard_raft_follower_test_cases[] = {
    KUNIT_CASE(asguard_test_check_append_rpc),
    {}
};

static struct kunit_suite asguard_raft_follower_test_suite = {
    .name = "asguard_raft_follower",
    .test_cases = asguard_raft_follower_test_cases,
};
kunit_test_suite(asguard_raft_follower_test_suite);
