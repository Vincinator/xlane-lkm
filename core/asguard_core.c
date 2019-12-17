#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

#include "asguard_core.h"
#include <asguard/asguard.h>
#include <asguard_con/asguard_con.h>

#include <asguard/logger.h>
#include <asguard/payload_helper.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Distributed Systems Group");
MODULE_DESCRIPTION("ASGUARD Connection Core");
MODULE_VERSION("0.01");

#undef LOG_PREFIX
#define LOG_PREFIX "[ASGUARD][CORE]"

static int ifindex = -1;
module_param(ifindex, int, 0660);


static struct asguard_core *score;


struct asguard_device *get_sdev(int devid)
{
	if (unlikely(devid < 0 || devid > MAX_NIC_DEVICES)) {
		asguard_error(" invalid asguard device id\n");

		return NULL;
	}

	if (unlikely(!score)) {
		asguard_error(" asguard core is not initialized\n");
		return NULL;
	}

	return score->sdevices[devid];
}
EXPORT_SYMBOL(get_sdev);


struct asguard_core *asguard_core(void)
{
	return score;
}
EXPORT_SYMBOL(asguard_core);

const char *asguard_get_protocol_name(enum asguard_protocol_type protocol_type)
{
	switch (protocol_type) {
	case ASGUARD_PROTO_FD:
		return "Failure Detector";
	case ASGUARD_PROTO_ECHO:
		return "Echo";
	case ASGUARD_PROTO_CONSENSUS:
		return "Consensus";
	default:
		return "Unknown Protocol!";
	}
}
EXPORT_SYMBOL(asguard_get_protocol_name);

void asguard_post_ts(int asguard_id, uint64_t cycles, int ctype)
{
	struct asguard_device *sdev = get_sdev(asguard_id);

	if (sdev->ts_state == ASGUARD_TS_RUNNING)
		asguard_write_timestamp(sdev, 1, cycles, asguard_id);

	if(ctype == 2) { // channel type 2 is leader channel
		sdev->last_leader_ts = cycles;
		if(sdev->cur_leader_lid != -1) {
			sdev->pminfo.pm_targets[sdev->cur_leader_lid].chb_ts = cycles;
			sdev->pminfo.pm_targets[sdev->cur_leader_lid].alive = 1;
		}

	}
}
EXPORT_SYMBOL(asguard_post_ts);

void set_all_targets_dead(struct asguard_device *sdev)
{
	struct pminfo *spminfo = &sdev->pminfo;
	int i;

	for (i = 0; i < spminfo->num_of_targets; i++)
		spminfo->pm_targets[i].alive = 0;

}
EXPORT_SYMBOL(set_all_targets_dead);

struct proto_instance *get_proto_instance(struct asguard_device *sdev, u16 proto_id)
{
	int idx;

	if (unlikely(proto_id < 0 || proto_id > MAX_PROTO_INSTANCES)){
		asguard_error("proto_id is invalid: %hu\n", proto_id);
		return NULL;
	}

	idx = sdev->instance_id_mapping[proto_id];

	if (unlikely(idx < 0 || idx >= MAX_PROTO_INSTANCES)){
		asguard_error("idx is invalid: %d\n", idx);

		return NULL;
	}

	return sdev->protos[idx];
}


void _handle_sub_payloads(struct asguard_device *sdev, int remote_lid, int cluster_id, char *payload, int instances, u32 bcnt)
{
	u16 cur_proto_id;
	u16 cur_offset;
	struct proto_instance *cur_ins;


	/* bcnt <= 0:
	 *		no payload left to handle
	 *
	 * instances <= 0:
	 *		all included instances were handled
	 */
	if (instances <= 0 || bcnt <= 0)
		return;

	// if (sdev->verbose >= 3)
	//	asguard_dbg("recursion. instances %d bcnt %d", instances, bcnt);

	cur_proto_id = GET_PROTO_TYPE_VAL(payload);

	// if (sdev->verbose >= 3)
	//	asguard_dbg("cur_proto_id %d", cur_proto_id);

	cur_offset = GET_PROTO_OFFSET_VAL(payload);

	// if (sdev->verbose >= 3)
	//	asguard_dbg("cur_offset %d", cur_offset);

	cur_ins = get_proto_instance(sdev, cur_proto_id);

	// check if instance for the given protocol id exists
	if (!cur_ins) {
		if (sdev->verbose >= 3)
			asguard_dbg("No instance for protocol id %d were found. Sender MAC: %pM\n, instances=%d", cur_proto_id, remote_mac, instances);
	} else {
		cur_ins->ctrl_ops.post_payload(cur_ins, remote_lid, cluster_id, payload);
	}

	// handle next payload
	_handle_sub_payloads(sdev, remote_lid, cluster_id, payload + cur_offset, instances - 1, bcnt - cur_offset);
}


int check_warmup_state(struct asguard_device *sdev, struct pminfo *spminfo)
{
	int i;

	if (unlikely(sdev->warmup_state == WARMING_UP)) {

		// Do not start Leader Election until all targets have send a message to this node.
		for (i = 0; i < spminfo->num_of_targets; i++)
			if (!spminfo->pm_targets[i].alive)
				return 1;

		// Starting all protocols
		for (i = 0; i < sdev->num_of_proto_instances; i++) {
			if (sdev->protos != NULL && sdev->protos[i] != NULL && sdev->protos[i]->ctrl_ops.start != NULL) {
				asguard_dbg("starting instance %d", i);
				sdev->protos[i]->ctrl_ops.start(sdev->protos[i]);
			} else {
				asguard_dbg("protocol instance %d not initialized", i);
			}
		}
		asguard_dbg("Warmup done!\n");
		sdev->warmup_state = WARMED_UP;
	}
	return 0;
}



static int do_process_pkt(void *data)
{
	u16 received_proto_instances;
	unsigned long flags;
	struct process_pkt_in *ppi = (struct process_pkt_in *) data;

	/* start of critical section - locking per remote target*/
	spin_lock_irqsave(ppi->target_lock, flags);

// Do payload handling work here ...

	/* end of critical section - locking per remote target */
	spin_unlock_irqrestore(ppi->target_lock, flags);

	return 0;
}



void asguard_process_pkt_payload(struct asguard_device *sdev, unsigned char *remote_mac, void *payload, u32 cqe_bcnt, int remote_lid)
{
	static struct task_struct *process_pkt_task;
	struct process_pkt_in *ppi;

	ppi = kmalloc(sizeof(struct process_pkt_in), GFP_KERNEL);

	if(!ppi){
		asguard_error("Not enough memory to process pkt\n");
		return;
	}

	ppi->sdev = sdev;
	ppi->remote_mac = remote_mac;
	ppi->payload = payload;
	ppi->cqe_bcnt = cqe_bcnt;
	ppi->remote_lid = remote_lid;

	/* Lock per remote target - processing of packets from multiple targets in parallel is allowed */
	ppi->target_lock = &sdev->pminfo.pm_targets[remote_lid].rx_process_lock;

	process_pkt_task = kthread_create(&do_process_pkt, ppi,
			"asguard process pkt");

	/* TODO: parameterize the cpu that should execute the process_pkt task */
	kthread_bind(process_pkt_task, 42);

	if (IS_ERR(process_pkt_task)) {
		asguard_error("asguard process pkt task Error. %s\n", __func__);
		return -EINVAL;
	}

	wake_up_process(process_pkt_task);

}



void asguard_post_payload(int asguard_id, unsigned char *remote_mac, void *payload, u32 cqe_bcnt)
{
	struct asguard_device *sdev = get_sdev(asguard_id);
	struct pminfo *spminfo = &sdev->pminfo;
	int remote_lid, rcluster_id;
	u16 received_proto_instances;

	if (unlikely(!sdev)) {
		asguard_error("sdev is NULL\n");
		return;
	}

	//asguard_dbg("Payload size: %d, state: %d %s %i", cqe_bcnt, sdev->pminfo.state, __func__, __LINE__);

	if (unlikely(sdev->pminfo.state != ASGUARD_PM_EMITTING))
		return;

	get_cluster_ids(sdev, remote_mac, &remote_lid, &rcluster_id);

	if (unlikely(remote_lid == -1 || rcluster_id == -1 || remote_lid > spminfo->num_of_targets)){
		asguard_dbg("Invalid ids! \n");
		return;
	}

	// Update aliveness state and timestamps
	spminfo->pm_targets[remote_lid].chb_ts = RDTSC_ASGUARD;
	spminfo->pm_targets[remote_lid].alive = 1;

	if(check_warmup_state(sdev, spminfo))
		return;


	received_proto_instances = GET_PROTO_AMOUNT_VAL(payload);

	_handle_sub_payloads(sdev, remote_lid, cluster_id, GET_PROTO_START_SUBS_PTR(payload),
		received_proto_instances, cqe_bcnt);


	// asguard_process_pkt_payload(sdev,remote_mac, payload, cqe_bcnt, remote_lid);

}
EXPORT_SYMBOL(asguard_post_payload);

void asguard_reset_remote_host_counter(int asguard_id)
{
	int i;
	struct asguard_device *sdev = get_sdev(asguard_id);
	struct asguard_pm_target_info *pmtarget;

	for (i = 0; i < MAX_REMOTE_SOURCES; i++) {
		pmtarget = &sdev->pminfo.pm_targets[i];

		kfree(pmtarget->pkt_data.pkt_payload[0]);
		kfree(pmtarget->pkt_data.pkt_payload[1]);
	}

	sdev->pminfo.num_of_targets = 0;

	asguard_dbg("reset number of targets to 0\n");
}
EXPORT_SYMBOL(asguard_reset_remote_host_counter);


/* Called by Connection Layer Glue (e.g. mlx5_con.c) */
int asguard_core_register_nic(int ifindex,  int asguard_id)
{
	char name_buf[MAX_ASGUARD_PROC_NAME];
	int i;

	if (asguard_id < 0 || ifindex < 0) {
		asguard_error("Invalid parameter. asguard_id=%d, ifindex=%d", asguard_id, ifindex);
		return -EINVAL;
	}

	asguard_dbg("register nic at asguard core. ifindex=%d, asguard_id=%d\n", ifindex, asguard_id);

	score->sdevices[asguard_id] =
		kmalloc(sizeof(struct asguard_device), GFP_KERNEL);

	score->num_devices++;
	score->sdevices[asguard_id]->ifindex = ifindex;
	score->sdevices[asguard_id]->asguard_id = asguard_id;
	score->sdevices[asguard_id]->ndev = asguard_get_netdevice(ifindex);
	score->sdevices[asguard_id]->pminfo.num_of_targets = 0;
//	score->sdevices[asguard_id]->proto = NULL;
	score->sdevices[asguard_id]->verbose = 0;
	score->sdevices[asguard_id]->rx_state = ASGUARD_RX_DISABLED;
	score->sdevices[asguard_id]->ts_state = ASGUARD_TS_UNINIT;
	score->sdevices[asguard_id]->last_leader_ts = 0;
	score->sdevices[asguard_id]->num_of_proto_instances = 0;
	score->sdevices[asguard_id]->fire = 0;
	score->sdevices[asguard_id]->tx_port = 3319;
	score->sdevices[asguard_id]->cur_leader_lid = -1;


	for (i = 0; i < MAX_PROTO_INSTANCES; i++)
		score->sdevices[asguard_id]->instance_id_mapping[i] = -1;

	score->sdevices[asguard_id]->protos =
				kmalloc_array(MAX_PROTO_INSTANCES, sizeof(struct proto_instance *), GFP_KERNEL);

	if (!score->sdevices[asguard_id]->protos)
		asguard_error("ERROR! Not enough memory for protocols\n");

	/* set default heartbeat interval */
	//sdev->pminfo.hbi = DEFAULT_HB_INTERVAL;
	score->sdevices[asguard_id]->pminfo.hbi = CYCLES_PER_1MS;

	snprintf(name_buf, sizeof(name_buf), "asguard/%d", ifindex);
	proc_mkdir(name_buf, NULL);

	/* Initialize Timestamping for NIC */
	init_asguard_ts_ctrl_interfaces(score->sdevices[asguard_id]);
	init_timestamping(score->sdevices[asguard_id]);

	/* Initialize logger base for NIC */
	//init_log_ctrl_base(score->sdevices[asguard_id]);

	/*  Initialize protocol instance controller */
	init_proto_instance_ctrl(score->sdevices[asguard_id]);

	/* Initialize Control Interfaces for NIC */
	init_asguard_pm_ctrl_interfaces(score->sdevices[asguard_id]);
	init_asguard_ctrl_interfaces(score->sdevices[asguard_id]);

	/* Initialize Component States*/
	pm_state_transition_to(&score->sdevices[asguard_id]->pminfo,
				   ASGUARD_PM_UNINIT);

	return asguard_id;
}
EXPORT_SYMBOL(asguard_core_register_nic);


int asguard_core_remove_nic(int asguard_id)
{
	int i;
	char name_buf[MAX_ASGUARD_PROC_NAME];

	if (asguard_validate_asguard_device(asguard_id))
		return -1;

	asguard_clean_timestamping(score->sdevices[asguard_id]);

	/* Remove Ctrl Interfaces for NIC */
	clean_asguard_pm_ctrl_interfaces(score->sdevices[asguard_id]);
	clean_asguard_ctrl_interfaces(score->sdevices[asguard_id]);


	remove_proto_instance_ctrl(score->sdevices[asguard_id]);


	//remove_logger_ifaces(&score->sdevices[asguard_id]->le_logger);

	snprintf(name_buf, sizeof(name_buf), "asguard/%d",
		 score->sdevices[asguard_id]->ifindex);

	remove_proc_entry(name_buf, NULL);


	return 0;
}
EXPORT_SYMBOL(asguard_core_remove_nic);

int asguard_validate_asguard_device(int asguard_id)
{
	if (!score) {
		asguard_error("score is NULL!\n");
		return -1;
	}
	if (asguard_id < 0 || asguard_id > MAX_NIC_DEVICES) {
		asguard_error("invalid asguard_id! %d\n", asguard_id);
		return -1;
	}
	if (!score->sdevices || !score->sdevices[asguard_id]) {
		asguard_error("sdevices is invalid!\n");
		return -1;
	}

	return 0;
}
EXPORT_SYMBOL(asguard_validate_asguard_device);

int register_protocol_instance(struct asguard_device *sdev, int instance_id, int protocol_id)
{

	int idx = sdev->num_of_proto_instances;
	int ret;


	if (idx > MAX_PROTO_INSTANCES) {
		ret = -EPERM;
		asguard_dbg("Too many instances exist, can not exceed maximum of %d instances\n", MAX_PROTOCOLS);
		asguard_dbg("Current active instances: %d\n", sdev->num_of_proto_instances);

		goto error;
	}

	sdev->protos[idx] = generate_protocol_instance(sdev, protocol_id);

	if (!sdev->protos[idx]) {
		asguard_dbg("Could not allocate memory for new protocol instance!\n");
		ret = -ENOMEM;
		goto error;
	}

	sdev->instance_id_mapping[instance_id] = idx;

	sdev->protos[idx]->instance_id = instance_id;

	sdev->num_of_proto_instances++;

	sdev->protos[idx]->ctrl_ops.init(sdev->protos[idx]);

	return 0;
error:
	asguard_error("Could not register new protocol instance %d\n", ret);
	return ret;
}

void clear_protocol_instances(struct asguard_device *sdev)
{
	int i;

	if (!sdev) {
		asguard_error("SDEV is NULL - can not clear instances.\n");
		return;
	}

	if (sdev->num_of_proto_instances > MAX_PROTO_INSTANCES) {
		asguard_dbg("num_of_proto_instances is faulty! Aborting cleanup of all instances\n");
		return;
	}

	// If pacemaker is running, do not clear the protocols!
	if (sdev->pminfo.state == ASGUARD_PM_EMITTING) {
		asguard_error("PM is running!\n");
		return;
	}

	for (i = 0; i < sdev->num_of_proto_instances; i++) {

		asguard_dbg("Cleaning proto with id=%d\n", i);

		if (!sdev->protos[i])
			continue;

		asguard_dbg("protocol instance exists\n");

		if (sdev->protos[i]->ctrl_ops.clean != NULL) {
			asguard_dbg(" Call clean function of protocol\n");
			sdev->protos[i]->ctrl_ops.clean(sdev->protos[i]);
			asguard_dbg(" Clean of Protocol done\n");

		}

		// timer are not finished yet!?
		kfree(sdev->protos[i]->proto_data);


		kfree(sdev->protos[i]);

	}
	asguard_dbg("done clean. num of proto instances: %d\n", sdev->num_of_proto_instances);

	for (i = 0; i < MAX_PROTO_INSTANCES; i++)
		sdev->instance_id_mapping[i] = -1;

	sdev->num_of_proto_instances = 0;

}


int asguard_core_register_remote_host(int asguard_id, u32 ip, char *mac,
					int protocol_id, int cluster_id)
{
	struct asguard_device *sdev = get_sdev(asguard_id);
	struct asguard_pm_target_info *pmtarget;
	int ifindex;

	if (!mac) {
		asguard_error("input mac is NULL!\n");
		return -1;
	}

	if (sdev->pminfo.num_of_targets >= MAX_REMOTE_SOURCES) {
		asguard_error("Reached Limit of remote hosts.\n");
		asguard_error("Limit is=%d\n", MAX_REMOTE_SOURCES);
		return -1;
	}

	ifindex = sdev->ifindex;
	pmtarget = &sdev->pminfo.pm_targets[sdev->pminfo.num_of_targets];

	if (!pmtarget) {
		asguard_error("pmtarget is NULL\n");
		return -1;
	}
	pmtarget->alive = 0;
	pmtarget->pkt_data.hb_active_ix = 0;
	pmtarget->pkt_data.naddr.dst_ip = ip;
	pmtarget->pkt_data.naddr.cluster_id = cluster_id;
	pmtarget->pkt_data.protocol_id = protocol_id;
	pmtarget->lhb_ts = 0;
	pmtarget->chb_ts = 0;
	pmtarget->resp_factor = 4;
	pmtarget->cur_waiting_interval = 2;

	pmtarget->pkt_data.pkt_payload[0] =
		kzalloc(sizeof(struct asguard_payload), GFP_KERNEL);

	pmtarget->pkt_data.pkt_payload[1] =
		kzalloc(sizeof(struct asguard_payload), GFP_KERNEL);

	memcpy(&pmtarget->pkt_data.naddr.dst_mac, mac, sizeof(unsigned char) * 6);

	sdev->pminfo.num_of_targets = sdev->pminfo.num_of_targets + 1;


	return 0;
}
EXPORT_SYMBOL(asguard_core_register_remote_host);


static int __init asguard_connection_core_init(void)
{
	int i;
	int err = -EINVAL;

	if (ifindex < 0) {
		asguard_error("ifindex parameter is missing\n");
		goto error;
	}

	err = register_asguard_at_nic(ifindex, asguard_post_ts, asguard_post_payload);

	if (err)
		goto error;

	score = kmalloc(sizeof(struct asguard_core), GFP_KERNEL);

	if (!score) {
		asguard_error("allocation of asguard core failed\n");
		return -1;
	}

	score->num_devices = 0;

	score->sdevices = kmalloc_array(
		MAX_NIC_DEVICES, sizeof(struct asguard_device *), GFP_KERNEL);

	if (!score->sdevices) {
		asguard_error("allocation of score->sdevices failed\n");
		return -1;
	}

	for(i = 0; i < MAX_NIC_DEVICES; i++)
		score->sdevices[i] = NULL;

	proc_mkdir("asguard", NULL);

	asguard_core_register_nic(ifindex, get_asguard_id_by_ifindex(ifindex));

	//init_asguard_proto_info_interfaces();

	return 0;
error:
	asguard_error("Could not initialize asguard - aborting init.\n");
	return err;

}

void asguard_stop_pacemaker(struct asguard_device *adev)
{
	asguard_dbg("Enter Stop pacemaker..\n");

	if(!adev) {
		asguard_error("asguard device is NULL\n");
		return;
	}
	asguard_dbg("setting pacemaker state..\n");

	adev->pminfo.state = ASGUARD_PM_READY;

	asguard_dbg("pacemaker state set..\n");
}

void asguard_stop_timestamping(struct asguard_device *adev)
{
	asguard_dbg("Enter Stop timestamping ..\n");

	if(!adev) {
		asguard_error("ASGuard Device is Null.\n");
		return;
	}
	asguard_ts_stop(adev);

	asguard_dbg("ts stopped..\n");

}

void asguard_stop(int asguard_id)
{
	asguard_dbg("Enter Stop asguard fun..\n");

	if (asguard_validate_asguard_device(asguard_id)){
		asguard_dbg("invalid asguard id %d", asguard_id);
		return;
	}

	asguard_stop_timestamping(score->sdevices[asguard_id]);

	asguard_stop_pacemaker(score->sdevices[asguard_id]);

	asguard_dbg("Stopped asguard..\n");

}

static void __exit asguard_connection_core_exit(void)
{
	int i;

	// MUST unregister asguard for drivers first
	unregister_asguard();

	if(!score){
		asguard_error("score is NULL \n");
		return;
	}

	for(i = 0; i < MAX_NIC_DEVICES; i++) {

		if(!score->sdevices[i]) {
			asguard_dbg("Skipping uninitialized device asguard_id=%d", i);
			continue;
		}

		asguard_dbg("Clean up device with asguard_id=%d", i);

		asguard_stop(i);

		clear_protocol_instances(score->sdevices[i]);

		asguard_core_remove_nic(i);

		kfree(score->sdevices[i]);
	}

	kfree(score);

	remove_proc_entry("asguard", NULL);

	asguard_dbg("Unloaded Module..", i);
}

module_init(asguard_connection_core_init);
module_exit(asguard_connection_core_exit);
