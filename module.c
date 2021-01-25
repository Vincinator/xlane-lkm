
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/etherdevice.h>
#include <linux/ip.h>
#include <net/ip.h>
#include <linux/netdevice.h>
#include <asm/checksum.h>
#include <linux/skbuff.h>
#include <linux/notifier.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/slab.h>
#include <linux/inetdevice.h>

#include <linux/kernel.h>

#include <linux/compiler.h>



#include "module.h"


#include "logger.h"
#include "echo.h"
#include "lkm/synbuf-chardev.h"
#include "lkm/pm-ctrl.h"
#include "lkm/kernel_ts.h"
#include "lkm/core-ctrl.h"
#include "lkm/proto-instance-ctrl.h"
#include "lkm/multicast-ctrl.h"

#include "replication.h"
#include "membership.h"
#include "pktqueue.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Distributed Systems Group");
MODULE_DESCRIPTION("ASGARD Connection Core");
MODULE_VERSION("0.01");

#undef LOG_PREFIX
#define LOG_PREFIX "[ASGARD][LKM]"


static int ifindex = -1;
module_param(ifindex, int, 0660);
static struct workqueue_struct *asgard_wq;

static int asgard_wq_lock = 0;

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



int is_ip_local(struct net_device *dev,	u32 ip_addr)
{
    u32 local_ipaddr;

    if(!dev ||! dev->ip_ptr ||! dev->ip_ptr->ifa_list || !dev->ip_ptr->ifa_list->ifa_address) {
        asgard_error("Network Device not initialized properly!\n");
        return 0;
    }

    local_ipaddr = ntohl(dev->ip_ptr->ifa_list->ifa_address);

    return local_ipaddr == ip_addr;
}
EXPORT_SYMBOL(is_ip_local);


void asgard_post_ts(int asgard_id, uint64_t cycles, int ctype)
{
    struct asgard_device *sdev = get_sdev(asgard_id);
    struct echo_priv *epriv;
    //if (sdev->ts_state == ASGARD_TS_RUNNING)
    //	asgard_write_timestamp(sdev, 1, cycles, asgard_id);

    if (ctype == 2) { // channel type 2 is leader channel
        sdev->last_leader_ts = cycles;
        if (sdev->cur_leader_lid != -1) {
            sdev->pminfo.pm_targets[sdev->cur_leader_lid].chb_ts =
                    cycles;
            sdev->pminfo.pm_targets[sdev->cur_leader_lid].alive = 1;
        }
    } else if (ctype == 3) { /* Channel Type is echo channel*/
        // asgard_dbg("optimistical timestamp on channel %d received\n", ctype);

        epriv = (struct echo_priv *)sdev->echo_priv;

        if (epriv != NULL) {
            epriv->ins->ctrl_ops.post_ts(epriv->ins, NULL, cycles);
        }
    }
}

struct sk_buff *asgard_reserve_skb(struct net_device *dev, u32 dst_ip, unsigned char *dst_mac, struct asgard_payload *payload)
{
    int ip_len, udp_len, asgard_len, total_len;
    struct sk_buff *skb;
    struct ethhdr *eth;
    struct iphdr *iph;
    struct udphdr *udph;
    static atomic_t ip_ident;
    u32 local_ipaddr;

    local_ipaddr = ntohl(dev->ip_ptr->ifa_list->ifa_address);

    asgard_len = sizeof(struct asgard_payload);
    udp_len = asgard_len + sizeof(struct udphdr);
    ip_len = udp_len + sizeof(struct iphdr);

    total_len = ip_len + ETH_HLEN;

    //asgard_dbg("Allocating %d Bytes for SKB\n", total_len);
    //asgard_dbg("asgard_len = %d Bytes\n", asgard_len);

    //  asgard_dbg("LL_RESERVED_SPACE_EXTRA(dev,0) = %d Bytes\n", LL_RESERVED_SPACE_EXTRA(dev,0));

    // head == data == tail
    // end = head + allocated skb size
    skb = alloc_skb(total_len, GFP_KERNEL);

    if (!skb) {
        asgard_error("Could not allocate SKB\n");
        return NULL;
    }

    refcount_set(&skb->users, 1);

    // data == tail == head + ETH_HLEN + sizeof(struct iphdr)+ sizeof(struct udphdr)
    // end = head + allocated skb size
    // reserving headroom for header to expand
    skb_reserve(skb,   ETH_HLEN + sizeof(struct iphdr)+ sizeof(struct udphdr));

    skb->dev = dev;

    // data == tail - sizeof(struct asgard_payload)
    // reserve space for payload
    skb_put(skb, sizeof(struct asgard_payload));

    // init skb user payload with 0
    memset(skb_tail_pointer(skb) - sizeof(struct asgard_payload), 0, sizeof(struct asgard_payload));

    // data = data - sizeof(struct udphdr)
    skb_push(skb, sizeof(struct udphdr));

    skb_reset_transport_header(skb);

    udph = udp_hdr(skb);

    udph->source = htons((u16) 1111);

    udph->dest = htons((u16) 3319);

    udph->len = htons(udp_len);

    udph->check = 0;

    udph->check = csum_tcpudp_magic(local_ipaddr, dst_ip, udp_len, IPPROTO_UDP,csum_partial(udph, udp_len, 0));

    if (udph->check == 0)
        udph->check = CSUM_MANGLED_0;

    // data = data - sizeof(struct iphdr)
    skb_push(skb, sizeof(struct iphdr));

    skb_reset_network_header(skb);

    iph = ip_hdr(skb);

    put_unaligned(0x45, (unsigned char *)iph);

    iph->tos      = 0;
    put_unaligned(htons(ip_len), &(iph->tot_len));
    iph->id       = htons(atomic_inc_return(&ip_ident));
    iph->frag_off = 0;
    iph->ttl      = 64;
    iph->protocol = IPPROTO_UDP;
    iph->check    = 0;

    put_unaligned(local_ipaddr, &(iph->saddr));
    put_unaligned(dst_ip, &(iph->daddr));

    iph->check
            = ip_fast_csum((unsigned char *)iph, iph->ihl);

    // data = data - ETH_HLEN
    eth = skb_push(skb, ETH_HLEN);

    skb_reset_mac_header(skb);
    skb->protocol = eth->h_proto = htons(ETH_P_IP);

    ether_addr_copy(eth->h_source, dev->dev_addr);
    ether_addr_copy(eth->h_dest, dst_mac);

    skb->dev = dev;

    return skb;
}
EXPORT_SYMBOL(asgard_reserve_skb);

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
    pmtarget->pkt_data.naddr.port = 3320;
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

    spin_lock_init(&pmtarget->pkt_data.slock);

    memcpy(&pmtarget->pkt_data.naddr.dst_mac, mac,
           sizeof(unsigned char) * 6);

    /* Local ID is increasing with the number of targets */
    add_cluster_member(sdev->ci, cluster_id, sdev->pminfo.num_of_targets,
                       2);

    pmtarget->aapriv =
            kmalloc(sizeof(struct asgard_async_queue_priv), GFP_KERNEL);
    init_asgard_async_queue(pmtarget->aapriv);

    /* Out of schedule SKB  pre-allocation*/
    sdev->pminfo.pm_targets[sdev->pminfo.num_of_targets].pkt_data.skb =
            asgard_reserve_skb(sdev->ndev, ip, mac, NULL);
    skb_set_queue_mapping(
            sdev->pminfo.pm_targets[sdev->pminfo.num_of_targets]
                    .pkt_data.skb,
            sdev->pminfo.active_cpu); // Queue mapping same for each target i

    sdev->pminfo.num_of_targets = sdev->pminfo.num_of_targets + 1;
    asgard_error("Registered Cluster Member with ID %d\n", cluster_id);

    return 0;
}
EXPORT_SYMBOL(asgard_core_register_remote_host);


void asg_init_workqueues(struct asgard_device *sdev){
    /* Allocate Workqueues */
    sdev->asgard_wq = alloc_workqueue("asgard",
                                      WQ_HIGHPRI | WQ_CPU_INTENSIVE | WQ_UNBOUND |
                                      WQ_MEM_RECLAIM | WQ_FREEZABLE,
                                      1);
    sdev->asgard_wq_lock = 0;

}
EXPORT_SYMBOL(asg_init_workqueues);

static int __init asgard_connection_core_init(void)
{
    int i;
    int err = -EINVAL;
    int ret = 0;

    if (ifindex < 0) {
        asgard_error("ifindex parameter is missing\n");
        goto error;
    }

//    err = register_asgard_at_nic(ifindex, asgard_post_ts,
 //                                asgard_post_payload, asgard_force_quit);

    if (err)
        goto error;

    /* Initialize User Space interfaces
     * NOTE: BEFORE call to asgard_core_register_nic! */
    err = synbuf_bypass_init_class();

    if (err) {
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
    score->sdevices = kmalloc_array(MAX_NIC_DEVICES, sizeof(struct asgard_device *), GFP_KERNEL);

    if (!score->sdevices) {
        asgard_error("allocation of score->sdevices failed\n");
        return -1;
    }

    for (i = 0; i < MAX_NIC_DEVICES; i++)
        score->sdevices[i] = NULL;

    proc_mkdir("asgard", NULL);

//    ret = asgard_core_register_nic(ifindex,
//                                   get_asgard_id_by_ifindex(ifindex));

    if (ret < 0) {
        asgard_error("Could not register NIC at asgard\n");
        goto reg_failed;
    }

    //init_asgard_proto_info_interfaces();



    asgard_dbg("asgard core initialized.\n");
    return 0;
reg_failed:
    // unregister_asgard();

    kfree(score->sdevices);
    kfree(score);
    remove_proc_entry("asgard", NULL);

error:
    asgard_error("Could not initialize asgard - aborting init.\n");
    return err;
}


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

void asgard_stop_pacemaker(struct asgard_device *adev)
{
    if (!adev) {
        asgard_error("asgard device is NULL\n");
        return;
    }

    adev->pminfo.state = ASGARD_PM_READY;
}

void asgard_stop_timestamping(struct asgard_device *adev)
{
    if (!adev) {
        asgard_error("Asgard Device is Null.\n");
        return;
    }

    asgard_ts_stop(adev);
}

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

void asgard_stop(int asgard_id)
{
    if (asgard_validate_asgard_device(asgard_id)) {
        asgard_dbg("invalid asgard id %d", asgard_id);
        return;
    }

    asgard_stop_timestamping(score->sdevices[asgard_id]);

    asgard_stop_pacemaker(score->sdevices[asgard_id]);
}

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


void clear_protocol_instances(struct asgard_device *sdev)
{
    int i;

    if (!sdev) {
        asgard_error("SDEV is NULL - can not clear instances.\n");
        return;
    }

    if (sdev->num_of_proto_instances > MAX_PROTO_INSTANCES) {
        asgard_dbg(
                "num_of_proto_instances is faulty! Aborting cleanup of all instances\n");
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
        if (sdev->protos[i]->proto_data)
            kfree(sdev->protos[i]->proto_data);

        if (!sdev->protos[i])
            kfree(sdev->protos[i]); // is non-NULL, see continue condition above
    }

    for (i = 0; i < MAX_PROTO_INSTANCES; i++)
        sdev->instance_id_mapping[i] = -1;
    sdev->num_of_proto_instances = 0;
}


static void __exit asgard_connection_core_exit(void)
{
    int i, j;

    asgard_wq_lock = 1;

    mb();

    if (asgard_wq)
        flush_workqueue(asgard_wq);

    /* MUST unregister asgard for drivers first */
//    unregister_asgard();

    if (!score) {
        asgard_error("score is NULL \n");
        return;
    }

    for (i = 0; i < MAX_NIC_DEVICES; i++) {
        if (!score->sdevices[i]) {
            continue;
        }

        asgard_stop(i);

        // clear all async queues
        for (j = 0; j < score->sdevices[i]->pminfo.num_of_targets; j++)
            async_clear_queue(
                    score->sdevices[i]->pminfo.pm_targets[j].aapriv);

        destroy_workqueue(score->sdevices[i]->asgard_leader_wq);
        destroy_workqueue(score->sdevices[i]->asgard_ringbuf_reader_wq);

        clear_protocol_instances(score->sdevices[i]);

        asgard_core_remove_nic(i);

        kfree(score->sdevices[i]);
    }
    if (asgard_wq)
        destroy_workqueue(asgard_wq);

    if (score->sdevices)
        kfree(score->sdevices);

    if (score)
        kfree(score);

    remove_proc_entry("asgard", NULL);

    synbuf_clean_class();
    asgard_dbg("ASGARD CORE CLEANED \n\n\n\n");
    // flush_workqueue(asgard_wq);
}




module_init(asgard_connection_core_init);
module_exit(asgard_connection_core_exit);

