#pragma once

#include "../module.h"
#include "../logger.h"
#include "../consensus.h"


#define ASGARD_UUID_BUF 42



void remove_le_config_ctrl_interfaces(struct consensus_priv *priv);
void init_le_config_ctrl_interfaces(struct consensus_priv *priv);