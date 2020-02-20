#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/init.h>

#include <linux/kthread.h>
#include <linux/sched.h>

#include <linux/kernel.h>

#include <asguard/asguard.h>
#include <asguard/logger.h>

static ssize_t asguard_hb_ctrl_proc_write(struct file *file,
					const char __user *buffer, size_t count,
					loff_t *data)
{
	int err;
	char kernel_buffer[MAX_PROCFS_BUF];
	struct pminfo *spminfo =
		(struct pminfo *)PDE_DATA(file_inode(file));
	long new_hb_state = -1;

	if (!spminfo)
		return -ENODEV;

	if (count == 0) {
		err = -EINVAL;
		goto error;
	}

	err = copy_from_user(kernel_buffer, buffer, count);

	if (err) {
		asguard_error("Copy from user failed%s\n", __func__);
		goto error;
	}

	kernel_buffer[count] = '\0';

	err = kstrtol(kernel_buffer, 0, &new_hb_state);

	if (err) {
		asguard_error("Error converting input%s\n", __func__);
		goto error;
	}

	switch (new_hb_state) {
	case 0:
		asguard_pm_stop(spminfo);
		break;
	case 1:
		asguard_pm_start_loop(spminfo);
		break;
	case 2:
		asguard_pm_reset(spminfo);
		break;
	default:
		asguard_error("Unknown action!\n");
		err = -EINVAL;
		goto error;
	}

	return count;
error:
	asguard_error("Heartbeat control failed.%s\n", __func__);
	return err;
}

static int asguard_hb_ctrl_proc_show(struct seq_file *m, void *v)
{
	struct pminfo *spminfo =
		(struct pminfo *)m->private;

	if (!spminfo)
		return -ENODEV;

	if (spminfo->state == ASGUARD_PM_EMITTING)
		seq_puts(m, "emitting");
	else if (spminfo->state == ASGUARD_PM_UNINIT)
		seq_puts(m, "uninit");
	else if (spminfo->state == ASGUARD_PM_READY)
		seq_puts(m, "ready");

	return 0;
}

static int asguard_hb_ctrl_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, asguard_hb_ctrl_proc_show,
			   PDE_DATA(file_inode(file)));
}

static ssize_t asguard_cpumgmt_write(struct file *file,
				   const char __user *user_buffer, size_t count,
				   loff_t *data)
{
	int err;
	char kernel_buffer[ASGUARD_NUMBUF];
	int tocpu = -1;
	struct pminfo *spminfo =
		(struct pminfo *)PDE_DATA(file_inode(file));
	size_t size;

	if (!spminfo)
		return -ENODEV;

	size = min(sizeof(kernel_buffer) - 1, count);

	memset(kernel_buffer, 0, sizeof(kernel_buffer));

	err = copy_from_user(kernel_buffer, user_buffer, count);
	if (err) {
		asguard_error("Copy from user failed%s\n", __func__);
		goto error;
	}

	kernel_buffer[size] = '\0';
	err = kstrtoint(kernel_buffer, 10, &tocpu);
	if (err) {
		asguard_error(
			"Error converting input buffer: %s, tocpu: 0x%x\n",
			kernel_buffer, tocpu);
		goto error;
	}

	if (tocpu > MAX_CPU_NUMBER || tocpu < 0)
		return -ENODEV;

	spminfo->active_cpu = tocpu;
	pm_state_transition_to(spminfo, ASGUARD_PM_READY);

	return count;
error:
	asguard_error("Could not set cpu.%s\n", __func__);
	return err;
}

static int asguard_cpumgmt_show(struct seq_file *m, void *v)
{
	struct pminfo *spminfo =
		(struct pminfo *)m->private;

	if (!spminfo)
		return -ENODEV;

	seq_printf(m, "%d\n", spminfo->active_cpu);
	return 0;
}

static int asguard_cpumgmt_open(struct inode *inode, struct file *file)
{
	return single_open(file, asguard_cpumgmt_show,
			   PDE_DATA(file_inode(file)));
}

static ssize_t asguard_hbi_write(struct file *file,
					const char __user *buffer, size_t count,
					loff_t *data)
{
	int err;
	char kernel_buffer[MAX_PROCFS_BUF];
	struct pminfo *spminfo =
		(struct pminfo *)PDE_DATA(file_inode(file));
	long new_hbi = -1;

	if (!spminfo)
		return -ENODEV;

	if (count == 0) {
		err = -EINVAL;
		goto error;
	}

	err = copy_from_user(kernel_buffer, buffer, count);

	if (err) {
		asguard_error("Copy from user failed%s\n", __func__);
		goto error;
	}

	kernel_buffer[count] = '\0';

	err = kstrtol(kernel_buffer, 0, &new_hbi);

	if (err) {
		asguard_error("Error converting input%s\n", __func__);
		goto error;
	}

	if ( new_hbi < MIN_HB_CYCLES || new_hbi > MAX_HB_CYCLES) {
		asguard_error("Invalid heartbeat interval! range is %d to %d, but got %ld",
					 MIN_HB_CYCLES, MAX_HB_CYCLES, new_hbi);
		err = -EINVAL;
		goto error;
	}

	spminfo->hbi = new_hbi;

	return count;
error:
	asguard_error("Heartbeat control failed.%s\n", __func__);
	return err;
}

static int asguard_hbi_show(struct seq_file *m, void *v)
{
	struct pminfo *spminfo =
		(struct pminfo *)m->private;

	if (!spminfo)
		return -ENODEV;

	seq_printf(m, "%llu\n", spminfo->hbi);
	return 0;
}

static int asguard_hbi_open(struct inode *inode, struct file *file)
{
	return single_open(file, asguard_hbi_show,
			   PDE_DATA(file_inode(file)));
}


static ssize_t asguard_ww_write(struct file *file,
					const char __user *buffer, size_t count,
					loff_t *data)
{
	int err;
	char kernel_buffer[MAX_PROCFS_BUF];
	struct pminfo *spminfo =
		(struct pminfo *)PDE_DATA(file_inode(file));
	long new_ww = -1;

	if (!spminfo)
		return -ENODEV;

	if (count == 0) {
		err = -EINVAL;
		goto error;
	}

	err = copy_from_user(kernel_buffer, buffer, count);

	if (err) {
		asguard_error("Copy from user failed%s\n", __func__);
		goto error;
	}

	kernel_buffer[count] = '\0';

	err = kstrtol(kernel_buffer, 0, &new_ww);

	if (err) {
		asguard_error("Error converting input %s\n", __func__);
		goto error;
	}

	spminfo->waiting_window = new_ww;

	return count;
error:
	asguard_error("Setting of waiting Window failed\n");
	return err;
}

static int asguard_ww_show(struct seq_file *m, void *v)
{
	struct pminfo *spminfo =
		(struct pminfo *)m->private;

	if (!spminfo)
		return -ENODEV;

	seq_printf(m, "%llu\n", spminfo->waiting_window);
	return 0;
}

static int asguard_ww_open(struct inode *inode, struct file *file)
{
	return single_open(file, asguard_ww_show,
			   PDE_DATA(file_inode(file)));
}


static ssize_t asguard_payload_write(struct file *file,
				   const char __user *user_buffer, size_t count,
				   loff_t *data)
{
	int ret = 0;
	char kernel_buffer[ASGUARD_TARGETS_BUF];
	struct pminfo *spminfo =
		(struct pminfo *)PDE_DATA(file_inode(file));
	size_t size = min(sizeof(kernel_buffer) - 1, count);
	static const char delimiters[] = " ,;";
	char *input_str;
	char *search_str;
	int i = 0;

	if (!spminfo) {
		asguard_error("spminfo is NULL.\n");
		ret = -ENODEV;
		goto out;
	}

	memset(kernel_buffer, 0, sizeof(kernel_buffer));

	ret = copy_from_user(kernel_buffer, user_buffer, count);
	if (ret) {
		asguard_error("Copy from user failed%s\n", __func__);
		goto out;
	}

	kernel_buffer[size] = '\0';

	search_str = kstrdup(kernel_buffer, GFP_KERNEL);
	while ((input_str = strsep(&search_str, delimiters)) != NULL) {
		if (strcmp(input_str, "") == 0 || strlen(input_str) == 0)
			break;

		if (i >= MAX_REMOTE_SOURCES) {
			asguard_error(
				"exceeded max of remote targets %d >= %d\n",
				i, MAX_REMOTE_SOURCES);
			break;
		}

		// TODO: Parse more input to pkt_payload struct.
		// Since this is only a test tool, prio for this task is low.
		// invert 0<->1 (and make sure {0,1} is the only possible input)
		// hb_active_ix = !!!(spminfo->pm_targets[i].pkt_data.hb_active_ix);
		// spminfo->pm_targets[i].pkt_data.pkt_payload[hb_active_ix].message = input_str[0] & 0xFF;
		// spminfo->pm_targets[i].pkt_data.hb_active_ix = !!!(spminfo->pm_targets[i].pkt_data.hb_active_ix);

		//asguard_dbg(" payload message: %02X\n", input_str[0] & 0xFF);
		i++;
	}

out:
	return count;
}

static int asguard_payload_show(struct seq_file *m, void *v)
{
	struct pminfo *spminfo =
		(struct pminfo *)m->private;
	int i;
    // freed in this function
	char *current_payload = kmalloc(16, GFP_KERNEL);
    // freed in this function
	char *current_ip =
		kmalloc(16, GFP_KERNEL); /* strlen of 255.255.255.255 is 15*/
	int ret = 0;

	if (!spminfo) {
		asguard_error("spminfo is NULL.\n");
		ret = -ENODEV;
		goto out;
	}

	for (i = 0; i < spminfo->num_of_targets; i++) {
		asguard_hex_to_ip(current_ip,
				spminfo->pm_targets[i].pkt_data.naddr.dst_ip);
		seq_printf(m, "%s:\n", current_ip);
		seq_hex_dump(m, "	", DUMP_PREFIX_OFFSET, 32, 1,
			     spminfo->pm_targets[i].pkt_data.pkt_payload,
			     ASGUARD_PAYLOAD_BYTES, false);
	}

out:
	kfree(current_ip);
	kfree(current_payload);

	return 0;
}

static int asguard_payload_open(struct inode *inode, struct file *file)
{
	return single_open(file, asguard_payload_show,
			   PDE_DATA(file_inode(file)));
}

static ssize_t asguard_target_write(struct file *file,
				  const char __user *user_buffer, size_t count,
				  loff_t *data)
{
	int err;
	char kernel_buffer[ASGUARD_TARGETS_BUF];
	char *search_str;
	struct pminfo *spminfo =
		(struct pminfo *)PDE_DATA(file_inode(file));
	struct asguard_device *sdev;
	size_t size = min(sizeof(kernel_buffer) - 1, count);
	char *input_str;
	static const char delimiters[] = " ,;()";
	int i = 0;
	int state = 0; /* first element of tuple is ip address, second is mac */
	u32 current_ip;
	unsigned char *current_mac = NULL;
	int current_protocol;
	int cluster_id;

	current_ip = -1;

	if (!spminfo)
		return -ENODEV;

	sdev = container_of(spminfo, struct asguard_device, pminfo);

	if (!sdev) {
		asguard_error(" Could not find asguard device!\n");
		return -ENODEV;
	}

	memset(kernel_buffer, 0, sizeof(kernel_buffer));

	err = copy_from_user(kernel_buffer, user_buffer, count);
	if (err) {
		asguard_error("Copy from user failed%s\n", __func__);
		goto error;
	}

	kernel_buffer[size] = '\0';
	if (spminfo->num_of_targets < 0 ||
	    spminfo->num_of_targets > ASGUARD_TARGETS_BUF) {
		asguard_error("num_of_targets is invalid! Have you set the target hosts?\n");
		return -EINVAL;
	}

	i = 0;
	sdev->pminfo.num_of_targets = 0;
	search_str = kstrdup(kernel_buffer, GFP_KERNEL);
	while ((input_str = strsep(&search_str, delimiters)) != NULL) {
		if (!input_str || strlen(input_str) <= 0)
			continue;
		if (i > ASGUARD_TARGETS_BUF) {
			asguard_error(
				"Target buffer full! Not all targets are applied, increase buffer in asguard source.\n");
			break;
		}
		if (state == 0) {
			current_ip = asguard_ip_convert(input_str);
			if (current_ip == -EINVAL) {
				asguard_error("Error formating IP address. %s\n",
					    __func__);
				return -EINVAL;
			}
			state = 1;
		} else if (state == 1) {
			current_mac = asguard_convert_mac(input_str);
			if (!current_mac) {
				asguard_error(
					"Invalid MAC. Failed to convert to byte string.\n");
				return -EINVAL;
			}
			state = 2;

		} else if (state == 2) {
			err = kstrtoint(input_str, 10, &current_protocol);
			if (err) {
				asguard_error(
					"Expected Porotocol Number. Error converting input buffer: %s\n",
					input_str);
				goto error;
			}



			state = 3;
		} else if (state == 3) {
			err = kstrtoint(input_str, 10, &cluster_id);

			if (is_ip_local(sdev->ndev, current_ip)) {
				sdev->cluster_id = cluster_id;
			} else {
				asguard_core_register_remote_host(sdev->asguard_id,
						current_ip, current_mac,
						current_protocol, cluster_id);
				i++;
			}
			state = 0;
			if (current_mac)
				kfree(current_mac);
		}
	}
	spminfo->num_of_targets = i;

	return count;
error:
    if (current_mac)
        kfree(current_mac);
	asguard_error("Error during parsing of input.%s\n", __func__);
	return err;
}

static int asguard_target_show(struct seq_file *m, void *v)
{
	struct pminfo *spminfo =
		(struct pminfo *)m->private;
	int i;
    // freed in this function
	char *current_ip =
		kmalloc(16, GFP_KERNEL); /* strlen of 255.255.255.255 is 15*/

	if (!spminfo)
		return -ENODEV;

	for (i = 0; i < spminfo->num_of_targets; i++) {
		asguard_hex_to_ip(current_ip,
				spminfo->pm_targets[i].pkt_data.naddr.dst_ip);
		seq_printf(m, "(%s,", current_ip);
		seq_printf(m, "%x:%x:%x:%x:%x:%x)\n",
			   spminfo->pm_targets[i].pkt_data.naddr.dst_mac[0],
			   spminfo->pm_targets[i].pkt_data.naddr.dst_mac[1],
			   spminfo->pm_targets[i].pkt_data.naddr.dst_mac[2],
			   spminfo->pm_targets[i].pkt_data.naddr.dst_mac[3],
			   spminfo->pm_targets[i].pkt_data.naddr.dst_mac[4],
			   spminfo->pm_targets[i].pkt_data.naddr.dst_mac[5]);
	}
	kfree(current_ip);

	return 0;
}

static int asguard_target_open(struct inode *inode, struct file *file)
{
	return single_open(file, asguard_target_show, PDE_DATA(file_inode(file)));
}

static ssize_t asguard_test_ctrl_write(struct file *file,
				     const char __user *user_buffer,
				     size_t count, loff_t *data)
{
	int err;
	char kernel_buffer[MAX_PROCFS_BUF];
	struct pminfo *spminfo =
		(struct pminfo *)PDE_DATA(file_inode(file));
	long new_active_processes = -1;

	if (!spminfo)
		return -ENODEV;

	if (count == 0)
		return -EINVAL;

	err = copy_from_user(kernel_buffer, user_buffer, count);

	if (err) {
		asguard_error("Copy from user failed%s\n", __func__);
		return err;
	}

	kernel_buffer[count] = '\0';

	err = kstrtol(kernel_buffer, 0, &new_active_processes);

	if (err) {
		asguard_error("Error converting input%s\n", __func__);
		return err;
	}

	asguard_dbg("created %ld active user space\n", new_active_processes);
}

static int asguard_test_ctrl_show(struct seq_file *m, void *v)
{
	struct pminfo *spminfo =
		(struct pminfo *)m->private;

	if (!spminfo)
		return -ENODEV;

	if (spminfo->tdata.state == ASGUARD_PM_TEST_UNINIT)
		seq_printf(m, "Not Initialized\n");
	else
		seq_printf(m, "active dummy user space processes: %d",
			   spminfo->tdata.active_processes);

	return 0;
}

static int asguard_test_ctrl_open(struct inode *inode, struct file *file)
{
	return single_open(file, asguard_test_ctrl_show,
			   PDE_DATA(file_inode(file)));
}

static const struct file_operations asguard_target_ops = {
	.owner = THIS_MODULE,
	.open = asguard_target_open,
	.write = asguard_target_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations asguard_hb_ctrl_ops = {
	.owner = THIS_MODULE,
	.open = asguard_hb_ctrl_proc_open,
	.write = asguard_hb_ctrl_proc_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations asguard_cpumgmt_ops = {
	.owner = THIS_MODULE,
	.open = asguard_cpumgmt_open,
	.write = asguard_cpumgmt_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations asguard_hbi_ops = {
	.owner = THIS_MODULE,
	.open = asguard_hbi_open,
	.write = asguard_hbi_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations asguard_ww_ops = {
	.owner = THIS_MODULE,
	.open = asguard_ww_open,
	.write = asguard_ww_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations asguard_payload_ops = {
	.owner = THIS_MODULE,
	.open = asguard_payload_open,
	.write = asguard_payload_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations asguard_test_ctrl_ops = {
	.owner = THIS_MODULE,
	.open = asguard_test_ctrl_open,
	.write = asguard_test_ctrl_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

void init_asguard_pm_ctrl_interfaces(struct asguard_device *sdev)
{
	char name_buf[MAX_ASGUARD_PROC_NAME];

	snprintf(name_buf, sizeof(name_buf), "asguard/%d/pacemaker",
		 sdev->ifindex);
	proc_mkdir(name_buf, NULL);

	snprintf(name_buf, sizeof(name_buf), "asguard/%d/pacemaker/payload",
		 sdev->ifindex);
	proc_create_data(name_buf, S_IRWXU | S_IRWXO, NULL, &asguard_payload_ops,
			 &sdev->pminfo);

	snprintf(name_buf, sizeof(name_buf), "asguard/%d/pacemaker/ctrl",
		 sdev->ifindex);
	proc_create_data(name_buf, S_IRWXU | S_IRWXO, NULL, &asguard_hb_ctrl_ops,
			 &sdev->pminfo);

	snprintf(name_buf, sizeof(name_buf), "asguard/%d/pacemaker/hbi",
		 sdev->ifindex);

	proc_create_data(name_buf, S_IRWXU | S_IRWXO, NULL, &asguard_hbi_ops,
			 &sdev->pminfo);

	snprintf(name_buf, sizeof(name_buf), "asguard/%d/pacemaker/waiting_window",
		 sdev->ifindex);

	proc_create_data(name_buf, S_IRWXU | S_IRWXO, NULL, &asguard_ww_ops,
			 &sdev->pminfo);

	snprintf(name_buf, sizeof(name_buf), "asguard/%d/pacemaker/targets",
		 sdev->ifindex);
	proc_create_data(name_buf, S_IRWXU | S_IRWXO, NULL, &asguard_target_ops,
			 &sdev->pminfo);

	snprintf(name_buf, sizeof(name_buf), "asguard/%d/pacemaker/cpu",
		 sdev->ifindex);
	proc_create_data(name_buf, S_IRWXU | S_IRWXO, NULL, &asguard_cpumgmt_ops,
			 &sdev->pminfo);


}
EXPORT_SYMBOL(init_asguard_pm_ctrl_interfaces);

void clean_asguard_pm_ctrl_interfaces(struct asguard_device *sdev)
{
	char name_buf[MAX_ASGUARD_PROC_NAME];

	snprintf(name_buf, sizeof(name_buf), "asguard/%d/pacemaker/payload",
		 sdev->ifindex);
	remove_proc_entry(name_buf, NULL);

	snprintf(name_buf, sizeof(name_buf), "asguard/%d/pacemaker/cpu",
		 sdev->ifindex);
	remove_proc_entry(name_buf, NULL);

	snprintf(name_buf, sizeof(name_buf), "asguard/%d/pacemaker/ctrl",
		 sdev->ifindex);
	remove_proc_entry(name_buf, NULL);

	snprintf(name_buf, sizeof(name_buf), "asguard/%d/pacemaker/hbi",
		 sdev->ifindex);
	remove_proc_entry(name_buf, NULL);

	snprintf(name_buf, sizeof(name_buf), "asguard/%d/pacemaker/waiting_window",
		 sdev->ifindex);
	remove_proc_entry(name_buf, NULL);

	snprintf(name_buf, sizeof(name_buf), "asguard/%d/pacemaker/targets",
		 sdev->ifindex);
	remove_proc_entry(name_buf, NULL);

	snprintf(name_buf, sizeof(name_buf), "asguard/%d/pacemaker",
		 sdev->ifindex);
	remove_proc_entry(name_buf, NULL);
}
EXPORT_SYMBOL(clean_asguard_pm_ctrl_interfaces);
