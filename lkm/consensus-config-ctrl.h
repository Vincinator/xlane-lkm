#pragma once

#include "../core/module.h"
#include "../core/logger.h"
#include "../core/consensus.h"


#define ASGARD_UUID_BUF 42



void remove_le_config_ctrl_interfaces(struct consensus_priv *priv);
void init_le_config_ctrl_interfaces(struct consensus_priv *priv);