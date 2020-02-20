#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/mutex.h>

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

#include <linux/workqueue.h>

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

static struct workqueue_struct *asguard_wq;


//  sdev, remote_lid, cluster_id, payload + cur_offset, instances - 1, bcnt - cur_offset

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

	/* Protect this kernel from buggy packets.
	 * In the current state, more than 4 instances are not intentional.
	 */
	if (instances > 4) {
	    asguard_error("Received packet that claimed to include %d instances.\n", instances);
	    return;
	}

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
			asguard_dbg("No instance for protocol id %d were found. instances=%d", cur_proto_id, instances);
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

		// Do not start Leader Election until all targets have send a message to this node
		for (i = 0; i < spminfo->num_of_targets; i++)
			if (!spminfo->pm_targets[i].alive)
				return 1;

		// Starting all protocols
		for (i = 0; i < sdev->num_of_proto_instances; i++) {
			if (sdev->protos != NULL && sdev->protos[i] != NULL && sdev->protos[i]->ctrl_ops.start != NULL) {
				sdev->protos[i]->ctrl_ops.start(sdev->protos[i]);
			} else {
				asguard_dbg("protocol instance %d not initialized", i);
			}
		}
		asguard_dbg("Warmup done!\n");
		sdev->warmup_state = WARMED_UP;
		update_leader(sdev, &sdev->pminfo);
	}
	return 0;
}


// Note: this function will not explicitly run on the same isolated cpu
//		.. for consecutive packets (even from the same host)
//   ... Timestamping may be
void pkt_process_handler(struct work_struct *w) {

	struct asguard_pkt_work_data *aw = NULL;

	aw = container_of(w, struct asguard_pkt_work_data, work);

	_handle_sub_payloads(aw->sdev, aw->remote_lid, aw->rcluster_id, GET_PROTO_START_SUBS_PTR(aw->payload),
		aw->received_proto_instances, aw->cqe_bcnt);

	if(aw)
		kfree(aw);
}

#define ASG_ETH_HEADER_SIZE 14

void asguard_post_payload(int asguard_id, void *payload, u16 headroom, u32 cqe_bcnt)
{
	struct asguard_device *sdev = get_sdev(asguard_id);
	struct pminfo *spminfo = &sdev->pminfo;
	int remote_lid, rcluster_id;
	u16 received_proto_instances;
	struct asguard_pkt_work_data *work;
	uint64_t ts2, ts3;
    return;
	char *remote_mac = ((char *) payload) + headroom + 6;

    print_hex_dump(KERN_DEBUG, "ASGUARD pkt: ", DUMP_PREFIX_NONE, 16, 1,
                    payload, cqe_bcnt, 0);

    asguard_dbg("cqe_bcnt=%u, SRC MAC=%pM", cqe_bcnt, remote_mac);



	ts2 = RDTSC_ASGUARD;


	if (unlikely(!sdev)) {
		asguard_error("sdev is NULL\n");
		return;
	}

	if (unlikely(sdev->pminfo.state != ASGUARD_PM_EMITTING))
		return;

	get_cluster_ids(sdev, remote_mac, &remote_lid, &rcluster_id);

	if (unlikely(remote_lid == -1)){
		asguard_dbg("Invalid ids! \n");
		return;
	}

	// Update aliveness state and timestamps
	spminfo->pm_targets[remote_lid].chb_ts = RDTSC_ASGUARD;
	spminfo->pm_targets[remote_lid].alive = 1;

	if(check_warmup_state(sdev, spminfo)){
		return;
	}

	spminfo->pm_targets[remote_lid].pkt_rx_counter++;

	received_proto_instances = GET_PROTO_AMOUNT_VAL(payload);

    // freed by pkt_process_handler
    work = kmalloc(sizeof(struct asguard_pkt_work_data), GFP_ATOMIC);

	work->cqe_bcnt = cqe_bcnt;
	work->payload = payload;
	work->rcluster_id = rcluster_id;
	work->remote_lid = remote_lid;
	work->received_proto_instances = received_proto_instances;
	work->sdev = sdev;

	INIT_WORK(&work->work, pkt_process_handler);
	if(!queue_work(asguard_wq, &work->work)) {
		asguard_dbg("Work item not put in query..");
	}

	ts3 = RDTSC_ASGUARD;

	if (sdev->ts_state == ASGUARD_TS_RUNNING){
		asguard_write_timestamp(sdev, 2, ts2, rcluster_id);
		asguard_write_timestamp(sdev, 3, ts3, rcluster_id);
	}

}
EXPORT_SYMBOL(asguard_post_payload);

void asguard_reset_remote_host_counter(int asguard_id)
{
	int i;
	struct asguard_device *sdev = get_sdev(asguard_id);
	struct asguard_pm_target_info *pmtarget;

	for (i = 0; i < MAX_REMOTE_SOURCES; i++) {
		pmtarget = &sdev->pminfo.pm_targets[i];
		kfree(pmtarget->pkt_data.hb_pkt_payload);
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
    // freed by asguard_connection_core_exit
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
	score->sdevices[asguard_id]->hold_fire = 0;
	score->sdevices[asguard_id]->tx_port = 3319;
	score->sdevices[asguard_id]->cur_leader_lid = -1;
	score->sdevices[asguard_id]->pkt_proc_sts = 0;
	score->sdevices[asguard_id]->pkt_proc_ets = 0;
	score->sdevices[asguard_id]->pkt_proc_ctr = 0;
	score->sdevices[asguard_id]->is_leader = 0;

	score->sdevices[asguard_id]->asguard_leader_wq =
		 alloc_workqueue("asguard_leader", WQ_HIGHPRI | WQ_CPU_INTENSIVE | WQ_UNBOUND, 0);

	for (i = 0; i < MAX_PROTO_INSTANCES; i++)
		score->sdevices[asguard_id]->instance_id_mapping[i] = -1;

    // freed by clear_protocol_instances
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

	init_asguard_async_list_of_queues(&(score->sdevices[asguard_id]->pminfo.async_priv));


	return asguard_id;
}
EXPORT_SYMBOL(asguard_core_register_nic);


int asguard_core_remove_nic(int asguard_id)
{
	char name_buf[MAX_ASGUARD_PROC_NAME];

	if (asguard_validate_asguard_device(asguard_id))
		return -1;

    clean_asguard_async_list_of_queues(score->sdevices[asguard_id]->pminfo.async_priv);

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
		return;
	}

	for (i = 0; i < sdev->num_of_proto_instances; i++) {


		if (!sdev->protos[i])
			continue;


		if (sdev->protos[i]->ctrl_ops.clean != NULL) {
			sdev->protos[i]->ctrl_ops.clean(sdev->protos[i]);

		}

		// timer are not finished yet!?
		if(sdev->protos[i]->proto_data)
		    kfree(sdev->protos[i]->proto_data);

		if(!sdev->protos[i])
		    kfree(sdev->protos[i]); // is non-NULL, see continue condition above

	}

	for (i = 0; i < MAX_PROTO_INSTANCES; i++)
		sdev->instance_id_mapping[i] = -1;

	sdev->num_of_proto_instances = 0;

}


int asguard_core_register_remote_host(int asguard_id, u32 ip, char *mac,
					int protocol_id, int cluster_id)
{
	struct asguard_device *sdev = get_sdev(asguard_id);
	struct asguard_pm_target_info *pmtarget;
	int ifindex, ret;

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
	pmtarget->pkt_data.active_dirty = 0;
	mutex_init(&pmtarget->pkt_data.active_dirty_lock);
	pmtarget->pkt_data.naddr.dst_ip = ip;
	pmtarget->pkt_data.naddr.cluster_id = cluster_id;
	pmtarget->pkt_data.protocol_id = protocol_id;
	pmtarget->lhb_ts = 0;
	pmtarget->chb_ts = 0;
	pmtarget->resp_factor = 4;
	pmtarget->cur_waiting_interval = 2;
	pmtarget->pkt_tx_counter = 0;
	pmtarget->pkt_rx_counter = 0;

	pmtarget->scheduled_log_replications = 0;
	pmtarget->received_log_replications = 0;

	pmtarget->pkt_data.contains_log_rep[0] = 0;
	pmtarget->pkt_data.contains_log_rep[1] = 0;

	pmtarget->pkt_data.pkt_payload[0] =
		kzalloc(sizeof(struct asguard_payload), GFP_KERNEL);

	pmtarget->pkt_data.pkt_payload[1] =
		kzalloc(sizeof(struct asguard_payload), GFP_KERNEL);

	pmtarget->pkt_data.hb_pkt_payload =
		kzalloc(sizeof(struct asguard_payload), GFP_KERNEL);

	memcpy(&pmtarget->pkt_data.naddr.dst_mac, mac, sizeof(unsigned char) * 6);

	sdev->pminfo.num_of_targets = sdev->pminfo.num_of_targets + 1;

	ret = init_asguard_async_queue(sdev->pminfo.async_priv, &(pmtarget->aapriv));

	return ret;
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

    // freed by asguard_connection_core_exit
    score = kmalloc(sizeof(struct asguard_core), GFP_KERNEL);

	if (!score) {
		asguard_error("allocation of asguard core failed\n");
		return -1;
	}

	score->num_devices = 0;

    // freed by asguard_connection_core_exit
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

	/* Allocate Workqueues */
	asguard_wq = alloc_workqueue("asguard",  WQ_HIGHPRI | WQ_CPU_INTENSIVE | WQ_UNBOUND, 0);


	return 0;
error:
	asguard_error("Could not initialize asguard - aborting init.\n");
	return err;

}

void asguard_stop_pacemaker(struct asguard_device *adev)
{

	if(!adev) {
		asguard_error("asguard device is NULL\n");
		return;
	}

	adev->pminfo.state = ASGUARD_PM_READY;
}

void asguard_stop_timestamping(struct asguard_device *adev)
{

	if(!adev) {
		asguard_error("ASGuard Device is Null.\n");
		return;
	}

	asguard_ts_stop(adev);


}

void asguard_stop(int asguard_id)
{

	if (asguard_validate_asguard_device(asguard_id)){
		asguard_dbg("invalid asguard id %d", asguard_id);
		return;
	}

	asguard_stop_timestamping(score->sdevices[asguard_id]);

	asguard_stop_pacemaker(score->sdevices[asguard_id]);

}

static void __exit asguard_connection_core_exit(void)
{
	int i;

	destroy_workqueue(asguard_wq);

	// MUST unregister asguard for drivers first
	unregister_asguard();

	if(!score){
		asguard_error("score is NULL \n");
		return;
	}

	for(i = 0; i < MAX_NIC_DEVICES; i++) {

		if(!score->sdevices[i]) {
			continue;
		}

		asguard_stop(i);

		destroy_workqueue(score->sdevices[i]->asguard_leader_wq);

		clear_protocol_instances(score->sdevices[i]);

		asguard_core_remove_nic(i);

		kfree(score->sdevices[i]);
	}

	if(score->sdevices)
        kfree(score->sdevices);

	if(score)
	    kfree(score);

	remove_proc_entry("asguard", NULL);

	// flush_workqueue(asguard_wq);

}

module_init(asguard_connection_core_init);
module_exit(asguard_connection_core_exit);
