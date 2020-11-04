#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <net/ip.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>

#include <asm/checksum.h>
#include <linux/skbuff.h>
#include <linux/notifier.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/slab.h>
#include <linux/inetdevice.h>

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

#include <linux/workqueue.h>

#include "asgard_core.h"
#include <asgard/asgard.h>
#include <asgard_con/asgard_con.h>


#include <asgard/logger.h>
#include <asgard/payload_helper.h>
#include <synbuf-chardev.h>
#include <asgard/asgard_uface.h>
#include <synbuf-chardev.h>
#include <asgard/asgard_echo.h>
#include <asgard/multicast.h>


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Distributed Systems Group");
MODULE_DESCRIPTION("ASGARD Connection Core");
MODULE_VERSION("0.01");

#undef LOG_PREFIX
#define LOG_PREFIX "[ASGARD][CORE]"

static int ifindex = -1;
module_param(ifindex, int, 0660);

static struct workqueue_struct *asgard_wq;

static int asgard_wq_lock = 0;

//  sdev, remote_lid, cluster_id, payload + cur_offset, instances - 1, bcnt - cur_offset

static struct asgard_core *score;

struct asgard_device *get_sdev(int devid)
{

	if (unlikely(devid < 0 || devid > MAX_NIC_DEVICES)) {
		asgard_error(" invalid asgard device id\n");
		return NULL;
	}

	if (unlikely(!score)) {
		asgard_error(" asgard core is not initialized\n");
		return NULL;
	}

	return score->sdevices[devid];
}
EXPORT_SYMBOL(get_sdev);

struct asgard_core *asgard_core(void)
{
	return score;
}
EXPORT_SYMBOL(asgard_core);

const char *asgard_get_protocol_name(enum asgard_protocol_type protocol_type)
{
	switch (protocol_type) {
	case ASGARD_PROTO_FD:
		return "Failure Detector";
	case ASGARD_PROTO_ECHO:
		return "Echo";
	case ASGARD_PROTO_CONSENSUS:
		return "Consensus";
	default:
		return "Unknown Protocol!";
	}
}
EXPORT_SYMBOL(asgard_get_protocol_name);

void asgard_post_ts(int asgard_id, uint64_t cycles, int ctype)
{
	struct asgard_device *sdev = get_sdev(asgard_id);
    struct echo_priv *epriv;
	//if (sdev->ts_state == ASGARD_TS_RUNNING)
	//	asgard_write_timestamp(sdev, 1, cycles, asgard_id);


	if(ctype == 2) { // channel type 2 is leader channel
		sdev->last_leader_ts = cycles;
		if(sdev->cur_leader_lid != -1) {
			sdev->pminfo.pm_targets[sdev->cur_leader_lid].chb_ts = cycles;
			sdev->pminfo.pm_targets[sdev->cur_leader_lid].alive = 1;
		}
	} else if(ctype == 3) { /* Channel Type is echo channel*/
        // asgard_dbg("optimistical timestamp on channel %d received\n", ctype);

        epriv = (struct echo_priv*) sdev->echo_priv;

        if(epriv != NULL) {
            epriv->ins->ctrl_ops.post_ts(epriv->ins, NULL, cycles);
        }
	}

}
EXPORT_SYMBOL(asgard_post_ts);


void set_all_targets_dead(struct asgard_device *sdev)
{
	struct pminfo *spminfo = &sdev->pminfo;
	int i;

	for (i = 0; i < spminfo->num_of_targets; i++)
		spminfo->pm_targets[i].alive = 0;

}
EXPORT_SYMBOL(set_all_targets_dead);

struct proto_instance *get_proto_instance(struct asgard_device *sdev, u16 proto_id)
{
	int idx;

	if (unlikely(proto_id < 0 || proto_id > MAX_PROTO_INSTANCES)){
		asgard_error("proto_id is invalid: %hu\n", proto_id);
		return NULL;
	}

	idx = sdev->instance_id_mapping[proto_id];

	if (unlikely(idx < 0 || idx >= MAX_PROTO_INSTANCES)){
		asgard_error("idx is invalid: %d\n", idx);

		return NULL;
	}

	return sdev->protos[idx];
}


void _handle_sub_payloads(struct asgard_device *sdev, int remote_lid, int cluster_id, char *payload, int instances, u32 bcnt)
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
	    asgard_error("BUG!? - Received packet that claimed to include %d instances\n", instances);
	    return;
	}

	// if (sdev->verbose >= 3)
	//	asgard_dbg("recursion. instances %d bcnt %d", instances, bcnt);

	cur_proto_id = GET_PROTO_TYPE_VAL(payload);

	// if (sdev->verbose >= 3)
	//	asgard_dbg("cur_proto_id %d", cur_proto_id);

	cur_offset = GET_PROTO_OFFSET_VAL(payload);

	// if (sdev->verbose >= 3)
	//	asgard_dbg("cur_offset %d", cur_offset);

	cur_ins = get_proto_instance(sdev, cur_proto_id);

	// check if instance for the given protocol id exists
	if (!cur_ins) {
		if (sdev->verbose >= 3)
			asgard_dbg("No instance for protocol id %d were found. instances=%d", cur_proto_id, instances);

    } else {
        cur_ins->ctrl_ops.post_payload(cur_ins, remote_lid, cluster_id, payload);
    }
	// handle next payload
	_handle_sub_payloads(sdev, remote_lid, cluster_id, payload + cur_offset, instances - 1, bcnt - cur_offset);

}

/*Checks if at least 3 Nodes have joined the cluster yet  */
int check_warmup_state(struct asgard_device *sdev, struct pminfo *spminfo)
{
	int i;
	int live_nodes = 0;

	if (unlikely(sdev->warmup_state == WARMING_UP)) {

	    if(spminfo->num_of_targets < 2) {
	        // asgard_error("number of targets in cluster is less than 2 (%d)\n", spminfo->num_of_targets);
            return 1;
        }

		// Do not start Leader Election until at least three nodes are alive in the cluster
		for (i = 0; i < spminfo->num_of_targets; i++)
			if (spminfo->pm_targets[i].alive)
                live_nodes++;

        if(live_nodes < 2) {
            asgard_error("live nodes is less than 2\n");
            return 1;
        }

		// Starting all protocols
		for (i = 0; i < sdev->num_of_proto_instances; i++) {
			if (sdev->protos != NULL && sdev->protos[i] != NULL && sdev->protos[i]->ctrl_ops.start != NULL) {
				sdev->protos[i]->ctrl_ops.start(sdev->protos[i]);
			} else {
				asgard_dbg("protocol instance %d not initialized", i);
			}
		}
		asgard_dbg("Warmup done!\n");
		sdev->warmup_state = WARMED_UP;
		update_leader(sdev, &sdev->pminfo);
	}
	return 0;
}


// Note: this function will not explicitly run on the same isolated cpu
//		.. for consecutive packets (even from the same host)
//   ... Timestamping may be
void pkt_process_handler(struct work_struct *w) {

	struct asgard_pkt_work_data *aw = NULL;
	char * user_data;


	aw = container_of(w, struct asgard_pkt_work_data, work);

    if(asgard_wq_lock) {
        asgard_dbg("drop handling of received packet - asgard shut down \n");
        goto exit;
    }

    user_data = ((char *) aw->payload) + aw->headroom + ETH_HLEN + sizeof(struct iphdr) + sizeof(struct udphdr);

	_handle_sub_payloads(aw->sdev, aw->remote_lid, aw->rcluster_id, GET_PROTO_START_SUBS_PTR(user_data),
		aw->received_proto_instances, aw->cqe_bcnt);

exit:

    kfree(aw->payload);

	if(aw)
		kfree(aw);
}

#define ASG_ETH_HEADER_SIZE 14

int extract_cluster_id_from_ad(char *payload) {

    u32 ad_cluster_id;
    enum le_opcode opcode;

    // check if opcode is ADVERTISE
    opcode = GET_CON_PROTO_OPCODE_VAL(payload);

    if(opcode != ADVERTISE) {
        return -1;
    }

    ad_cluster_id = GET_CON_PROTO_PARAM1_VAL(payload);
    return ad_cluster_id;
}


u32 extract_cluster_ip_from_ad(char *payload) {

    u32 ad_cluster_ip;
    enum le_opcode opcode;

    // check if opcode is ADVERTISE
    opcode = GET_CON_PROTO_OPCODE_VAL(payload);

    if(opcode != ADVERTISE) {
        return 0;
    }

    ad_cluster_ip = GET_CON_PROTO_PARAM2_VAL(payload);
    return ad_cluster_ip;
}

char *extract_cluster_mac_from_ad(char *payload) {

    char* ad_cluster_mac;
    enum le_opcode opcode;

    // check if opcode is ADVERTISE
    opcode = GET_CON_PROTO_OPCODE_VAL(payload);

    if(opcode != ADVERTISE) {
        return NULL;
    }

    ad_cluster_mac = GET_CON_PROTO_PARAM3_MAC_PTR(payload);

    if(!ad_cluster_mac)
        return NULL;

    return ad_cluster_mac;

}

void asgard_post_payload(int asgard_id, void *payload_in, u16 headroom, u32 cqe_bcnt)
{
	struct asgard_device *sdev = get_sdev(asgard_id);
	struct pminfo *spminfo = &sdev->pminfo;
	int remote_lid, rcluster_id, cluster_id_ad, i;
	u16 received_proto_instances;
	struct asgard_pkt_work_data *work;
	//uint64_t ts2, ts3;
	char *payload;
    char *remote_mac;
    char *user_data;
    u32 *dst_ip;
    u32 cluster_ip_ad;
    char *cluster_mac_ad;

    // freed by pkt_process_handler
    payload = kzalloc(cqe_bcnt, GFP_KERNEL);
	memcpy(payload, payload_in, cqe_bcnt);

    // asgard_write_timestamp(sdev, 1, RDTSC_ASGARD, asgard_id);

	remote_mac = ((char *) payload) + headroom + 6;
	user_data = ((char *) payload) + headroom + ETH_HLEN + sizeof(struct iphdr) + sizeof(struct udphdr);

	//ts2 = RDTSC_ASGARD;

	if (unlikely(!sdev)) {
		asgard_error("sdev is NULL\n");
		return;
	}

	if (unlikely(sdev->pminfo.state != ASGARD_PM_EMITTING))
		return;

	get_cluster_ids(sdev, remote_mac, &remote_lid, &rcluster_id);

	if (unlikely(remote_lid == -1)){
		dst_ip = (u32 *) (((char *) payload) + headroom + 30);

		/* Receiving messages from self? */
		if(*dst_ip != sdev->multicast_ip) {
		    asgard_error("Invalid PKT Source %x but %x\n", *dst_ip, sdev->multicast_ip);

            print_hex_dump(KERN_DEBUG, "raw pkt data: ", DUMP_PREFIX_NONE, 32, 1,
                            payload, cqe_bcnt > 128 ? 128 : cqe_bcnt , 0);

            asgard_dbg("Cluster has %d Members: \n", spminfo->num_of_targets);

            for (i = 0; i < spminfo->num_of_targets; i++) {
                asgard_dbg("\tCluster Member Node %d has IP: %pI4 MAC: %pMF\n",
                        spminfo->pm_targets[i].pkt_data.naddr.cluster_id,
                        (void*) & spminfo->pm_targets[i].pkt_data.naddr.dst_ip,
                        spminfo->pm_targets[i].pkt_data.naddr.dst_mac);
            }

		    return;
		}

        /* Extract advertised Cluster ID */
        cluster_id_ad = extract_cluster_id_from_ad(GET_PROTO_START_SUBS_PTR(user_data));
        cluster_ip_ad = extract_cluster_ip_from_ad(GET_PROTO_START_SUBS_PTR(user_data));
        cluster_mac_ad = extract_cluster_mac_from_ad(GET_PROTO_START_SUBS_PTR(user_data));

        asgard_dbg("\tMAC: %pMF", cluster_mac_ad);
        asgard_dbg("\tID: %d", cluster_id_ad);
        asgard_dbg("\tIP: %pI4", (void*) &cluster_ip_ad);

        if(cluster_id_ad < 0||cluster_ip_ad == 0||!cluster_mac_ad){
            asgard_error("included ip, id or mac is wrong \n");
            return;
        }

        asgard_core_register_remote_host(sdev->asgard_id, cluster_ip_ad, cluster_mac_ad, 1, cluster_id_ad);

        return;
	}

	// Update aliveness state and timestamps

	spminfo->pm_targets[remote_lid].chb_ts = RDTSC_ASGARD;
	spminfo->pm_targets[remote_lid].alive = 1;

    update_cluster_member(sdev->ci, remote_lid, 1);

	if(check_warmup_state(sdev, spminfo)){
	    // asgard_error("not warmed up yet.\n");
		return;
	}

	// TODO: get leader election protocol ID (current workaround: hardcoded, only use one protocol instance
    write_log(&sdev->protos[0]->logger, START_CONSENSUS, RDTSC_ASGARD);


    // asgard_dbg("PKT START:");
   //print_hex_dump(KERN_DEBUG, "raw pkt data: ", DUMP_PREFIX_NONE, 32, 1,
   //                payload, cqe_bcnt > 128 ? 128 : cqe_bcnt , 0);

	spminfo->pm_targets[remote_lid].pkt_rx_counter++;

	received_proto_instances = GET_PROTO_AMOUNT_VAL(user_data);

/*    _handle_sub_payloads(sdev, remote_lid, rcluster_id, GET_PROTO_START_SUBS_PTR(user_data),
                         received_proto_instances, cqe_bcnt);*/

    // freed by pkt_process_handler
    work = kmalloc(sizeof(struct asgard_pkt_work_data), GFP_ATOMIC);

	work->cqe_bcnt = cqe_bcnt;
	work->payload = payload;
	work->rcluster_id = rcluster_id;
	work->remote_lid = remote_lid;
	work->received_proto_instances = received_proto_instances;
	work->sdev = sdev;
	work->headroom = headroom;

	if(asgard_wq_lock){
	    asgard_dbg("Asgard is shutting down, ignoring packet\n");
	    kfree(work);
        return;
    }

	INIT_WORK(&work->work, pkt_process_handler);

	if(!queue_work(asgard_wq, &work->work)) {

	    asgard_dbg("Work item not put in query..");

		if(payload)
		    kfree(payload);
		if(work)
		    kfree(work);
	}

	//ts3 = RDTSC_ASGARD;

	//if (sdev->ts_state == ASGARD_TS_RUNNING){
    //    asgard_write_timestamp(sdev, 2, ts2, rcluster_id);
    //    asgard_write_timestamp(sdev, 3, ts3, rcluster_id);
    //}

}
EXPORT_SYMBOL(asgard_post_payload);

void asgard_reset_remote_host_counter(int asgard_id)
{
	int i;
	struct asgard_device *sdev = get_sdev(asgard_id);
	struct asgard_pm_target_info *pmtarget;

	for (i = 0; i < MAX_REMOTE_SOURCES; i++) {
		pmtarget = &sdev->pminfo.pm_targets[i];
		//kfree(pmtarget->pkt_data.payload);
		kfree(pmtarget->pkt_data.payload);
	}

	sdev->pminfo.num_of_targets = 0;

	asgard_dbg("reset number of targets to 0\n");
}
EXPORT_SYMBOL(asgard_reset_remote_host_counter);


/* Called by Connection Layer Glue (e.g. mlx5_con.c) */
int asgard_core_register_nic(int ifindex,  int asgard_id)
{
	char name_buf[MAX_ASGARD_PROC_NAME];
	int i;

	if (asgard_id < 0 || ifindex < 0) {
		asgard_error("Invalid parameter. asgard_id=%d, ifindex=%d", asgard_id, ifindex);
		return -EINVAL;
	}

	asgard_dbg("register nic at asgard core. ifindex=%d, asgard_id=%d\n", ifindex, asgard_id);
    // freed by asgard_connection_core_exit
	score->sdevices[asgard_id] =
		kmalloc(sizeof(struct asgard_device), GFP_KERNEL);

	score->num_devices++;
    score->sdevices[asgard_id]->ifindex = ifindex;
    score->sdevices[asgard_id]->hb_interval = 0;

    score->sdevices[asgard_id]->bug_counter = 0;
    score->sdevices[asgard_id]->asgard_id = asgard_id;
	score->sdevices[asgard_id]->ndev = asgard_get_netdevice(ifindex);
	score->sdevices[asgard_id]->pminfo.num_of_targets = 0;
    score->sdevices[asgard_id]->pminfo.waiting_window = 100000;
//	score->sdevices[asgard_id]->proto = NULL;
	score->sdevices[asgard_id]->verbose = 0;
	score->sdevices[asgard_id]->rx_state = ASGARD_RX_DISABLED;
	score->sdevices[asgard_id]->ts_state = ASGARD_TS_UNINIT;
	score->sdevices[asgard_id]->last_leader_ts = 0;
	score->sdevices[asgard_id]->num_of_proto_instances = 0;
	score->sdevices[asgard_id]->hold_fire = 0;
	score->sdevices[asgard_id]->tx_port = 3319;
	score->sdevices[asgard_id]->cur_leader_lid = -1;
	score->sdevices[asgard_id]->is_leader = 0;
    score->sdevices[asgard_id]->consensus_priv = NULL;
    score->sdevices[asgard_id]->echo_priv = NULL;

	score->sdevices[asgard_id]->multicast_ip = asgard_ip_convert("232.43.211.234");
    score->sdevices[asgard_id]->multicast_mac = asgard_convert_mac("01:00:5e:2b:d3:ea");

    score->sdevices[asgard_id]->multicast.aapriv
            = kmalloc(sizeof(struct asgard_async_queue_priv), GFP_KERNEL);

    score->sdevices[asgard_id]->multicast.delay = 0;
    score->sdevices[asgard_id]->multicast.enable = 0;
    score->sdevices[asgard_id]->multicast.nextIdx = 0;

    init_asgard_async_queue(score->sdevices[asgard_id]->multicast.aapriv);

    if(score->sdevices[asgard_id]->ndev) {

        if(!score->sdevices[asgard_id]->ndev->ip_ptr||!score->sdevices[asgard_id]->ndev->ip_ptr->ifa_list){
            asgard_error("Network Interface with ifindex %d has no IP Address configured!\n",ifindex);
            return -EINVAL;
        }

        score->sdevices[asgard_id]->self_ip = score->sdevices[asgard_id]->ndev->ip_ptr->ifa_list->ifa_address;

        if(!score->sdevices[asgard_id]->self_ip){
            asgard_error("self IP Address is NULL!");
            return -EINVAL;

        }

        if(!score->sdevices[asgard_id]->ndev->dev_addr){
            asgard_error("self MAC Address is NULL!");
            return -EINVAL;

        }
        score->sdevices[asgard_id]->self_mac = kmalloc(6, GFP_KERNEL);

        memcpy(score->sdevices[asgard_id]->self_mac, score->sdevices[asgard_id]->ndev->dev_addr, 6);

        asgard_dbg("Using IP: %x and MAC: %pMF", score->sdevices[asgard_id]->self_ip, score->sdevices[asgard_id]->self_mac);

    }

    score->sdevices[asgard_id]->pminfo.multicast_pkt_data.payload =
            kzalloc(sizeof(struct asgard_payload), GFP_KERNEL);

    score->sdevices[asgard_id]->pminfo.multicast_pkt_data_oos.payload =
            kzalloc(sizeof(struct asgard_payload), GFP_KERNEL);


    spin_lock_init(&score->sdevices[asgard_id]->pminfo.multicast_pkt_data_oos.lock);

    score->sdevices[asgard_id]->asgard_leader_wq =
		 alloc_workqueue("asgard_leader", WQ_HIGHPRI | WQ_CPU_INTENSIVE | WQ_UNBOUND, 1);

    /* Only one active Worker! Due to reading of the ringbuffer ..*/
    score->sdevices[asgard_id]->asgard_ringbuf_reader_wq =
            alloc_workqueue("asgard_ringbuf_reader", WQ_HIGHPRI | WQ_CPU_INTENSIVE | WQ_UNBOUND, 1);


    for (i = 0; i < MAX_PROTO_INSTANCES; i++)
		score->sdevices[asgard_id]->instance_id_mapping[i] = -1;

    // freed by clear_protocol_instances
    score->sdevices[asgard_id]->protos =
				kmalloc_array(MAX_PROTO_INSTANCES, sizeof(struct proto_instance *), GFP_KERNEL);

	if (!score->sdevices[asgard_id]->protos)
		asgard_error("ERROR! Not enough memory for protocols\n");

	/* set default heartbeat interval */
	//sdev->pminfo.hbi = DEFAULT_HB_INTERVAL;
	score->sdevices[asgard_id]->pminfo.hbi = CYCLES_PER_1MS;

	snprintf(name_buf, sizeof(name_buf), "asgard/%d", ifindex);
	proc_mkdir(name_buf, NULL);

	/* Initialize Timestamping for NIC */
	init_asgard_ts_ctrl_interfaces(score->sdevices[asgard_id]);
	init_timestamping(score->sdevices[asgard_id]);

	/* Initialize logger base for NIC */
	//init_log_ctrl_base(score->sdevices[asgard_id]);

	/*  Initialize protocol instance controller */
	init_proto_instance_ctrl(score->sdevices[asgard_id]);

    /*  Initialize multicast controller */
    init_multicast(score->sdevices[asgard_id]);


    /* Initialize Control Interfaces for NIC */
	init_asgard_pm_ctrl_interfaces(score->sdevices[asgard_id]);
	init_asgard_ctrl_interfaces(score->sdevices[asgard_id]);

	/* Initialize Component States*/
	pm_state_transition_to(&score->sdevices[asgard_id]->pminfo,
				   ASGARD_PM_UNINIT);

    /* Initialize synbuf for Cluster Membership - one page is enough */
    score->sdevices[asgard_id]->synbuf_clustermem = create_synbuf("clustermem", 1);

    if(!score->sdevices[asgard_id]->synbuf_clustermem){
        asgard_error("Could not create synbuf for clustermem");
        return -1;
    }

    /* Write ci changes directly to the synbuf
     * ubuf is at least one page which should be enough
     */
    score->sdevices[asgard_id]->ci = (struct cluster_info *)
            score->sdevices[asgard_id]->synbuf_clustermem->ubuf;

    score->sdevices[asgard_id]->ci->overall_cluster_member = 1; /* Node itself is a member */
    score->sdevices[asgard_id]->ci->cluster_self_id = score->sdevices[asgard_id]->pminfo.cluster_id;

    return asgard_id;
}
EXPORT_SYMBOL(asgard_core_register_nic);


int asgard_core_remove_nic(int asgard_id)
{
	char name_buf[MAX_ASGARD_PROC_NAME];

	if (asgard_validate_asgard_device(asgard_id))
		return -1;


    asgard_clean_timestamping(score->sdevices[asgard_id]);

	/* Remove Ctrl Interfaces for NIC */
	clean_asgard_pm_ctrl_interfaces(score->sdevices[asgard_id]);
	clean_asgard_ctrl_interfaces(score->sdevices[asgard_id]);

	remove_proto_instance_ctrl(score->sdevices[asgard_id]);

	//remove_logger_ifaces(&score->sdevices[asgard_id]->le_logger);

    /* Clean Multicast*/
    remove_multicast(score->sdevices[asgard_id]);

    snprintf(name_buf, sizeof(name_buf), "asgard/%d",
		 score->sdevices[asgard_id]->ifindex);

	remove_proc_entry(name_buf, NULL);

    /* Clean Cluster Membership Synbuf*/
    synbuf_chardev_exit(score->sdevices[asgard_id]->synbuf_clustermem);



	return 0;
}
EXPORT_SYMBOL(asgard_core_remove_nic);

int asgard_validate_asgard_device(int asgard_id)
{
	if (!score) {
		asgard_error("score is NULL!\n");
		return -1;
	}
	if (asgard_id < 0 || asgard_id > MAX_NIC_DEVICES) {
		asgard_error("invalid asgard_id! %d\n", asgard_id);
		return -1;
	}
	if (!score->sdevices || !score->sdevices[asgard_id]) {
		asgard_error("sdevices is invalid!\n");
		return -1;
	}

	return 0;
}
EXPORT_SYMBOL(asgard_validate_asgard_device);

int register_protocol_instance(struct asgard_device *sdev, int instance_id, int protocol_id)
{

	int idx = sdev->num_of_proto_instances;
	int ret;


	if (idx > MAX_PROTO_INSTANCES) {
		ret = -EPERM;
		asgard_dbg("Too many instances exist, can not exceed maximum of %d instances\n", MAX_PROTOCOLS);
		asgard_dbg("Current active instances: %d\n", sdev->num_of_proto_instances);

		goto error;
	}
	if(idx < 0) {
	    ret = -EPERM;
        asgard_dbg("Invalid Instance ID: %d\n",instance_id);
        goto error;
	}

	if(sdev->instance_id_mapping[instance_id] != -1){
	    asgard_dbg("Instance Already registered! (%d)\n", instance_id);
	    ret = -EINVAL;
	    goto error;
	}

	sdev->protos[idx] = generate_protocol_instance(sdev, protocol_id);

	if (!sdev->protos[idx]) {
		asgard_dbg("Could not allocate memory for new protocol instance!\n");
		ret = -ENOMEM;
		goto error;
	}

	sdev->instance_id_mapping[instance_id] = idx;

	sdev->protos[idx]->instance_id = instance_id;

	sdev->num_of_proto_instances++;

	sdev->protos[idx]->ctrl_ops.init(sdev->protos[idx]);

	return 0;
error:
	asgard_error("Could not register new protocol instance %d\n", ret);
	return ret;
}

void clear_protocol_instances(struct asgard_device *sdev)
{
	int i;

	if (!sdev) {
		asgard_error("SDEV is NULL - can not clear instances.\n");
		return;
	}

	if (sdev->num_of_proto_instances > MAX_PROTO_INSTANCES) {
		asgard_dbg("num_of_proto_instances is faulty! Aborting cleanup of all instances\n");
		return;
	}

	// If pacemaker is running, do not clear the protocols!
	if (sdev->pminfo.state == ASGARD_PM_EMITTING) {
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


int asgard_core_register_remote_host(int asgard_id, u32 ip, char *mac,
					int protocol_id, int cluster_id)
{
	struct asgard_device *sdev = get_sdev(asgard_id);
	struct asgard_pm_target_info *pmtarget;

	if (!mac) {
		asgard_error("input mac is NULL!\n");
		return -1;
	}

	if (sdev->pminfo.num_of_targets >= MAX_REMOTE_SOURCES) {
		asgard_error("Reached Limit of remote hosts.\n");
		asgard_error("Limit is=%d\n", MAX_REMOTE_SOURCES);
		return -1;
	}

	pmtarget = &sdev->pminfo.pm_targets[sdev->pminfo.num_of_targets];

	if (!pmtarget) {
		asgard_error("Pacemaker target is NULL\n");
		return -1;
	}
	pmtarget->alive = 0;
	pmtarget->pkt_data.naddr.dst_ip = ip;
	pmtarget->pkt_data.naddr.cluster_id = cluster_id;
    pmtarget->pkt_data.port = 3320;
	pmtarget->lhb_ts = 0;
	pmtarget->chb_ts = 0;
	pmtarget->resp_factor = 4;
	pmtarget->cur_waiting_interval = 2;
	pmtarget->pkt_tx_counter = 0;
	pmtarget->pkt_rx_counter = 0;
    pmtarget->pkt_tx_errors = 0;

	pmtarget->scheduled_log_replications = 0;
	pmtarget->received_log_replications = 0;

	pmtarget->pkt_data.payload =
		kzalloc(sizeof(struct asgard_payload), GFP_KERNEL);

    spin_lock_init(&pmtarget->pkt_data.lock);

	memcpy(&pmtarget->pkt_data.naddr.dst_mac, mac, sizeof(unsigned char) * 6);

	/* Local ID is increasing with the number of targets */
    add_cluster_member(sdev->ci, cluster_id, sdev->pminfo.num_of_targets, 2);

    pmtarget->aapriv = kmalloc(sizeof(struct asgard_async_queue_priv), GFP_KERNEL);
    init_asgard_async_queue(pmtarget->aapriv);




            /* Out of schedule SKB  pre-allocation*/
    sdev->pminfo.pm_targets[sdev->pminfo.num_of_targets].pkt_data.skb = asgard_reserve_skb(sdev->ndev, ip, mac, NULL);
    skb_set_queue_mapping(sdev->pminfo.pm_targets[sdev->pminfo.num_of_targets].pkt_data.skb, sdev->pminfo.active_cpu); // Queue mapping same for each target i

    sdev->pminfo.num_of_targets = sdev->pminfo.num_of_targets + 1;
    asgard_error("Registered Cluster Member with ID %d\n", cluster_id);

	return 0;
}
EXPORT_SYMBOL(asgard_core_register_remote_host);


static int __init asgard_connection_core_init(void)
{
	int i;
	int err = -EINVAL;
    int ret = 0;

	if (ifindex < 0) {
		asgard_error("ifindex parameter is missing\n");
		goto error;
	}

	err = register_asgard_at_nic(ifindex, asgard_post_ts, asgard_post_payload);

	if (err)
		goto error;

    /* Initialize User Space interfaces
     * NOTE: BEFORE call to asgard_core_register_nic! */
    err = synbuf_bypass_init_class();

    if(err) {
        asgard_error("synbuf_bypass_init_class failed\n");
        return -ENODEV;
    }

    // freed by asgard_connection_core_exit
    score = kmalloc(sizeof(struct asgard_core), GFP_KERNEL);

	if (!score) {
		asgard_error("allocation of asgard core failed\n");
		return -1;
	}

	score->num_devices = 0;

    // freed by asgard_connection_core_exit
	score->sdevices = kmalloc_array(
		MAX_NIC_DEVICES, sizeof(struct asgard_device *), GFP_KERNEL);

	if (!score->sdevices) {
		asgard_error("allocation of score->sdevices failed\n");
		return -1;
	}

	for(i = 0; i < MAX_NIC_DEVICES; i++)
		score->sdevices[i] = NULL;

    proc_mkdir("asgard", NULL);

	ret = asgard_core_register_nic(ifindex, get_asgard_id_by_ifindex(ifindex));

	if(ret < 0) {
	    asgard_error("Could not register NIC at asgard\n");
	    goto reg_failed;
	}

	//init_asgard_proto_info_interfaces();

	/* Allocate Workqueues */
	asgard_wq = alloc_workqueue("asgard",  WQ_HIGHPRI | WQ_CPU_INTENSIVE | WQ_UNBOUND | WQ_MEM_RECLAIM | WQ_FREEZABLE, 1);
    asgard_wq_lock = 0;


    asgard_dbg("asgard core initialized.\n");
    return 0;

reg_failed:
    unregister_asgard();

    kfree(score->sdevices);
    kfree(score);
    remove_proc_entry("asgard", NULL);

error:
	asgard_error("Could not initialize asgard - aborting init.\n");
	return err;

}

void asgard_stop_pacemaker(struct asgard_device *adev)
{

	if(!adev) {
		asgard_error("asgard device is NULL\n");
		return;
	}

	adev->pminfo.state = ASGARD_PM_READY;
}

void asgard_stop_timestamping(struct asgard_device *adev)
{

	if(!adev) {
		asgard_error("Asgard Device is Null.\n");
		return;
	}

	asgard_ts_stop(adev);


}

void asgard_stop(int asgard_id)
{

	if (asgard_validate_asgard_device(asgard_id)){
		asgard_dbg("invalid asgard id %d", asgard_id);
		return;
	}

	asgard_stop_timestamping(score->sdevices[asgard_id]);

	asgard_stop_pacemaker(score->sdevices[asgard_id]);

}

static void __exit asgard_connection_core_exit(void)
{
	int i, j;

    asgard_wq_lock = 1;

    mb();

    if(asgard_wq)
        flush_workqueue(asgard_wq);

    /* MUST unregister asgard for drivers first */
	unregister_asgard();

    if(!score){
		asgard_error("score is NULL \n");
		return;
	}

	for(i = 0; i < MAX_NIC_DEVICES; i++) {

		if(!score->sdevices[i]) {
			continue;
		}

		asgard_stop(i);

        // clear all async queues
        for(j = 0; j <  score->sdevices[i]->pminfo.num_of_targets; j++)
            async_clear_queue(score->sdevices[i]->pminfo.pm_targets[j].aapriv);

        destroy_workqueue(score->sdevices[i]->asgard_leader_wq);
        destroy_workqueue(score->sdevices[i]->asgard_ringbuf_reader_wq);

		clear_protocol_instances(score->sdevices[i]);

		asgard_core_remove_nic(i);

        kfree(score->sdevices[i]);

    }
    if(asgard_wq)
        destroy_workqueue(asgard_wq);

    if(score->sdevices)
        kfree(score->sdevices);

    if(score)
        kfree(score);

	remove_proc_entry("asgard", NULL);

    synbuf_clean_class();
    asgard_dbg("ASGARD CORE CLEANED \n\n\n\n");
	// flush_workqueue(asgard_wq);

}

module_init(asgard_connection_core_init);
module_exit(asgard_connection_core_exit);
