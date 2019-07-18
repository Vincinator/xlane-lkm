#include <sassy/logger.h>
#include <sassy/sassy.h>


int echo_init(struct sassy_device* sdev){

	

	sassy_dbg("echo init");
	return 0;
}

int echo_start(struct sassy_device* sdev){

	sassy_dbg("echo start");
	return 0;
}

int echo_stop(struct sassy_device* sdev){

	sassy_dbg("echo stop");
	return 0;
}

int echo_clean(struct sassy_device* sdev){

	sassy_dbg("echo clean");
	return 0;
}

int echo_info(struct sassy_device* sdev){

	sassy_dbg("echo info");
	return 0;
}
