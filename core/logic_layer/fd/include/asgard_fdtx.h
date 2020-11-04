#pragma once
#include <asgard/asgard.h>

#include "asgard_fd.h"

int asgard_bypass_init_class(struct asgard_device *sdev);
void asgard_clean_class(struct asgard_device *sdev);
int asgard_setup_chardev(struct asgard_device *sdev, struct asgard_fd_priv *priv);

