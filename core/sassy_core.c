#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

#include "sassy_core.h"
#include <sassy/sassy.h>
#include <sassy/logger.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Vincent Riesop");
MODULE_DESCRIPTION("SASSY Connection Core");
MODULE_VERSION("0.01");

#undef LOG_PREFIX
#define LOG_PREFIX "[SASSY][CORE]"

static struct sassy_core *score;

static int device_counter;

struct sassy_device *get_sdev(int devid)
{
	if (unlikely(devid < 0 || devid > MAX_NIC_DEVICES)) {
		sassy_error(" invalid sassy device id\n");
		return NULL;
	}

	if (unlikely(!score)) {
		sassy_error(" sassy core is not initialized\n");
		return NULL;
	}

	return score->sdevices[devid];
}
EXPORT_SYMBOL(get_sdev);


struct sassy_core *sassy_core(void)
{
	return score;
}
EXPORT_SYMBOL(sassy_core);

const char *sassy_get_protocol_name(enum sassy_protocol_type protocol_type)
{
	switch (protocol_type) {
	case SASSY_PROTO_FD:
		return "Failure Detector";
	case SASSY_PROTO_ECHO:
		return "Echo";
	case SASSY_PROTO_CONSENSUS:
		return "Consensus";
	default:
		return "Unknown Protocol!";
	}
}

void sassy_post_ts(int sassy_id, uint64_t cycles)
{
	struct sassy_device *sdev = get_sdev(sassy_id);

	if (unlikely(sdev->rx_state == SASSY_RX_DISABLED))
		return;

	if (sdev->ts_state == SASSY_TS_RUNNING)
		sassy_write_timestamp(sdev, 1, cycles, sassy_id);
}
EXPORT_SYMBOL(sassy_post_ts);

void sassy_post_payload(int sassy_id, unsigned char *remote_mac, void *payload)
{
	u8 *payload_raw_ptr = (u8 *)payload;
	u8 protocol_id = *payload_raw_ptr;
	struct sassy_device *sdev = get_sdev(sassy_id);
	struct sassy_protocol *sproto = NULL;

	if (unlikely(!sdev)) {
		sassy_error("sdev is NULL\n");
		return;
	}

	if (unlikely(sdev->rx_state == SASSY_RX_DISABLED))
		return;

	if (unlikely(protocol_id < 0 || protocol_id > MAX_PROTOCOLS)) {
		sassy_error("Protocol ID is faulty %d\n", protocol_id);
		return;
	}

	sproto = score->protocols[protocol_id];

	if (unlikely(!sproto)) {
		sassy_error("failed to get protocol handler\n");
		return;
	}

    if (sdev->ts_state == SASSY_TS_RUNNING)
		sassy_write_timestamp(sdev, 2, rdtsc(), sassy_id);

	sproto->ctrl_ops.post_payload(sdev, remote_mac, (void *)payload);

	if (sdev->ts_state == SASSY_TS_RUNNING)
		sassy_write_timestamp(sdev, 3, rdtsc(), sassy_id);
}
EXPORT_SYMBOL(sassy_post_payload);

void sassy_reset_remote_host_counter(int sassy_id)
{
	int i;
	struct sassy_rx_table *rxt;
	struct sassy_device *sdev = get_sdev(sassy_id);
	struct sassy_pm_target_info *pmtarget;

	rxt = score->rx_tables[sassy_id];

	if (!rxt)
		return;

	/* Free Memory of all  */
	for (i = 0; i < MAX_REMOTE_SOURCES; i++) {
		pmtarget = &sdev->pminfo.pm_targets[i];

		kfree(pmtarget->pkt_data.pkt_payload[0]);
		kfree(pmtarget->pkt_data.pkt_payload[1]);
		kfree(rxt->rhost_buffers[i]);
	}

	sdev->pminfo.num_of_targets = 0;
	sassy_dbg(" set num_of_targets to 0\n");
}
EXPORT_SYMBOL(sassy_reset_remote_host_counter);

static int sassy_generate_next_id(void)
{
	if (device_counter >= SASSY_MLX5_DEVICES_LIMIT) {
		sassy_error(
			"Reached Limit of maximum connected mlx5 devices.\n");
		sassy_error("Limit=%d, device_counter=%d\n",
			    SASSY_MLX5_DEVICES_LIMIT, device_counter);
		return -1;
	}

	return device_counter++;
}

/* Called by Connection Layer Glue (e.g. mlx5_con.c) */
int sassy_core_register_nic(int ifindex)
{
	char name_buf[MAX_SASSY_PROC_NAME];
	int sassy_id;
	int i;

	sassy_dbg("register nic at sassy core\n");

	sassy_id = sassy_generate_next_id();

	if (sassy_id < 0)
		return -1;

	score->rx_tables[sassy_id] =
		kmalloc(sizeof(struct sassy_rx_table), GFP_KERNEL);
	score->rx_tables[sassy_id]->rhost_buffers =
		kmalloc_array(MAX_REMOTE_SOURCES,
			      sizeof(struct sassy_rx_buffer *),
						GFP_KERNEL);

	/* Allocate each rhost ring buffer*/

	for (i = 0; i < MAX_REMOTE_SOURCES; i++) {
		score->rx_tables[sassy_id]->rhost_buffers[i] =
			kmalloc(sizeof(struct sassy_rx_buffer),
				GFP_KERNEL);
	}

	score->sdevices[sassy_id] =
		kmalloc(sizeof(struct sassy_device), GFP_KERNEL);
	score->sdevices[sassy_id]->ifindex = ifindex;
	score->sdevices[sassy_id]->sassy_id = sassy_id;
	score->sdevices[sassy_id]->ndev = sassy_get_netdevice(ifindex);
	score->sdevices[sassy_id]->pminfo.num_of_targets = 0;
	score->sdevices[sassy_id]->proto = NULL;
	score->sdevices[sassy_id]->verbose = 0;
	score->sdevices[sassy_id]->rx_state = SASSY_RX_DISABLED;

	snprintf(name_buf, sizeof(name_buf), "sassy/%d", ifindex);
	proc_mkdir(name_buf, NULL);

    /* Initialize Timestamping Interfaces for NIC */
	init_sassy_ts_ctrl_interfaces(score->sdevices[sassy_id]);
	init_timestamping(score->sdevices[sassy_id]);

	/* Initialize Control Interfaces for NIC */
	init_sassy_pm_ctrl_interfaces(score->sdevices[sassy_id]);
	init_proto_selector(score->sdevices[sassy_id]);
	init_sassy_ctrl_interfaces(score->sdevices[sassy_id]);

	/* Initialize Component States*/
	pm_state_transition_to(&score->sdevices[sassy_id]->pminfo,
			       SASSY_PM_UNINIT);

	return sassy_id;
}
EXPORT_SYMBOL(sassy_core_register_nic);

static int sassy_core_remove_nic(int sassy_id)
{
	int i;
	char name_buf[MAX_SASSY_PROC_NAME];

	if (sassy_validate_sassy_device(sassy_id))
		return -1;

	/* Remove Ctrl Interfaces for NIC */
	clean_sassy_pm_ctrl_interfaces(score->sdevices[sassy_id]);
	clean_sassy_ctrl_interfaces(score->sdevices[sassy_id]);

	remove_proto_selector(score->sdevices[sassy_id]);

	snprintf(name_buf, sizeof(name_buf), "sassy/%d",
		 score->sdevices[sassy_id]->ifindex);
	proc_mkdir(name_buf, NULL);

	/* Free Memory used for this NIC */
	for (i = 0; i < MAX_PROCESSES_PER_HOST; i++)
		kfree(score->rx_tables[sassy_id]->rhost_buffers[i]);

	kfree(score->rx_tables[sassy_id]);
	kfree(score->sdevices[sassy_id]);
}

int sassy_validate_sassy_device(int sassy_id)
{
	if (!score) {
		sassy_error("score is NULL!\n");
		return -1;
	}
	if (sassy_id < 0 || sassy_id > MAX_NIC_DEVICES) {
		sassy_error("invalid sassy_id! %d\n", sassy_id);
		return -1;
	}
	if (!score->sdevices || !score->sdevices[sassy_id]) {
		sassy_error("sdevices is invalid!\n");
		return -1;
	}

	if (!score->rx_tables || !score->rx_tables[sassy_id]) {
		sassy_error("rx_tables is invalid!\n");
		return -1;
	}
	return 0;
}
EXPORT_SYMBOL(sassy_validate_sassy_device);

int sassy_core_register_remote_host(int sassy_id, u32 ip, char *mac,
				    int protocol_id)
{
	struct sassy_rx_table *rxt;
	struct sassy_device *sdev = get_sdev(sassy_id);
	struct sassy_pm_target_info *pmtarget;
	int ifindex;
	struct sassy_protocol *sproto;

	if (!mac) {
		sassy_error("input mac is NULL!\n");
		return -1;
	}

	/* Check if Protocol ID is supported. */
	if (protocol_id < 0 || protocol_id > MAX_PROTOCOLS) {
		sassy_error("Protocol Number %d is invalid\n", protocol_id);
		return -1;
	}

	sproto = score->protocols[protocol_id];

	if (!sproto) {
		sassy_error(
			"Protocol id %d is valid, but protocol is not initialized. BUG.",
			protocol_id);
		return -1;
	}

	if (sdev->pminfo.num_of_targets >= MAX_REMOTE_SOURCES) {
		sassy_error("Reached Limit of remote hosts.\n");
		sassy_error("Limit is=%d\n", MAX_REMOTE_SOURCES);
		return -1;
	}

	rxt = score->rx_tables[sassy_id];

	if (!rxt) {
		sassy_error("rxt is NULL\n");
		return -1;
	}

	ifindex = sdev->ifindex;
	pmtarget = &sdev->pminfo.pm_targets[sdev->pminfo.num_of_targets];

	if (!pmtarget) {
		sassy_error("pmtarget is NULL\n");
		return -1;
	}

	pmtarget->pkt_data.hb_active_ix = 0;
	pmtarget->pkt_data.dst_ip = ip;
	pmtarget->pkt_data.protocol_id = protocol_id;

	pmtarget->pkt_data.pkt_payload[0] =
		kzalloc(SASSY_PAYLOAD_BYTES, GFP_KERNEL);
	pmtarget->pkt_data.pkt_payload[1] =
		kzalloc(SASSY_PAYLOAD_BYTES, GFP_KERNEL);

	sproto->ctrl_ops.init_payload(pmtarget->pkt_data.pkt_payload[0]);
	sproto->ctrl_ops.init_payload(pmtarget->pkt_data.pkt_payload[1]);

	memcpy(&pmtarget->pkt_data.dst_mac, mac, sizeof(unsigned char) * 6);

	sdev->pminfo.num_of_targets = sdev->pminfo.num_of_targets + 1;

	return 0;
}
EXPORT_SYMBOL(sassy_core_register_remote_host);

static int __init sassy_connection_core_init(void)
{
	sassy_dbg("init\n");

	score = kmalloc(sizeof(struct sassy_core), GFP_KERNEL);

	if (!score) {
		sassy_error("allocation of sassy core failed\n");
		return -1;
	}

	score->protocols = kmalloc_array(
		MAX_PROTOCOLS, sizeof(struct sassy_protocol *), GFP_KERNEL);

	if (!score->protocols) {
		sassy_error("allocation of protocol ptr buffer failed\n");
		return -1;
	}

	score->rx_tables = kmalloc_array(
		MAX_NIC_DEVICES, sizeof(struct sassy_rx_table *), GFP_KERNEL);

	if (!score->rx_tables) {
		sassy_error("allocation of score->rx_tables failed\n");
		return -1;
	}

	score->sdevices = kmalloc_array(
		MAX_NIC_DEVICES, sizeof(struct sassy_device *), GFP_KERNEL);

	if (!score->rx_tables) {
		sassy_error("allocation of score->sdevices failed\n");
		return -1;
	}

	proc_mkdir("sassy", NULL);



	// /proc/sassy/<id>/protocols/<protos>
	init_sassy_proto_info_interfaces();

	sassy_dbg("init done\n");

	return 0;
}

static void __exit sassy_connection_core_exit(void)
{
	int i;

	sassy_dbg("cleanup\n");

	for (i = 0; i < device_counter; i++)
		sassy_core_remove_nic(i);

	// TODO: free all sassy core components!

	kfree(score);

	// /proc/sassy/<id>/protocols/<protos>
	clean_sassy_proto_info_interfaces();

	sassy_dbg("cleanup done\n");
}

subsys_initcall(sassy_connection_core_init);
module_exit(sassy_connection_core_exit);

