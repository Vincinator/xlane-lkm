#include <sassy/logger.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/uaccess.h>

#include <linux/kernel.h>

#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/udp.h>
#include <linux/etherdevice.h>
#include <linux/ip.h>
#include <net/ip.h>
#include <linux/slab.h>
#include <linux/inetdevice.h>


#include <sassy/sassy.h>


void sassy_pm_test_update_proc_state(struct sassy_pacemaker_info *spminfo, int procid, int state){
	struct sassy_process_info *cur_pinfo = &spminfo->tdata.pinfos[procid];
	
	cur_pinfo->ps = state & 0xFF;
	sassy_dbg("Process %d updated state to %hhu", procid, cur_pinfo->ps);
}


static ssize_t sassy_test_procfile_write(struct file *file, const char __user *user_buffer, size_t count, loff_t *data)
{
	int err;
	char kernel_buffer[count+1];
	struct sassy_test_procfile_container *container = (struct sassy_test_procfile_container*)PDE_DATA(file_inode(file));
	struct sassy_pacemaker_info *spminfo = container->spminfo;

	long state = -1;

	if (!spminfo)
		return -ENODEV;

	if (count == 0) {
		return -EINVAL;
	}

	err = copy_from_user(kernel_buffer, buffer, count);

	if (err) {
		sassy_error("Copy from user failed%s\n", __FUNCTION__);
		return err;
	}

	kernel_buffer[count] = '\0';

	err = kstrtol(kernel_buffer, 0, &state);

	if (err) {
		sassy_error(" Error converting input%s\n", __FUNCTION__);
		return err;
	}

	sassy_pm_test_update_proc_state(spminfo, container->procid, state);

	sassy_dbg("created %d active user space");

}

static int sassy_test_procfile_show(struct seq_file *m, void *v)
{
	struct sassy_test_procfile_container *container = (struct sassy_test_procfile_container*) m->private;
	struct sassy_pacemaker_info *spminfo = container->spminfo;
	int procid = container->procid;
	struct sassy_process_info *cur_pinfo = &spminfo->tdata.pinfos[procid];

	int i;

	if (!spminfo)
		return -ENODEV;
	
	seq_printf(m, "state: %hhu", cur_pinfo->ps);
	
	return 0;
}

static int sassy_test_procfile_open(struct inode *inode, struct file *file)
{
	return single_open(file, sassy_test_procfile_show,PDE_DATA(file_inode(file)));
}


static const struct file_operations sassy_test_procfile_ops = {
		.owner	= THIS_MODULE,
		.open	= sassy_test_procfile_open,
		.write	= sassy_test_procfile_write,
		.read	= seq_read,
		.llseek	= seq_lseek,
		.release = single_release,
};


void sassy_pm_create_test_procfile(struct sassy_test_procfile_container *container) {
	char name_buf[MAX_SYNCBEAT_PROC_NAME];
	struct sassy_pacemaker_info *spminfo = container->spminfo;

	struct sassy_device *sdev = container_of(spminfo, struct sassy_device, pminfo);

	if(!sdev){
		sassy_error(" Can not find sdev \n");
		return;
	}

	snprintf(name_buf, sizeof name_buf, "sassy/%d/pacemaker/test/%d", sdev->ifindex, container->procid);
	proc_create_data(name_buf, S_IRWXU|S_IRWXO, NULL, &sassy_test_procfile_ops, container);


}

void sassy_pm_create_test_data(struct sassy_pacemaker_info *spminfo, int procid) {

	struct sassy_process_info *cur_pinfo = &spminfo->tdata.pinfos[procid];

	cur_pinfo->pid 	=  procid & 0xFF;		/* procid must fit in one byte (aka <= 255) */
	cur_pinfo->ps 	=  1;					/* Init proc as running */

}

void sassy_pm_test_create_processes(struct sassy_pacemaker_info *spminfo, int num_of_proc){
	struct sassy_test_procfile_container **containers;
	int i;

	if(num_of_proc >= MAX_PROCESSES_PER_HOST) {
		sassy_error("Can not create %d processes - SASSY Proc Limit is %d", num_of_proc, MAX_PROCESSES_PER_HOST);
	}

	containers = kmalloc_array(num_of_proc, sizeof(struct sassy_test_procfile_container *), GFP_KERNEL);
	for(i=0; i<num_of_proc;i++){
		
		containers[i] = kmalloc(sizeof(struct sassy_test_procfile_container),GFP_KERNEL);

		containers[i]->procid = i;
		containers[i]->spminfo = spminfo;

		sassy_pm_create_test_data(spminfo, i);
		sassy_pm_create_test_procfile(containers[i]);
	}

	spminfo->tdata.active_processes = num_of_proc;
}