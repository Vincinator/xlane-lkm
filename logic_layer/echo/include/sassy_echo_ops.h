#include <sassy/sassy.h>


struct sassy_echo_priv {
	int test;
};

int echo_init(struct sassy_device*);
int echo_start(struct sassy_device*);
int echo_stop(struct sassy_device*);
int echo_clean(struct sassy_device*);
int echo_info(struct sassy_device*);