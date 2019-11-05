#pragma once
#include <asguard/asguard.h>

#include "asguard_fd.h"

int asguard_bypass_init_class(struct asguard_device *sdev);
void asguard_clean_class(struct asguard_device *sdev);
int asguard_setup_chardev(struct asguard_device *sdev, struct asguard_fd_priv *priv);

