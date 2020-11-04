#pragma once

#include <asgard/asgard.h>
#include <asgard/consensus.h>

int check_handle_nomination(struct consensus_priv *priv, u32 param1, u32 param2, u32 param3, u32 param4, int rcluster_id, int remote_lid);
