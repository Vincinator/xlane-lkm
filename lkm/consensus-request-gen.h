#pragma once

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/timer.h>

#include "../libasraft.h"

#include "../ringbuffer.h"
#include "../candidate.h"
#include "../follower.h"
#include "../leader.h"
#include "../consensus.h"
#include "../module.h"
#include "../kvstore.h"
#include "../replication.h"


void testcase_X_requests_per_sec(struct consensus_priv *priv, int x);
static enum hrtimer_restart testcase_timer(struct hrtimer *timer);
void testcase_one_shot_big_log(struct consensus_priv *priv);
void testcase_stop_timer(struct consensus_priv *priv);