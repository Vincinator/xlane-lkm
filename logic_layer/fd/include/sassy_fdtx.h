#pragma once
#include <sassy/sassy.h>

int sassy_bypass_init_class(struct sassy_device *sdev);
void sassy_clean_class(struct sassy_device *sdev);
int sassy_setup_chardev(struct sassy_device *sdev);

