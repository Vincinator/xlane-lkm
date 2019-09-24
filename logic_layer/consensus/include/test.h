#pragma once

#include <sassy/sassy.h>

#include <sassy/consensus.h>


struct consensus_test_container {
	struct consensus_priv *priv;
	struct hrtimer timer;
};