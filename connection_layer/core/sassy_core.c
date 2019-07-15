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

static int device_counter = 0;


void sassy_reset_remote_host_counter(int sassy_id){
    int i;
    struct sassy_rx_table *rxt;
    struct sassy_device *sdev;
    struct sassy_pm_target_info *pmtarget;

    if(sassy_validate_sassy_device(sassy_id))
        return;

    rxt = score->rx_tables[sassy_id];
    sdev = score->sdevices[sassy_id];

    /* Free Memory of all  */
    for(i = 0; i < MAX_REMOTE_SOURCES; i++){

        pmtarget = &sdev->pminfo.pm_targets[i];

        kfree(rxt->rhost_buffers[i]);
        kfree(pmtarget->hb_pkt_params->hb_payload);
        kfree(pmtarget->hb_pkt_params->dst_mac);
        kfree(pmtarget->hb_pkt_params);
    }
    sdev->pminfo.num_of_targets = 0;
    sassy_dbg(" set num_of_targets to 0\n");
}
EXPORT_SYMBOL(sassy_reset_remote_host_counter);

int sassy_generate_next_id(void) 
{
    if(device_counter >= SASSY_MLX5_DEVICES_LIMIT) {
        sassy_error("Reached Limit of maximum connected mlx5 devices.\n");
        sassy_error("Limit=%d, device_counter=%d\n", SASSY_MLX5_DEVICES_LIMIT, device_counter);
        return -1;
    }

    return device_counter++;
}

int sassy_core_write_packet(int sassy_id, int remote_id) {
    sassy_dbg("Not implemented: %s\n", __FUNCTION__);
    return 0;
}


/* Called by Connection Layer Glue (e.g. mlx5_con.c) */
int sassy_core_register_nic(int ifindex) 
{
    char name_buf[MAX_SYNCBEAT_PROC_NAME];
    int sassy_id;

    sassy_dbg("register nic at sassy core\n");

    sassy_id = sassy_generate_next_id();

    if(sassy_id < 0)
        return -1;


    score->rx_tables[sassy_id] = kmalloc(sizeof(struct sassy_rx_table), GFP_KERNEL);
    score->rx_tables[sassy_id]->rhost_buffers = kmalloc_array(MAX_REMOTE_SOURCES, sizeof(struct sassy_rx_buffer*), GFP_KERNEL);
    
    score->sdevices[sassy_id] = kmalloc(sizeof(struct sassy_device), GFP_KERNEL);
    score->sdevices[sassy_id]->ifindex = ifindex;
    score->sdevices[sassy_id]->sassy_id = sassy_id;
    score->sdevices[sassy_id]->ndev = sassy_get_netdevice(ifindex);
    score->sdevices[sassy_id]->pminfo.num_of_targets = 0;

    snprintf(name_buf,  sizeof name_buf, "sassy/%d", ifindex);
    proc_mkdir(name_buf, NULL);

    /* Initialize Control Interfaces for NIC */
    init_sassy_pm_ctrl_interfaces(score->sdevices[sassy_id]);


    /* Initialize Component States*/
    pm_state_transition_to(&score->sdevices[sassy_id]->pminfo, SASSY_PM_UNINIT);

    return sassy_id;
}
EXPORT_SYMBOL(sassy_core_register_nic);


int sassy_core_remove_nic(int sassy_id)
{
    int i;
    char name_buf[MAX_SYNCBEAT_PROC_NAME];

    if(sassy_validate_sassy_device(sassy_id)){
        return -1;
    }

    /* Remove Ctrl Interfaces for NIC */
    clean_sassy_pm_ctrl_interfaces(score->sdevices[sassy_id]);

    snprintf(name_buf,  sizeof name_buf, "sassy/%d", score->sdevices[sassy_id]->ifindex);
    proc_mkdir(name_buf, NULL);

    /* Free Memory used for this NIC */

    for(i = 0; i < MAX_PROCESSES_PER_HOST; i++){
        kfree(score->rx_tables[sassy_id]->rhost_buffers[i]);
    }
    kfree(score->rx_tables[sassy_id]);
    kfree(score->sdevices[sassy_id]);

}

int sassy_validate_sassy_device(int sassy_id) {
    
    if(!score){
        sassy_error("score is NULL!\n");
        return -1;
    }
    if(sassy_id < 0 || sassy_id > MAX_NIC_DEVICES){
        sassy_error("invalid sassy_id! %d\n", sassy_id);
        return -1;
    }
    if(!score->sdevices || !score->sdevices[sassy_id]){
        sassy_error("sdevices is invalid!\n");
        return -1;
    }

    if(!score->rx_tables || !score->rx_tables[sassy_id]){
        sassy_error("rx_tables is invalid!\n");
        return -1;
    }
    return 0;
}
EXPORT_SYMBOL(sassy_validate_sassy_device);

int sassy_core_register_remote_host(int sassy_id, uint32_t ip, char *mac)
{
    struct sassy_rx_table *rxt;
    struct sassy_device *sdev;
    struct sassy_pm_target_info *pmtarget;
    int ifindex;

    if(!mac){
        sassy_error("input mac is NULL!\n");
        return -1;
    }

    if(sassy_validate_sassy_device(sassy_id)){
        return -1;
    }

    sdev = score->sdevices[sassy_id];

    if(!sdev) {
        sassy_error("sdev is NULL \n");
        return -1;
    }

    if(sdev->pminfo.num_of_targets >= MAX_REMOTE_SOURCES) {
        sassy_error("Reached Limit of remote hosts. \n");
        sassy_error("Limit is=%d \n", MAX_REMOTE_SOURCES);
        return -1;
    }

    rxt = score->rx_tables[sassy_id];

    if(!rxt){
        sassy_error("rxt is NULL \n");
        return -1;
    }


    ifindex = sdev->ifindex;
    pmtarget = &sdev->pminfo.pm_targets[sdev->pminfo.num_of_targets];

    if(!pmtarget){
        sassy_error("pmtarget is NULL\n");
        return -1;
    }
    sassy_dbg(" write rhost_buffers \n");
    rxt->rhost_buffers[sdev->pminfo.num_of_targets] = kmalloc(sizeof(struct sassy_rx_buffer), GFP_KERNEL);

    pmtarget->hb_pkt_params = kzalloc(sizeof(struct sassy_hb_packet_params), GFP_KERNEL);
    sassy_dbg(" 2 \n");

    pmtarget->hb_pkt_params->hb_payload = kzalloc(sizeof(struct sassy_heartbeat_payload), GFP_KERNEL);
    sassy_dbg(" 3 \n");

    pmtarget->hb_pkt_params->dst_mac = kmalloc(sizeof(unsigned char) * 6, GFP_KERNEL);
    sassy_dbg(" 4 \n");

    pmtarget->hb_pkt_params->dst_ip = ip;
    sassy_dbg(" 5 \n");

    memcpy(pmtarget->hb_pkt_params->dst_mac, mac, sizeof(unsigned char) * 6);
    sassy_dbg(" 6 \n");

    sdev->pminfo.num_of_targets = sdev->pminfo.num_of_targets + 1;
    sassy_dbg(" 7 \n");

    return 0;

}
EXPORT_SYMBOL(sassy_core_register_remote_host);


static int __init sassy_connection_core_init(void)
{
    sassy_dbg("init\n");

    score = kmalloc(sizeof(struct sassy_core), GFP_KERNEL);

    if(!score) {
        sassy_error("allocation of sassy core failed\n");
        return -1;
    }

    score->rx_tables = kmalloc_array(MAX_NIC_DEVICES, sizeof(struct sassy_rx_table *), GFP_KERNEL);

    if(!score->rx_tables) {
        sassy_error("allocation of score->rx_tables failed\n");
        return -1;
    }

    score->sdevices = kmalloc_array(MAX_NIC_DEVICES, sizeof(struct sassy_device *), GFP_KERNEL);

    if(!score->rx_tables) {
        sassy_error("allocation of score->sdevices failed\n");
        return -1;
    }


    proc_mkdir("sassy", NULL);

    sassy_dbg("init done\n");

    return 0;
}

static void __exit sassy_connection_core_exit(void) 
{
    int i;

    sassy_dbg("cleanup\n");

    for(i = 0; i < device_counter; i++){
        sassy_core_remove_nic(i);
    } 


    kfree(score);



    sassy_dbg("cleanup done\n");

}

subsys_initcall(sassy_connection_core_init);
module_exit(sassy_connection_core_exit);