#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/init.h>

#include <linux/kernel.h>

#include <sassy/sassy.h>
#include <sassy/logger.h>

static ssize_t sassy_hb_ctrl_proc_write(struct file *file,
					const char __user *buffer, size_t count,
					loff_t *data)
{
	int err;
	char kernel_buffer[count + 1];
	struct sassy_pacemaker_info *spminfo =
		(struct sassy_pacemaker_info *)PDE_DATA(file_inode(file));
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
	struct sassy_pacemaker_info *spminfo =
		(struct sassy_pacemaker_info *)m->private;

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
	return single_open(file, sassy_hb_ctrl_proc_show,
			   PDE_DATA(file_inode(file)));
}

static ssize_t sassy_cpumgmt_write(struct file *file,
				   const char __user *user_buffer, size_t count,
				   loff_t *data)
{
	int err;
	char kernel_buffer[SASSY_NUMBUF];
	int tocpu = -1;
	struct sassy_pacemaker_info *spminfo =
		(struct sassy_pacemaker_info *)PDE_DATA(file_inode(file));
	size_t size;

	if (!spminfo)
		return -ENODEV;

	size = min(sizeof(kernel_buffer) - 1, count);

	sassy_dbg(" Write init count=%lu %s\n", count, __FUNCTION__);
	memset(kernel_buffer, 0, sizeof(kernel_buffer));

	err = copy_from_user(kernel_buffer, user_buffer, count);
	if (err) {
		sassy_error(" Copy from user failed%s\n", __FUNCTION__);
		goto error;
	}

	kernel_buffer[size] = '\0';
	err = kstrtoint(kernel_buffer, 10, &tocpu);
	if (err) {
		sassy_error(
			" Error converting input buffer: %s, tocpu: 0x%x \n",
			kernel_buffer, tocpu);
		goto error;
	}

	if (tocpu > MAX_CPU_NUMBER || tocpu < 0)
		return -ENODEV;

	spminfo->active_cpu = tocpu;
	pm_state_transition_to(spminfo, SASSY_PM_READY);

	return count;
error:
	sassy_error(" Could not set cpu.%s\n", __FUNCTION__);
	return err;
}

static int sassy_cpumgmt_show(struct seq_file *m, void *v)
{
	struct sassy_pacemaker_info *spminfo =
		(struct sassy_pacemaker_info *)m->private;

	if (!spminfo)
		return -ENODEV;

	seq_printf(m, "%d\n", spminfo->active_cpu);
	return 0;
}

static int sassy_cpumgmt_open(struct inode *inode, struct file *file)
{
	return single_open(file, sassy_cpumgmt_show,
			   PDE_DATA(file_inode(file)));
}

static ssize_t sassy_payload_write(struct file *file,
				   const char __user *user_buffer, size_t count,
				   loff_t *data)
{
	int ret = 0;
	char kernel_buffer[SASSY_TARGETS_BUF];
	struct sassy_pacemaker_info *spminfo =
		(struct sassy_pacemaker_info *)PDE_DATA(file_inode(file));
	size_t size = min(sizeof(kernel_buffer) - 1, count);
	const char delimiters[] = " ,;";
	char *input_str;
	char *search_str;
	int i = 0;
	int hb_active_ix;

	if (!spminfo) {
		sassy_error("spminfo is NULL.\n");
		ret = -ENODEV;
		goto out;
	}

	memset(kernel_buffer, 0, sizeof(kernel_buffer));

	ret = copy_from_user(kernel_buffer, user_buffer, count);
	if (ret) {
		sassy_error(" Copy from user failed%s\n", __FUNCTION__);
		goto out;
	}

	kernel_buffer[size] = '\0';

	search_str = kstrdup(kernel_buffer, GFP_KERNEL);
	while ((input_str = strsep(&search_str, delimiters)) != NULL) {
		sassy_dbg(" reading: %s", input_str);

		if (strcmp(input_str, "") == 0 || strlen(input_str) == 0)
			break;

		if (i >= MAX_REMOTE_SOURCES) {
			sassy_error(
				" exceeded max of remote targets %d >= %d \n",
				i, MAX_REMOTE_SOURCES);
			break;
		}

		// TODO: Parse more input to pkt_payload struct.
		//		 Since this is only a test tool, prio for this task is low.

		// invert 0<->1 (and make sure {0,1} is the only possible input)
		//hb_active_ix = !!!(spminfo->pm_targets[i].pkt_data.hb_active_ix);
		//spminfo->pm_targets[i].pkt_data.pkt_payload[hb_active_ix].message = input_str[0] & 0xFF;
		//spminfo->pm_targets[i].pkt_data.hb_active_ix = !!!(spminfo->pm_targets[i].pkt_data.hb_active_ix);

		//sassy_dbg(" payload message: %02X\n", input_str[0] & 0xFF);
		i++;
	}

out:
	return count;
}

static int sassy_payload_show(struct seq_file *m, void *v)
{
	struct sassy_pacemaker_info *spminfo =
		(struct sassy_pacemaker_info *)m->private;
	int i;
	char *current_payload = kmalloc(16, GFP_KERNEL);
	char *current_ip =
		kmalloc(16, GFP_KERNEL); /* strlen of 255.255.255.255 is 15*/
	int ret = 0;

	if (!spminfo) {
		sassy_error("spminfo is NULL.\n");
		ret = -ENODEV;
		goto out;
	}

	for (i = 0; i < spminfo->num_of_targets; i++) {
		sassy_hex_to_ip(current_ip,
				spminfo->pm_targets[i].pkt_data.dst_ip);
		seq_printf(m, "%s: \n", current_ip);
		seq_hex_dump(m, "	", DUMP_PREFIX_OFFSET, 32, 1,
			     spminfo->pm_targets[i].pkt_data.pkt_payload,
			     SASSY_PAYLOAD_BYTES, false);
	}

out:
	kfree(current_ip);
	kfree(current_payload);

	return 0;
}

static int sassy_payload_open(struct inode *inode, struct file *file)
{
	return single_open(file, sassy_payload_show,
			   PDE_DATA(file_inode(file)));
}

static ssize_t sassy_target_write(struct file *file,
				  const char __user *user_buffer, size_t count,
				  loff_t *data)
{
	int err;
	char kernel_buffer[SASSY_TARGETS_BUF];
	char *search_str;
	struct sassy_pacemaker_info *spminfo =
		(struct sassy_pacemaker_info *)PDE_DATA(file_inode(file));
	struct sassy_device *sdev;
	size_t size = min(sizeof(kernel_buffer) - 1, count);
	char *input_str;
	const char delimiters[] = " ,;()";
	int i = 0;
	int state = 0; /* first element of tuple is ip address, second is mac */
	int current_ip;
	unsigned char *current_mac;
	int current_protocol;

	if (!spminfo)
		return -ENODEV;

	sdev = container_of(spminfo, struct sassy_device, pminfo);

	if (!sdev) {
		sassy_error(" Could not find sassy device!\n");
		return -ENODEV;
	}

	memset(kernel_buffer, 0, sizeof(kernel_buffer));

	err = copy_from_user(kernel_buffer, user_buffer, count);
	if (err) {
		sassy_error(" Copy from user failed%s\n", __FUNCTION__);
		goto error;
	}

	kernel_buffer[size] = '\0';
	if (spminfo->num_of_targets < 0 ||
	    spminfo->num_of_targets > SASSY_TARGETS_BUF) {
		sassy_error(" num_of_targets is invalid! \n");
		return -EINVAL;
	}
	
	i = 0;
	search_str = kstrdup(kernel_buffer, GFP_KERNEL);
	while ((input_str = strsep(&search_str, delimiters)) != NULL) {
		sassy_dbg(" reading: %s", input_str);
		if (strcmp(input_str, "") == 0 || strlen(input_str) <= 0)
			continue;
		if (i > SASSY_TARGETS_BUF) {
			sassy_error(
				" Target buffer full! Not all targets are applied, increase buffer in sassy source.\n");
			break;
		}
		if (state == 0) {
			current_ip = sassy_ip_convert(input_str);
			if (current_ip == -EINVAL) {
				sassy_error(" Error formating IP address. %s\n",
					    __FUNCTION__);
				return -EINVAL;
			}
			sassy_dbg(" ip: %s\n", input_str);
			state = 1;
		} else if (state == 1) {
			current_mac = sassy_convert_mac(input_str);
			if (!current_mac) {
				sassy_error(
					" Invalid MAC. Failed to convert to byte string.\n");
				return -EINVAL;
			}
			sassy_dbg(" mac: %s\n", input_str);
			state = 2;

		} else if (state == 2) {
			err = kstrtoint(input_str, 10, &current_protocol);
			if (err) {
				sassy_error(
					" Error converting input buffer: %s\n",
					input_str);
				goto error;
			}

			sassy_dbg(" protocol: %d\n", current_protocol);

			sassy_core_register_remote_host(sdev->sassy_id,
							current_ip, current_mac,
							current_protocol);

			i++;
			state = 0;
			kfree(current_mac);
		}
	}
	spminfo->num_of_targets = i;
	sassy_dbg("Number of targets is now: %d\n", i);

	return count;
error:
	sassy_error(" Error during parsing of input.%s\n", __FUNCTION__);
	return err;
}

static int sassy_target_show(struct seq_file *m, void *v)
{
	struct sassy_pacemaker_info *spminfo =
		(struct sassy_pacemaker_info *)m->private;
	int i;
	char *current_ip =
		kmalloc(16, GFP_KERNEL); /* strlen of 255.255.255.255 is 15*/

	if (!spminfo)
		return -ENODEV;

	for (i = 0; i < spminfo->num_of_targets; i++) {
		sassy_hex_to_ip(current_ip,
				spminfo->pm_targets[i].pkt_data.dst_ip);
		seq_printf(m, "(%s,", current_ip);
		seq_printf(m, "%x:%x:%x:%x:%x:%x)\n",
			   spminfo->pm_targets[i].pkt_data.dst_mac[0],
			   spminfo->pm_targets[i].pkt_data.dst_mac[1],
			   spminfo->pm_targets[i].pkt_data.dst_mac[2],
			   spminfo->pm_targets[i].pkt_data.dst_mac[3],
			   spminfo->pm_targets[i].pkt_data.dst_mac[4],
			   spminfo->pm_targets[i].pkt_data.dst_mac[5]);
	}
	kfree(current_ip);

	return 0;
}

static int sassy_target_open(struct inode *inode, struct file *file)
{
	return single_open(file, sassy_target_show, PDE_DATA(file_inode(file)));
}

static ssize_t sassy_test_ctrl_write(struct file *file,
				     const char __user *user_buffer,
				     size_t count, loff_t *data)
{
	int err;
	char kernel_buffer[count + 1];
	struct sassy_pacemaker_info *spminfo =
		(struct sassy_pacemaker_info *)PDE_DATA(file_inode(file));
	long new_active_processes = -1;

	if (!spminfo)
		return -ENODEV;

	if (count == 0)
		return -EINVAL;

	err = copy_from_user(kernel_buffer, user_buffer, count);

	if (err) {
		sassy_error("Copy from user failed%s\n", __FUNCTION__);
		return err;
	}

	kernel_buffer[count] = '\0';

	err = kstrtol(kernel_buffer, 0, &new_active_processes);

	if (err) {
		sassy_error(" Error converting input%s\n", __FUNCTION__);
		return err;
	}

	sassy_dbg("created %d active user space\n", new_active_processes);
}

static int sassy_test_ctrl_show(struct seq_file *m, void *v)
{
	struct sassy_pacemaker_info *spminfo =
		(struct sassy_pacemaker_info *)m->private;
	int i;

	if (!spminfo)
		return -ENODEV;

	if (spminfo->tdata.state == SASSY_PM_TEST_UNINIT)
		seq_printf(m, "Not Initialized\n");
	else
		seq_printf(m, "active dummy user space processes: %d",
			   spminfo->tdata.active_processes);

	return 0;
}

static int sassy_test_ctrl_open(struct inode *inode, struct file *file)
{
	return single_open(file, sassy_test_ctrl_show,
			   PDE_DATA(file_inode(file)));
}

static const struct file_operations sassy_target_ops = {
	.owner = THIS_MODULE,
	.open = sassy_target_open,
	.write = sassy_target_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations sassy_hb_ctrl_ops = {
	.owner = THIS_MODULE,
	.open = sassy_hb_ctrl_proc_open,
	.write = sassy_hb_ctrl_proc_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations sassy_cpumgmt_ops = {
	.owner = THIS_MODULE,
	.open = sassy_cpumgmt_open,
	.write = sassy_cpumgmt_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations sassy_payload_ops = {
	.owner = THIS_MODULE,
	.open = sassy_payload_open,
	.write = sassy_payload_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations sassy_test_ctrl_ops = {
	.owner = THIS_MODULE,
	.open = sassy_test_ctrl_open,
	.write = sassy_test_ctrl_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

void init_sassy_pm_ctrl_interfaces(struct sassy_device *sdev)
{
	char name_buf[MAX_SASSY_PROC_NAME];


	snprintf(name_buf, sizeof name_buf, "sassy/%d/pacemaker",
		 sdev->ifindex);
	proc_mkdir(name_buf, NULL);

	snprintf(name_buf, sizeof name_buf, "sassy/%d/pacemaker/payload",
		 sdev->ifindex);
	proc_create_data(name_buf, S_IRWXU | S_IRWXO, NULL, &sassy_payload_ops,
			 &sdev->pminfo);

	snprintf(name_buf, sizeof name_buf, "sassy/%d/pacemaker/ctrl",
		 sdev->ifindex);
	proc_create_data(name_buf, S_IRWXU | S_IRWXO, NULL, &sassy_hb_ctrl_ops,
			 &sdev->pminfo);

	snprintf(name_buf, sizeof name_buf, "sassy/%d/pacemaker/targets",
		 sdev->ifindex);
	proc_create_data(name_buf, S_IRWXU | S_IRWXO, NULL, &sassy_target_ops,
			 &sdev->pminfo);

	snprintf(name_buf, sizeof name_buf, "sassy/%d/pacemaker/cpu",
		 sdev->ifindex);
	proc_create_data(name_buf, S_IRWXU | S_IRWXO, NULL, &sassy_cpumgmt_ops,
			 &sdev->pminfo);

	sassy_dbg("Pacemaker ctrl interfaces created for device (%d)\n",
		  sdev->ifindex);
}
EXPORT_SYMBOL(init_sassy_pm_ctrl_interfaces);

void clean_sassy_pm_ctrl_interfaces(struct sassy_device *sdev)
{
	char name_buf[MAX_SASSY_PROC_NAME];

	snprintf(name_buf, sizeof name_buf, "sassy/%d/pacemaker/payload",
		 sdev->ifindex);
	remove_proc_entry(name_buf, NULL);

	snprintf(name_buf, sizeof name_buf, "sassy/%d/pacemaker/cpu",
		 sdev->ifindex);
	remove_proc_entry(name_buf, NULL);

	snprintf(name_buf, sizeof name_buf, "sassy/%d/pacemaker/ctrl",
		 sdev->ifindex);
	remove_proc_entry(name_buf, NULL);

	snprintf(name_buf, sizeof name_buf, "sassy/%d/pacemaker/targets",
		 sdev->ifindex);
	remove_proc_entry(name_buf, NULL);

	snprintf(name_buf, sizeof name_buf, "sassy/%d/pacemaker",
		 sdev->ifindex);
	remove_proc_entry(name_buf, NULL);
}
EXPORT_SYMBOL(clean_sassy_pm_ctrl_interfaces);
