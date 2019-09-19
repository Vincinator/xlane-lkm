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
#include <sassy/payload_helper.h>

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
EXPORT_SYMBOL(sassy_get_protocol_name);

void sassy_post_ts(int sassy_id, uint64_t cycles)
{
	struct sassy_device *sdev = get_sdev(sassy_id);

	if (sdev->ts_state == SASSY_TS_RUNNING)
		sassy_write_timestamp(sdev, 1, cycles, sassy_id);
}
EXPORT_SYMBOL(sassy_post_ts);

void set_all_targets_dead(struct sassy_device *sdev)
{
	struct pminfo *spminfo = &sdev->pminfo;
	int i;

	for(i = 0; i < spminfo->num_of_targets; i++) {
		spminfo->pm_targets[i].alive = 0;
	}
}
EXPORT_SYMBOL(set_all_targets_dead);

struct proto_instance *get_proto_instance(struct sassy_device *sdev, int proto_id)
{
	int idx;

	if (unlikely(proto_id < 0 || proto_id > MAX_PROTO_INSTANCES)) {
		sassy_dbg("Invalid protocol id %d\n", proto_id);
		return NULL;
	}

	idx = sdev->instance_id_mapping[proto_id];

	if (unlikely(idx < 0 || idx > MAX_PROTO_INSTANCES)) {
		sassy_dbg("Invalid protocol idx %d\n", idx);
		return NULL;
	}

	return sdev->protos[idx];
}


void _handle_sub_payloads(struct sassy_device *sdev, unsigned char *remote_mac, void *payload, int instances, u32 bcnt)
{
	int cur_proto_id;
	int cur_offset;
	struct proto_instance *cur_ins;
	/* bcnt <= 0: 
	 *		no payload left to handle
	 *
	 * instances <= 0:
	 *		all included instances were handled
	 */
	if(instances <= 0 || bcnt <= 0){
		return;
	}

	cur_proto_id = GET_PROTO_TYPE_VAL(payload);
	cur_offset = GET_PROTO_OFFSET_VAL(payload);

	cur_ins = get_proto_instance(sdev, cur_proto_id);

	// check if instance for the given protocol id exists
	if(!cur_ins) {
		sassy_dbg("No instance for protocol id %d were found\n", cur_proto_id);
	} else {
		cur_ins->ctrl_ops.post_payload(cur_ins, remote_mac, payload);
		sassy_dbg("Posted Protocol for Instance id: %d  with proto offset %d\n", cur_proto_id, cur_offset);
	}

	// handle next payload
	_handle_sub_payloads(sdev, remote_mac,((char *) payload) + cur_offset, instances -1, bcnt - cur_offset);
}


void sassy_post_payload(int sassy_id, unsigned char *remote_mac, void *payload, u32 cqe_bcnt)
{
	u8 *payload_raw_ptr = (u8 *)payload;
	u8 protocol_id = *payload_raw_ptr;
	struct sassy_device *sdev = get_sdev(sassy_id);
	u8 received_proto_instances;
	
	if (unlikely(!sdev)) {
		sassy_error("sdev is NULL\n");
		return;
	}	
	
    if (unlikely(sdev->pminfo.state != SASSY_PM_EMITTING))
    	return;

    received_proto_instances = GET_PROTO_AMOUNT_VAL(payload);
    sassy_dbg("Received Instances %hhu\n", received_proto_instances);

	_handle_sub_payloads(sdev, remote_mac, GET_PROTO_START_SUBS_PTR(payload), received_proto_instances, cqe_bcnt);


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

	for (i = 0; i < MAX_REMOTE_SOURCES; i++) {
		pmtarget = &sdev->pminfo.pm_targets[i];

		kfree(pmtarget->pkt_data.pkt_payload[0]);
		kfree(pmtarget->pkt_data.pkt_payload[1]);
		kfree(rxt->rhost_buffers[i]);
	}

	sdev->pminfo.num_of_targets = 0;

	sassy_dbg("reset number of targets to 0\n");
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
//	score->sdevices[sassy_id]->proto = NULL;
	score->sdevices[sassy_id]->verbose = 0;
	score->sdevices[sassy_id]->rx_state = SASSY_RX_DISABLED;
	score->sdevices[sassy_id]->ts_state = SASSY_TS_UNINIT;
	
	score->sdevices[sassy_id]->num_of_proto_instances = 0;

	for(i = 0; i < MAX_PROTO_INSTANCES; i ++)
		score->sdevices[sassy_id]->instance_id_mapping[i] = -1;

	score->sdevices[sassy_id]->protos = 
				kmalloc_array(MAX_PROTO_INSTANCES, sizeof(struct proto_instance *), GFP_KERNEL);

	/* set default heartbeat interval */
	//sdev->pminfo.hbi = DEFAULT_HB_INTERVAL;
	score->sdevices[sassy_id]->pminfo.hbi = CYCLES_PER_1MS;

	snprintf(name_buf, sizeof(name_buf), "sassy/%d", ifindex);
	proc_mkdir(name_buf, NULL);

    /* Initialize Timestamping for NIC */
	init_sassy_ts_ctrl_interfaces(score->sdevices[sassy_id]);
	init_timestamping(score->sdevices[sassy_id]);

	/* Initialize logger base for NIC */
	init_log_ctrl_base(score->sdevices[sassy_id]);

	/*  Initialize protocol instance controller */
	init_proto_instance_ctrl(score->sdevices[sassy_id]);

	/* Initialize Control Interfaces for NIC */
	init_sassy_pm_ctrl_interfaces(score->sdevices[sassy_id]);
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

	remove_proto_instance_ctrl(score->sdevices[sassy_id]);


	//remove_logger_ifaces(&score->sdevices[sassy_id]->le_logger);
	clear_protocol_instances(score->sdevices[sassy_id]);

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

int register_protocol_instance(struct sassy_device *sdev, int instance_id, int protocol_id) 
{

	int idx = sdev->num_of_proto_instances; // index starts at 0!
	int ret;

	if (idx > MAX_PROTO_INSTANCES) {
		ret = -EPERM;
		sassy_dbg("Too many instances exist, can not exceed maximum of %d instances\n", MAX_PROTOCOLS);
		sassy_dbg("Current active instances: %d\n", sdev->num_of_proto_instances);

		goto error;
	}

	sdev->protos[idx] = generate_protocol_instance(sdev, protocol_id);

	if(!sdev->protos[idx]) {
		sassy_dbg("Could not allocate memory for new protocol instance!\n");
		ret = -ENOMEM;
		goto error;
	}

	sdev->instance_id_mapping[instance_id] = idx;
	sdev->protos[idx]->instance_id = instance_id;
	sdev->num_of_proto_instances++;

	sdev->protos[idx]->ctrl_ops.init(sdev->protos[idx]);

	return 0;
error:
	sassy_error("Could not register new protocol instance %d\n", ret);
	return ret;
}

void clear_protocol_instances(struct sassy_device *sdev)
{
	int idx, i;

	if (sdev->num_of_proto_instances > MAX_PROTO_INSTANCES) {
		sassy_dbg("num_of_proto_instances is faulty! Aborting cleanup of all instances\n");
		return;
	}

	for(idx = 0; idx < sdev->num_of_proto_instances; idx++) {
		kfree(sdev->protos[idx]->proto_data);
		kfree(sdev->protos[idx]);
	}

	sdev->num_of_proto_instances = 0;

	for(i = 0; i < MAX_PROTO_INSTANCES; i ++)
		sdev->instance_id_mapping[i] = -1;

}


int sassy_core_register_remote_host(int sassy_id, u32 ip, char *mac,
				    int protocol_id, int cluster_id)
{
	struct sassy_rx_table *rxt;
	struct sassy_device *sdev = get_sdev(sassy_id);
	struct sassy_pm_target_info *pmtarget;
	int ifindex;

	if (!mac) {
		sassy_error("input mac is NULL!\n");
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
	pmtarget->alive = 0;
	pmtarget->pkt_data.hb_active_ix = 0;
	pmtarget->pkt_data.naddr.dst_ip = ip;
	pmtarget->pkt_data.naddr.cluster_id = cluster_id;
	pmtarget->pkt_data.protocol_id = protocol_id;

	pmtarget->pkt_data.pkt_payload[0] =
		kzalloc(sizeof(struct sassy_payload), GFP_KERNEL);
		
	pmtarget->pkt_data.pkt_payload[1] =
		kzalloc(sizeof(struct sassy_payload), GFP_KERNEL);

	memcpy(&pmtarget->pkt_data.naddr.dst_mac, mac, sizeof(unsigned char) * 6);

	sdev->pminfo.num_of_targets = sdev->pminfo.num_of_targets + 1;


	return 0;
}
EXPORT_SYMBOL(sassy_core_register_remote_host);

static int __init sassy_connection_core_init(void)
{

	score = kmalloc(sizeof(struct sassy_core), GFP_KERNEL);

	if (!score) {
		sassy_error("allocation of sassy core failed\n");
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

	//init_sassy_proto_info_interfaces();

	return 0;
}


void sassy_stop(int sassy_id)
{
	if (sassy_validate_sassy_device(sassy_id))
		return;

	/* Stop Pacemaker */
	sassy_pm_stop(&score->sdevices[sassy_id]->pminfo);

	/* Stop Timestamping */
	sassy_ts_stop(score->sdevices[sassy_id]);

}

static void __exit sassy_connection_core_exit(void)
{
	int i;

	// Stop running sassy processes
	for(i = 0; i < device_counter; i++)
		sassy_stop(i);
	
	for (i = 0; i < device_counter; i++)
		sassy_core_remove_nic(i);

	// TODO: free all sassy core components!

	kfree(score);

	//clean_sassy_proto_info_interfaces();

}

subsys_initcall(sassy_connection_core_init);
module_exit(sassy_connection_core_exit);

