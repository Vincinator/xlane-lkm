#include <asgard/logger.h>
#include <asgard/asgard.h>

#include "include/asgard_fd.h"
#include "include/asgard_fdtx.h"


int fd_init(struct proto_instance *ins)
{
	int err = 0;
	struct asgard_fd_priv *priv =
		(struct asgard_fd_priv *)ins->proto_data;

	struct asgard_device *sdev = priv->sdev;

	err = asgard_bypass_init_class(sdev);

	if (err)
		goto error;

	err = asgard_setup_chardev(sdev, priv);

	if (err)
		goto error;

	priv->num_procs = 0;

	asgard_dbg("fd init\n");
	return 0;
error:
	asgard_error("failed to init chardevs for FD protocol\n");
	return err;
}

int fd_init_payload(void *payload)
{
	struct fd_payload *fd_p = (struct fd_payload *)payload;
	int i;

	asgard_dbg("initializing FD payload\n");

	fd_p->protocol_id = ASGARD_PROTO_FD;
	fd_p->message = 0;
	fd_p->alive_rp = 0;

	for (i = 0; i < MAX_PROCESSES_PER_HOST; i++) {
		fd_p->pinfo[i].pid = 0;
		fd_p->pinfo[i].ps = 0;
	}

	return 0;
}

int fd_start(struct proto_instance *ins)
{
	struct asgard_fd_priv *priv =
		(struct asgard_fd_priv *)ins->proto_data;

	asgard_dbg("fd start\n");
	return 0;
}

int fd_stop(struct proto_instance *ins)
{
	struct asgard_fd_priv *priv =
		(struct asgard_fd_priv *)ins->proto_data;
	asgard_dbg("fd stop\n");
	return 0;
}

int fd_clean(struct proto_instance *ins)
{
	struct asgard_fd_priv *priv =
		(struct asgard_fd_priv *)ins->proto_data;
	struct asgard_device *sdev = priv->sdev;

	asgard_clean_class(sdev);
	asgard_dbg("fd clean\n");
	return 0;
}

int fd_info(struct proto_instance *ins)
{
	struct asgard_fd_priv *priv =
		(struct asgard_fd_priv *)ins->proto_data;

	asgard_dbg("fd info\n");
	return 0;
}

/* Check if User Space Applications are still alive.
 * If aliveness counter of US application did not update,
 * then mark corresponding state as down in HB packet of FD protocol
 */
int fd_us_update(struct proto_instance *ins, void *payload)
{
	int i;
	struct asgard_fd_priv *priv =
		(struct asgard_fd_priv *)ins->proto_data;
	struct asgard_device *sdev = priv->sdev;

	struct fd_payload *cur_p = (struct fd_payload *)payload;

	struct fd_aliveness_counters *last_counters =
		&priv->last_counter_values;
	struct pminfo *spminfo;

	struct fd_aliveness_counters *us_counters =
		(struct fd_aliveness_counters *)priv->tx_buf;

	if (sdev->verbose >= 3)
		asgard_dbg("fd us update\n");

	if (!us_counters) {
		asgard_error("aliveness counter buffer is uninitialized.\n");
		return -ENODEV;
	}

	for (i = 0; i < MAX_PROCESSES_PER_HOST; i++) {
		cur_p->pinfo[i].pid = i & 0xFF;
		// Check aliveness counter and set state
		cur_p->pinfo[i].ps =
			(us_counters->ac[i] == last_counters->ac[i]) ? 0 : 1;

		if (sdev->verbose) {
			if (us_counters->ac[i] != last_counters->ac[i])
				asgard_dbg("proc: %d state alive!\n",
					  cur_p->pinfo[i].pid);
		}
		last_counters->ac[i] = us_counters->ac[i];
	}
	return 0;
}

int fd_post_payload(struct proto_instance *ins, int remote_lid, int cluster_id,
		    void *payload)
{
	// .. Test only ..
	//print_hex_dump(KERN_DEBUG, "ASGARD HB: ", DUMP_PREFIX_NONE, 16, 1,
	//                payload, ASGARD_PAYLOAD_BYTES, 0);

	//asgard_dbg("SRC MAC=%pM", remote_mac);

}

int fd_post_ts(struct proto_instance *ins, unsigned char *remote_mac,
	       uint64_t ts)
{

}
