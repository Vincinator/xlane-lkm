#include <sassy/logger.h>
#include <sassy/sassy.h>

#include "include/sassy_fd.h"

int fd_init(struct sassy_device* sdev)
{
	/* Initialize */

	sassy_dbg("fd init\n");
	return 0;
}


int fd_init_payload(void *payload)
{
	
	struct fd_payload *fd_p = (struct fd_payload*) payload;
	int i;
	
	sassy_dbg("initializing FD payload\n");

	fd_p->protocol_id = SASSY_PROTO_FD;
	fd_p->message = 42;
	fd_p->alive_rp = 43;

	for(i=0; i < MAX_PROCESSES_PER_HOST;i++){
		fd_p->pinfo[i].pid = i & 0xFF;
		fd_p->pinfo[i].ps = 0;
	}

	return 0;
}


int fd_start(struct sassy_device* sdev)
{

	sassy_dbg("fd start\n");
	return 0;
}

int fd_stop(struct sassy_device* sdev)
{

	sassy_dbg("fd stop\n");
	return 0;
}

int fd_clean(struct sassy_device* sdev)
{

	sassy_dbg("fd clean\n");
	return 0;
}

int fd_info(struct sassy_device* sdev)
{

	sassy_dbg("fd info\n");
	return 0;
}


int fd_post_payload(struct sassy_device* sdev, unsigned char *remote_mac, void* payload)
{

	// .. Test only ..
    //print_hex_dump(KERN_DEBUG, "SASSY HB: ", DUMP_PREFIX_NONE, 16, 1,
    //                payload, SASSY_PAYLOAD_BYTES, 0);

	//sassy_dbg("SRC MAC=%pM", remote_mac);
    sassy_dbg("fd payload received\n");
}

int fd_post_ts(struct sassy_device* sdev, unsigned char *remote_mac, uint64_t ts)
{

	//sassy_dbg("SRC MAC=%pM", remote_mac);
    sassy_dbg("fd optimistical timestamp received. \n");

}
