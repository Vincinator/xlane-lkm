#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <sassy/logger.h>
#include <linux/slab.h>

#include "sassy_core.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Vincent Riesop");
MODULE_DESCRIPTION("SASSY Connection Core");
MODULE_VERSION("0.01");

#undef LOG_PREFIX
#define LOG_PREFIX "[SASSY][CORE]"

/* RX Data */
struct sassy_core *score;


int remote_host_counter = 0;

struct sassy_rx_buffer * sassy_get_rx_buffer(int sassy_id, int remote_id) {
	return score->rx_tables[sassy_id]->rhost_buffers[remote_host_counter];
}

int sassy_core_write_packet(int sassy_id, int remote_id) {
	struct sassy_rx_buffer *buf = sassy_get_rx_buffer(sassy_id, remote_id);
	sassy_dbg("Not implemented: %s\n", __FUNCTION__);
}


/* Called by Connection Layer Glue (e.g. mlx5_con.c) */
int sassy_core_register_nic(int sassy_id) {

	sassy_dbg("register nic at sassy core\n");

	score->rx_tables[sassy_id] = kmalloc(sizeof(struct sassy_rx_table), GFP_ATOMIC);
	score->rx_tables[sassy_id]->rhost_buffers = kmalloc_array(MAX_REMOTE_SOURCES, sizeof(struct sassy_rx_buffer*), GFP_ATOMIC);
	
	return 0;
}
EXPORT_SYMBOL(sassy_core_register_nic);

int sassy_core_register_remote_host(int sassy_id){
	struct sassy_rx_table *rxt = score->rx_tables[sassy_id];

	if(remote_host_counter >= MAX_REMOTE_SOURCES) {
		sassy_error("Reached Limit of remote hosts. \n");
		sassy_error("Limit is=%d, remote_host_counter= %d \n", MAX_REMOTE_SOURCES, remote_host_counter);
		return -1;
	}

	rxt->rhost_buffers[remote_host_counter] = kmalloc(sizeof(struct sassy_rx_buffer), GFP_ATOMIC);


	return remote_host_counter++;
}


static int __init sassy_connection_core_init(void)
{
	sassy_dbg("init\n");

	score = kmalloc(sizeof(struct sassy_core), GFP_ATOMIC);

	if(!score) {
		sassy_error("allocation of sassy core failed\n");
		return -1;
	}

	score->rx_tables = kmalloc_array(MAX_NIC_PORTS, sizeof(struct sassy_rx_table *), GFP_ATOMIC);

	if(!score->rx_tables) {
		sassy_error("allocation of score->rx_tablesfailed\n");
		return -1;
	}


	sassy_dbg("init done\n");

	return 0;
}

static void __exit sassy_connection_core_exit(void) 
{

	sassy_dbg("cleanup\n");
	kfree(score);

	sassy_dbg("cleanup done\n");

}

subsys_initcall(sassy_connection_core_init);
module_exit(sassy_connection_core_exit);