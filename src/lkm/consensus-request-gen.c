//
// Created by vincent on 1/25/21.
//

#include "consensus-request-gen.h"



#define MAX_VALUE_SM_VALUE_SPACE 1024
#define MAX_VALUE_SM_ID_SPACE 255

void testcase_stop_timer(struct consensus_priv *priv)
{
    priv->test_data.running = 0;
}

/* Fully saturates the local log (if leader)
 *
 */
void testcase_one_shot_big_log(struct consensus_priv *priv)
{
    int i, err;
    struct data_chunk *cur_cmd;
    u32 rand_value, rand_id;
    u32 *dptr;
    err = 0;

    for (i = 0; i < MAX_CONSENSUS_LOG; i++) {

        rand_value = prandom_u32_max(MAX_VALUE_SM_VALUE_SPACE);
        rand_id = prandom_u32_max(MAX_VALUE_SM_ID_SPACE);

        // freed by consensus_clean
        cur_cmd = kmalloc(sizeof(struct data_chunk), GFP_KERNEL);

        if (!cur_cmd) {
            err = -ENOMEM;
            goto error;
        }
        dptr = (u32*) cur_cmd->data;
        (*dptr) = rand_id;
        dptr += 1;
        (*dptr) = rand_value;

        err = append_command(priv, cur_cmd, priv->term, i, 0);

        if (err)
            goto error;
    }

    return;

    error:
    asgard_error("Evaluation Crashed errorcode=%d\n", err);
    return;

}

static enum hrtimer_restart testcase_timer(struct hrtimer *timer)
{
    struct consensus_test_container *test_data =
    container_of(timer, struct consensus_test_container, timer);

    struct consensus_priv *priv = test_data->priv;
    u32 rand_value, rand_id;
    ktime_t currtime, interval;
    struct data_chunk *cur_cmd;
    int err = 0;
    int i;
    u32 *dptr;

    int start_idx = priv->sm_log.last_idx + 1;

    if (test_data->running == 0)
        return HRTIMER_NORESTART;

    currtime  = ktime_get();
    interval = ktime_set(1, 0);
    hrtimer_forward(timer, currtime, interval);

    if (priv->nstate != LEADER)
        return HRTIMER_RESTART; // nothing to do, node is not a leader.

    asgard_dbg("Incoming Client requests..\n");

    // write x random entries to local log (if node is leader)
    for (i = start_idx; i < start_idx + test_data->x; i++) {

        rand_value = prandom_u32_max(MAX_VALUE_SM_VALUE_SPACE);
        rand_id = prandom_u32_max(MAX_VALUE_SM_ID_SPACE);

        // freed by consensus_clean
        cur_cmd = kmalloc(sizeof(struct data_chunk), GFP_KERNEL);

        if (!cur_cmd) {
            err = -ENOMEM;
            goto error;
        }
        dptr = (u32*) cur_cmd->data;
        (*dptr) = rand_id;
        dptr += 1;
        (*dptr) = rand_value;
        err = append_command(priv, cur_cmd, priv->term, i, 0);
        // write_log(&priv->ins->logger, CONSENSUS_REQUEST, RDTSC_ASGARD);

        if (err)
            goto error;
    }

    // notify the leader to start the log replication (application logic)
    check_pending_log_rep(priv->sdev);
    return HRTIMER_NORESTART; // only one time! no restart ...
    error:
    asgard_error("Evaluation Stopped errorcode=%d\n", err);
    return HRTIMER_NORESTART;

}

void _init_testcase_timeout(struct consensus_test_container *test_data)
{
    ktime_t timeout;

    // timeout to 1 second
    timeout = ktime_set(1, 0);

    hrtimer_init(&test_data->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL_PINNED);

    test_data->timer.function = &testcase_timer;
    test_data->running = 1;
    hrtimer_start(&test_data->timer, timeout, HRTIMER_MODE_REL_PINNED);
}


void testcase_X_requests_per_sec(struct consensus_priv *priv, int x)
{

    priv->test_data.priv = priv;
    priv->test_data.x = x;

    if (x < 0)  {
        asgard_dbg("Invalid Input\n");
        return;
    }

    // start hrtimer!
    _init_testcase_timeout(&priv->test_data);

    //asgard_dbg("Appending %d consensus requests every second, if node is currently the leader\n", x);

}
