#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>

#include <sassy/sassy.h>
#include <sassy/sassy_dev.h>
#include <sassy/sassy_hb.h>
#include <sassy/sassy_net.h>
#include <sassy/sassy_pm.h>


static ssize_t sassy_hb_ctrl_proc_write(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	int err;
	char kernel_buffer[count+1];
	struct sassy_pacemaker_info *spminfo = (struct sassy_pacemaker_info*)PDE_DATA(file_inode(file));
	long new_hb_state = -1;

	if (!spminfo)
		return -ENODEV;

	if (count == 0) {
		err = -EINVAL;
		goto error;
	}

	err = copy_from_user(kernel_buffer, buffer, count);

	if (err) {
		sassy_error("Copy from user failed%s\n", __FUNCTION__);
		goto error;
	}

	kernel_buffer[count] = '\0';

	printk(KERN_INFO "Run: %s\n", kernel_buffer);
	err = kstrtol(kernel_buffer, 0, &new_hb_state);

	if (err) {
		sassy_error(" Error converting input%s\n", __FUNCTION__);
		goto error;
	}

	switch (new_hb_state) {
	case 0:
		sassy_dbg(" Stop Heartbeat thread\n");
		sassy_pm_stop(spminfo);
		break;
	case 1:
		sassy_dbg(" Start Heartbeat thread\n");
		sassy_pm_start(spminfo);
		break;
	case 2: 
		sassy_dbg(" Reset Kernel Configuration\n");
		sassy_pm_reset(spminfo);
        break;
	default:
		sassy_error(" Unknown action!\n");
		break;
	}

	sassy_dbg(" Heartbeat state changed successfully.%s\n", __FUNCTION__);
	return count;
error:
	sassy_error(" Heartbeat control failed.%s\n", __FUNCTION__);
	return err;
}

static int sassy_hb_ctrl_proc_show(struct seq_file *m, void *v)
{
	struct sassy_pacemaker_info *spminfo = (struct sassy_pacemaker_info*) m->private;

	if (!spminfo)
		return -ENODEV;

	if (spminfo->state == SASSY_PM_EMITTING)
		seq_puts(m, "emitting");
	else if (spminfo->state == SASSY_PM_UNINIT)
		seq_puts(m, "uninit");
	else if (spminfo->state == SASSY_PM_READY)
		seq_puts(m, "ready");

	return 0;
}

static int sassy_hb_ctrl_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, sassy_hb_ctrl_proc_show,PDE_DATA(file_inode(file)));
}

static ssize_t sassy_cpumgmt_write(struct file *file, const char __user *user_buffer, size_t count, loff_t *data)
{
	int err;
	char kernel_buffer[SYNCBEAT_NUMBUF];
	int tocpu = -1;
	struct sassy_device *sdev = (struct sassy_device*)PDE_DATA(file_inode(file));
	size_t size;

	if (!sdev)
		return -ENODEV;

	size = min(sizeof(kernel_buffer) - 1, count);

	sassy_error(" Write init count=%lu %s\n", count, __FUNCTION__);
	memset(kernel_buffer, 0, sizeof(kernel_buffer));

	err = copy_from_user(kernel_buffer, user_buffer, count);
	if (err) {
		sassy_error(" Copy from user failed%s\n", __FUNCTION__);
		goto error;
	}

	kernel_buffer[size] = '\0';
	err = kstrtoint(kernel_buffer, 10, &tocpu);
	if (err) {
		sassy_error(" Error converting input buffer: %s, tocpu: 0x%x \n", kernel_buffer, tocpu);
		goto error;
	}

	if (tocpu > MAX_CPU_NUMBER || tocpu < 0)
		return -ENODEV;


	sdev->active_cpu = tocpu;

	return count;
error:
	sassy_error(" Could not set cpu.%s\n", __FUNCTION__);
	return err;
}

static int sassy_cpumgmt_show(struct seq_file *m, void *v)
{
	struct sassy_device *spminfo = (struct sassy_device*) m->private;

	if (!spminfo)
		return -ENODEV;

	seq_printf(m, "%d\n", spminfo->active_cpu);
	return 0;
}

static int sassy_cpumgmt_open(struct inode *inode, struct file *file)
{
	return single_open(file, sassy_cpumgmt_show,PDE_DATA(file_inode(file)));
}

static ssize_t sassy_target_write(struct file *file, const char __user *user_buffer, size_t count, loff_t *data)
{
	int err;
	char kernel_buffer[SYNCBEAT_TARGETS_BUF];
	char *search_str;
	struct sassy_pacemaker_info *spminfo =  (struct sassy_pacemaker_info*) PDE_DATA(file_inode(file));
	size_t size = min(sizeof(kernel_buffer) - 1, count);
	char *input_str;
	const char delimiters[] = " ,;()";
	int i = 0;
	int read_ip = 1; /* first element of tuple is ip address, second is mac */

	if (!sdev) 
		return -ENODEV;

	memset(kernel_buffer, 0, sizeof(kernel_buffer));

	err = copy_from_user(kernel_buffer, user_buffer, count);
	if (err) {
		sassy_error(" Copy from user failed%s\n", __FUNCTION__);
		goto error;
	}

	kernel_buffer[size] = '\0';
	if (spminfo->num_of_targets < 0 || spminfo->num_of_targets > SYNCBEAT_TARGETS_BUF){
		sassy_error(" num_of_targets is invalid! \n");
		return -EINVAL;
	}
	i = spminfo->num_of_targets; /* just append */
	search_str = kstrdup(kernel_buffer, GFP_KERNEL);
	while ((input_str = strsep(&search_str, delimiters)) != NULL) {
		sassy_dbg(" reading: %s", input_str);
		if(strcmp(input_str, "") == 0 || strlen(input_str) <= 2)
			continue;
		if(i  > SYNCBEAT_TARGETS_BUF){
			sassy_error(" Target buffer full! Not all targets are applied, increase buffer in sassy source.\n");
			break;
		}
		if(read_ip) {
			spminfo->targets[i].dst_ip = sassy_ip_convert(input_str);
			if (spminfo->targets[i].dst_ip == -EINVAL) {
				sassy_error(" Error formating IP address. %s\n",__FUNCTION__);
				return -EINVAL;
			}
			sassy_dbg(" ip: %s\n", input_str);
			read_ip = 0;
		}else {
			spminfo->targets[i].dst_mac = sassy_convert_mac(input_str);
			if (!spminfo->targets[i].dst_mac) {
				sassy_error(" Invalid MAC. Failed to convert to byte string.\n");
				err = -EINVAL;
				return -ENODEV;
			}
			sassy_dbg(" mac: %s\n", input_str);
			read_ip = 1;
			i++;
		}
	}
	spminfo->num_of_targets = i;

	return count;
error:
	sassy_error("Setting IP destination for heartbeat failed.%s\n", __FUNCTION__);
	return err;
}

static int sassy_target_show(struct seq_file *m, void *v)
{
	struct sassy_pacemaker_info *spminfo = (struct sassy_pacemaker_info*) m->private;
	int i;
	char *current_ip = kmalloc(16, GFP_KERNEL); /* strlen of 255.255.255.255 is 15*/

	if (!spminfo)
		return -ENODEV;

	for(i = 0; i < spminfo->num_of_targets; i++){
		sassy_hex_to_ip(current_ip, spminfo->targets[i].dst_ip);
		seq_printf(m, "(%s,", current_ip );
		seq_printf(m, "%x:%x:%x:%x:%x:%x)\n", 
			spminfo->targets[i].dst_mac[0], spminfo->targets[i].dst_mac[1],
			spminfo->targets[i].dst_mac[2], spminfo->targets[i].dst_mac[3],
			spminfo->targets[i].dst_mac[4], spminfo->targets[i].dst_mac[5]);
	}
	kfree(current_ip);

	return 0;
}

static int sassy_target_open(struct inode *inode, struct file *file)
{
	return single_open(file, sassy_target_show,PDE_DATA(file_inode(file)));
}


static const struct file_operations sassy_target_ops = {
		.owner	= THIS_MODULE,
		.open	= sassy_target_open,
		.write	= sassy_target_write,
		.read	= seq_read,
		.llseek	= seq_lseek,
		.release = single_release,
};

static const struct file_operations sassy_hb_ctrl_ops = {
	.owner	= THIS_MODULE,
	.open	= sassy_hb_ctrl_proc_open,
	.write	= sassy_hb_ctrl_proc_write,
	.read	= seq_read,
	.llseek	= seq_lseek,
	.release = single_release,
};

static const struct file_operations sassy_cpumgmt_ops = {
		.owner	= THIS_MODULE,
		.open	= sassy_cpumgmt_open,
		.write	= sassy_cpumgmt_write,
		.read	= seq_read,
		.llseek	= seq_lseek,
		.release = single_release,
};

void init_sassy_pm_ctrl_interfaces(struct sassy_device *sdev)
{
	char name_buf[MAX_SYNCBEAT_PROC_NAME];

	snprintf(name_buf,  sizeof name_buf, "sassy/%d/pacemaker", sdev->ifindex);
	proc_mkdir(name_buf, NULL);

	snprintf(name_buf, sizeof name_buf, "sassy/%d/pacemaker/ctrl", sdev->ifindex);
	proc_create_data(name_buf, S_IRWXU|S_IRWXO, NULL, &sassy_hb_ctrl_ops, sdev->pminfo);

	snprintf(name_buf, sizeof name_buf, "sassy/%d/pacemaker/targets", sdev->ifindex);
	proc_create_data(name_buf, S_IRWXU|S_IRWXO, NULL, &sassy_target_ops, sdev->pminfo);

	snprintf(name_buf, sizeof name_buf, "sassy/%d/pacemaker/cpu", sdev->ifindex);
	proc_create_data(name_buf, S_IRWXU|S_IRWXO, NULL, &sassy_cpumgmt_ops, sdev->pminfo);

	sassy_dbg("Pacemaker ctrl interfaces created for device (%d)\n", sdev->ifindex);
}
EXPORT_SYMBOL(init_sassy_pm_ctrl_interfaces);