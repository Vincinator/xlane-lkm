#include <sassy/logger.h>
#include <sassy/sassy.h>


int fd_init(struct sassy_device* sdev){
	sassy_dbg("fd init\n");
	return 0;
}

int fd_start(struct sassy_device* sdev){

	sassy_dbg("fd start\n");
	return 0;
}

int fd_stop(struct sassy_device* sdev){

	sassy_dbg("fd stop\n");
	return 0;
}

int fd_clean(struct sassy_device* sdev){

	sassy_dbg("fd clean\n");
	return 0;
}

int fd_info(struct sassy_device* sdev){

	sassy_dbg("fd info\n");
	return 0;
}


int fd_post_payload(struct sassy_device*, void* payload)
{

	// .. Test only ..
    print_hex_dump(KERN_DEBUG, "SASSY HB: ", DUMP_PREFIX_NONE, 16, 1,
                    payload_raw_ptr, SASSY_PAYLOAD_BYTES, 0);


    sassy_dbg("fd payload received\n");
}

int fd_post_ts(struct sassy_device*, uint64_t ts)
{


    sassy_dbg("fd optimistical timestamp received. \n");

}
