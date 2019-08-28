#include <sassy/logger.h>
#include <sassy/sassy.h>

#include "include/sassy_fd.h"
#include "include/sassy_fdtx.h"


int fd_init(struct sassy_device *sdev)
{
	int err = 0;
	struct sassy_protocol *sproto;
	struct sassy_fd_priv *priv;

	err = sassy_bypass_init_class();

	if (err)
		goto error;

	err = sassy_setup_chardev(sdev);

	if (err)
		goto error;

	sproto = sdev->proto;
	priv = (struct sassy_fd_priv *)sproto->priv;
	priv->num_procs = 0;

	sassy_dbg("fd init\n");
	return 0;
error:
	sassy_error("failed to init chardevs for FD protocol\n");
	return err;
}

int fd_init_payload(struct sassy_payload *payload)
{
	struct fd_payload *fd_p = (struct fd_payload *)payload->other_payload;
	int i;

	sassy_dbg("initializing FD payload\n");

	fd_p->protocol_id = SASSY_PROTO_FD;
	fd_p->message = 0;
	fd_p->alive_rp = 0;

	for (i = 0; i < MAX_PROCESSES_PER_HOST; i++) {
		fd_p->pinfo[i].pid = 0;
		fd_p->pinfo[i].ps = 0;
	}

	return 0;
}

int fd_start(struct sassy_device *sdev)
{
	sassy_dbg("fd start\n");
	return 0;
}

int fd_stop(struct sassy_device *sdev)
{
	sassy_dbg("fd stop\n");
	return 0;
}

int fd_clean(struct sassy_device *sdev)
{
	sassy_clean_class();

	sassy_dbg("fd clean\n");
	return 0;
}

int fd_info(struct sassy_device *sdev)
{
	sassy_dbg("fd info\n");
	return 0;
}

/* Check if User Space Applications are still alive.
 * If aliveness counter of US application did not update,
 * then mark corresponding state as down in HB packet of FD protocol
 */
int fd_us_update(struct sassy_device *sdev, void *payload)
{
	int i;
	struct sassy_protocol *sproto = sdev->proto;
	struct sassy_fd_priv *priv = (struct sassy_fd_priv *)sproto->priv;
	struct sassy_payload *comp_payload = (struct sassy_payload *)payload;
	struct fd_payload *cur_p = (struct fd_payload *)comp_payload->other_payload;

	struct fd_aliveness_counters *last_counters =
		&priv->last_counter_values;
	struct pminfo *spminfo;

	struct fd_aliveness_counters *us_counters =
		(struct fd_aliveness_counters *)priv->tx_buf;

	if (sdev->verbose >= 3)
		sassy_dbg("fd us update\n");

	if (!us_counters) {
		sassy_error("aliveness counter buffer is uninitialized.\n");
		return -ENODEV;
	}

	for (i = 0; i < MAX_PROCESSES_PER_HOST; i++) {
		cur_p->pinfo[i].pid = i & 0xFF;
		// Check aliveness counter and set state
		cur_p->pinfo[i].ps =
			(us_counters->ac[i] == last_counters->ac[i]) ? 0 : 1;

		if (sdev->verbose) {
			if (us_counters->ac[i] != last_counters->ac[i])
				sassy_dbg("proc: %d state alive!\n",
					  cur_p->pinfo[i].pid);
		}
		last_counters->ac[i] = us_counters->ac[i];
	}
	return 0;
}

int fd_post_payload(struct sassy_device *sdev, unsigned char *remote_mac,
		    void *payload)
{
	// .. Test only ..
	//print_hex_dump(KERN_DEBUG, "SASSY HB: ", DUMP_PREFIX_NONE, 16, 1,
	//                payload, SASSY_PAYLOAD_BYTES, 0);

	//sassy_dbg("SRC MAC=%pM", remote_mac);

	if (sdev->verbose >= 3)
		sassy_dbg("fd payload received\n");
}

int fd_post_ts(struct sassy_device *sdev, unsigned char *remote_mac,
	       uint64_t ts)
{
	if (sdev->verbose >= 3) {
		//sassy_dbg("SRC MAC=%pM", remote_mac);
		sassy_dbg("fd optimistical timestamp received.\n");
	}
}
