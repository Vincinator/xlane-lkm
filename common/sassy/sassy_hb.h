#ifndef _SASSY_HB_H_
#define _SASSY_HB_H_

#define MAX_PROCESSES_PER_HOST 16



struct sassy_remote_process_info {
	u8 rpid;	/* Process ID on remote host */
	u8 rps;		/* Status of remote process */
};


struct sassy_heartbeat_packet {
	u8 alive_rp;						/* Number of alive processes */
	struct sassy_remote_process_info rpinfo[MAX_PROCESSES_PER_HOST];
};


#endif /* _SASSY_HB_H_ */