#pragma once
#include <sassy/sassy.h>

int sassy_bypass_init_class(void);
void sassy_clean_class(void);
int sassy_setup_chardev(const struct sassy_device *sdev);
