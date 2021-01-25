#pragma once

#include "../core/module.h"
#include "../core/logger.h"
#include "../core/consensus.h"
#include "consensus-request-gen.h"



void remove_eval_ctrl_interfaces(struct consensus_priv *priv);
void init_eval_ctrl_interfaces(struct consensus_priv *priv);