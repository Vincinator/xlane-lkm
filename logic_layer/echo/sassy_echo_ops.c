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

int echo_post_payload(struct sassy_device* sdev, void *payload){

	// .. Test only ..
    print_hex_dump(KERN_DEBUG, "SASSY HB: ", DUMP_PREFIX_NONE, 16, 1,
                    payload_raw_ptr, SASSY_PAYLOAD_BYTES, 0);

   	sassy_dbg("echo post payload");

	return 0;
}

int echo_post_ts(struct sassy_device* sdev, uint64_t ts){

	sassy_dbg("echo post optimistical ts");
	return 0;
}