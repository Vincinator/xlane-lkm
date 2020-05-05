#include "asgard_ping_ctrl.h"


#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include <asguard/asguard.h>
#include <asguard/logger.h>
#include <asguard/consensus.h>

#include <asguard/asguard_echo.h>


static int get_remote_lid(struct pminfo *spminfo, int cluster_id)
{
    int i;

    for(i = 0; i < spminfo->num_of_targets; i++) {

        if (spminfo->pm_targets[i].pkt_data.naddr.cluster_id == cluster_id) {
            return i;
        }
    }

    return -1;
}

int read_user_input_int(const char *user_buffer, size_t count, const struct echo_priv *priv,
                        char *kernel_buffer, int *target_cluster_id) {

    int err;
    size_t size;


    size = min(sizeof(kernel_buffer) - 1, count);

    memset(kernel_buffer, 0, sizeof(kernel_buffer));

    err = copy_from_user(kernel_buffer, user_buffer, count);

    if (err) {
        asguard_error("Copy from user failed%s\n", __func__);
        return -1;
    }

    kernel_buffer[size] = '\0';

    err = kstrtoint(kernel_buffer, 10, target_cluster_id);

    if (err) {
        asguard_dbg("Error converting input buffer: %s, todevid: 0x%x\n",
                    kernel_buffer, (*target_cluster_id));
        return -1;
    }

    return 0;
}



int read_pingpong_user_input(const char *user_buffer, size_t count, const struct echo_priv *priv,
                              char *kernel_buffer,  int *target_cluster_id,
                              int *remote_lid)
{
    int err;

    err = read_user_input_int(user_buffer, count, priv, kernel_buffer, target_cluster_id);

    if(err){
        asguard_error("Could no read user input\n");
        return -1;
    }

    /* TODO: trigger unicast ping to target. Add -1 case to ping all cluster nodes */

    if((*target_cluster_id) < 0 || (*target_cluster_id) > MAX_NODE_ID){
        asguard_error("Invalid Cluster ID: %d", (*target_cluster_id));
        return -1;

    }

    (*remote_lid) = get_remote_lid(&priv->sdev->pminfo, (*target_cluster_id));


    if((*remote_lid) < 0){
        asguard_error("could not find local data for cluster node %d\n", (*target_cluster_id));
        return -1;
    }

    return 0;
}



static ssize_t asguard_pupu_ctrl_write(struct file *file,
                                       const char __user *user_buffer, size_t count,
                                       loff_t *data)
{
    struct echo_priv *priv =
            (struct echo_priv *)PDE_DATA(file_inode(file));
    char kernel_buffer[ASGUARD_NUMBUF];
    int target_cluster_id = -3;
    int err;
    int remote_lid;

    err = read_pingpong_user_input(user_buffer, count, priv, kernel_buffer,&target_cluster_id, &remote_lid);

    if(err)
        goto error;

    setup_echo_msg_uni(priv->ins, &priv->sdev->pminfo, remote_lid,
                       priv->sdev->pminfo.cluster_id, target_cluster_id,
                       RDTSC_ASGUARD, 0, 0, ASGUARD_PING_REQ_UNI);

    /* Use Echo Protocol Port for ping */
    priv->sdev->pminfo.pm_targets[remote_lid].pkt_data.port = 3321;

    return count;
error:
    asguard_error("Error during parsing of input.%s\n", __func__);
    return err;
}

static int asguard_ping_ctrl_show(struct seq_file *m, void *v)
{
    struct echo_priv *priv =
            (struct echo_priv *)m->private;

    if (!priv)
        return -ENODEV;

    //seq_printf(m, "%pUB\n", priv->uuid.b);

    return 0;
}

static int asguard_pupu_ctrl_open(struct inode *inode, struct file *file)
{
    return single_open(file, asguard_ping_ctrl_show,
                       PDE_DATA(file_inode(file)));
}

static ssize_t asguard_pmpm_ctrl_write(struct file *file,
                                       const char __user *user_buffer, size_t count,
                                       loff_t *data)
{
    struct echo_priv *priv =
            (struct echo_priv *)PDE_DATA(file_inode(file));


    setup_echo_msg_multi(priv->ins, &priv->sdev->pminfo,
                         priv->sdev->pminfo.cluster_id, 0,
                         RDTSC_ASGUARD, 0, 0, ASGUARD_PING_REQ_MULTI);
    priv->fire_ping = 1;
    return count;
}

static int asguard_pmpm_show(struct seq_file *m, void *v)
{
    struct echo_priv *priv =
            (struct echo_priv *)m->private;

    if (!priv)
        return -ENODEV;

    //seq_printf(m, "%pUB\n", priv->uuid.b);

    return 0;
}

static int asguard_pmpm_ctrl_open(struct inode *inode, struct file *file)
{
    return single_open(file, asguard_pmpm_show,
                       PDE_DATA(file_inode(file)));
}

static ssize_t asguard_pmpu_ctrl_write(struct file *file,
                                       const char __user *user_buffer, size_t count,
                                       loff_t *data)
{
    struct echo_priv *priv =
            (struct echo_priv *)PDE_DATA(file_inode(file));
    char kernel_buffer[ASGUARD_NUMBUF];
    int target_cluster_id = -3;
    int err;
    int remote_lid;

    err = read_pingpong_user_input(user_buffer, count, priv, kernel_buffer,&target_cluster_id, &remote_lid);

    if(err)
        goto error;

    setup_echo_msg_multi(priv->ins, &priv->sdev->pminfo,
                       priv->sdev->pminfo.cluster_id, target_cluster_id,
                       RDTSC_ASGUARD, 0, 0, ASGUARD_PING_REQ_UNI);
    priv->fire_ping = 1;

    return count;

error:
    asguard_error("Error during parsing of input.%s\n", __func__);
    return err;

}

static int asguard_pmpu_show(struct seq_file *m, void *v)
{
    struct echo_priv *priv =
            (struct echo_priv *)m->private;

    if (!priv)
        return -ENODEV;

    //seq_printf(m, "%pUB\n", priv->uuid.b);

    return 0;
}

static int asguard_pmpu_ctrl_open(struct inode *inode, struct file *file)
{
    return single_open(file, asguard_pmpu_show,
                       PDE_DATA(file_inode(file)));
}

static ssize_t asguard_pupm_ctrl_write(struct file *file,
                                       const char __user *user_buffer, size_t count,
                                       loff_t *data)
{
    struct echo_priv *priv =
            (struct echo_priv *)PDE_DATA(file_inode(file));
    char kernel_buffer[ASGUARD_NUMBUF];
    int target_cluster_id = -3;
    int err;
    int remote_lid;

    err = read_pingpong_user_input(user_buffer, count, priv, kernel_buffer,&target_cluster_id, &remote_lid);

    if(err)
        goto error;

    setup_echo_msg_uni(priv->ins, &priv->sdev->pminfo, remote_lid,
                         priv->sdev->pminfo.cluster_id, target_cluster_id,
                         RDTSC_ASGUARD, 0, 0, ASGUARD_PING_REQ_MULTI);

    /* Use Echo Protocol Port for ping */
    priv->sdev->pminfo.pm_targets[remote_lid].pkt_data.port = 3321;


    return count;

error:
    asguard_error("Error during parsing of input.%s\n", __func__);
    return err;
}

static int asguard_pupm_ctrl_show(struct seq_file *m, void *v)
{
    struct echo_priv *priv =
            (struct echo_priv *)m->private;

    if (!priv)
        return -ENODEV;

    //seq_printf(m, "%pUB\n", priv->uuid.b);

    return 0;
}
static int asguard_pupm_ctrl_open(struct inode *inode, struct file *file)
{
    return single_open(file, asguard_pupm_ctrl_show,
                       PDE_DATA(file_inode(file)));
}


static ssize_t asguard_ppwt_ctrl_write(struct file *file,
                                       const char __user *user_buffer, size_t count,
                                       loff_t *data)
{
    struct echo_priv *priv =
            (struct echo_priv *)PDE_DATA(file_inode(file));
    char kernel_buffer[ASGUARD_NUMBUF];
    int target = -3;
    int err;

    err = read_user_input_int(user_buffer, count, priv, kernel_buffer, &target);

    if(err||target < 0)
        goto error;

    priv->pong_waiting_interval = target;

    asguard_dbg("Set Png Reply Waiting Time to %d\n", target);

    return count;

    error:
    asguard_error("Error during parsing of input.%s\n", __func__);
    return err;
}

static int asguard_ppwt_show(struct seq_file *m, void *v)
{
    struct echo_priv *priv =
            (struct echo_priv *)m->private;

    if (!priv)
        return -ENODEV;

    //seq_printf(m, "%pUB\n", priv->uuid.b);

    return 0;
}

static int asguard_ppwt_ctrl_open(struct inode *inode, struct file *file)
{
    return single_open(file, asguard_ppwt_show,
                       PDE_DATA(file_inode(file)));
}

static const struct file_operations asguard_pupm_ops = {
        .owner = THIS_MODULE,
        .open = asguard_pupm_ctrl_open,
        .write = asguard_pupm_ctrl_write,
        .read = seq_read,
        .llseek = seq_lseek,
        .release = single_release,
};

static const struct file_operations asguard_pupu_ops = {
        .owner = THIS_MODULE,
        .open = asguard_pupu_ctrl_open,
        .write = asguard_pupu_ctrl_write,
        .read = seq_read,
        .llseek = seq_lseek,
        .release = single_release,
};

static const struct file_operations asguard_pmpu_ops = {
        .owner = THIS_MODULE,
        .open = asguard_pmpu_ctrl_open,
        .write = asguard_pmpu_ctrl_write,
        .read = seq_read,
        .llseek = seq_lseek,
        .release = single_release,
};

static const struct file_operations asguard_pmpm_ops = {
        .owner = THIS_MODULE,
        .open = asguard_pmpm_ctrl_open,
        .write = asguard_pmpm_ctrl_write,
        .read = seq_read,
        .llseek = seq_lseek,
        .release = single_release,
};

static const struct file_operations asguard_ppwt_ops = {
        .owner = THIS_MODULE,
        .open = asguard_ppwt_ctrl_open,
        .write = asguard_ppwt_ctrl_write,
        .read = seq_read,
        .llseek = seq_lseek,
        .release = single_release,
};

void init_ping_ctrl_interfaces(struct echo_priv *priv)
{
    char name_buf[MAX_ASGUARD_PROC_NAME];


    snprintf(name_buf, sizeof(name_buf),
             "asguard/%d/proto_instances/%d/waiting_time",
             priv->sdev->ifindex, priv->ins->instance_id);

    priv->echo_ppwt_entry = proc_create_data(name_buf, S_IRWXU | S_IRWXO, NULL, &asguard_ppwt_ops, priv);



    snprintf(name_buf, sizeof(name_buf),
             "asguard/%d/proto_instances/%d/ping_unicast_pong_unicast",
             priv->sdev->ifindex, priv->ins->instance_id);

    priv->echo_pupu_entry = proc_create_data(name_buf, S_IRWXU | S_IRWXO, NULL, &asguard_pupu_ops, priv);

    snprintf(name_buf, sizeof(name_buf),
             "asguard/%d/proto_instances/%d/ping_unicast_pong_multicast",
             priv->sdev->ifindex, priv->ins->instance_id);

    priv->echo_pupm_entry = proc_create_data(name_buf, S_IRWXU | S_IRWXO, NULL, &asguard_pupm_ops, priv);

    snprintf(name_buf, sizeof(name_buf),
             "asguard/%d/proto_instances/%d/ping_multicast_pong_unicast",
             priv->sdev->ifindex, priv->ins->instance_id);

    priv->echo_pmpu_entry = proc_create_data(name_buf, S_IRWXU | S_IRWXO, NULL, &asguard_pmpu_ops, priv);


    snprintf(name_buf, sizeof(name_buf),
             "asguard/%d/proto_instances/%d/ping_multicast_pong_multicast",
             priv->sdev->ifindex, priv->ins->instance_id);

    priv->echo_pmpm_entry = proc_create_data(name_buf, S_IRWXU | S_IRWXO, NULL, &asguard_pmpm_ops, priv);


}
EXPORT_SYMBOL(init_ping_ctrl_interfaces);

void remove_ping_ctrl_interfaces(struct echo_priv *priv)
{
    char name_buf[MAX_ASGUARD_PROC_NAME];

    snprintf(name_buf, sizeof(name_buf),
             "asguard/%d/proto_instances/%d/waiting_time",
             priv->sdev->ifindex, priv->ins->instance_id);

    if(priv->echo_ppwt_entry) {
        remove_proc_entry(name_buf, NULL);
        priv->echo_ppwt_entry = NULL;
    }

    snprintf(name_buf, sizeof(name_buf),
             "asguard/%d/proto_instances/%d/ping_unicast_pong_unicast",
             priv->sdev->ifindex, priv->ins->instance_id);

    if(priv->echo_pupu_entry) {
        remove_proc_entry(name_buf, NULL);
        priv->echo_pupu_entry = NULL;
    }

    snprintf(name_buf, sizeof(name_buf),
             "asguard/%d/proto_instances/%d/ping_multicast_pong_unicast",
             priv->sdev->ifindex, priv->ins->instance_id);

    if(priv->echo_pmpu_entry) {
        remove_proc_entry(name_buf, NULL);
        priv->echo_pmpu_entry = NULL;
    }

    snprintf(name_buf, sizeof(name_buf),
             "asguard/%d/proto_instances/%d/ping_unicast_pong_multicast",
             priv->sdev->ifindex, priv->ins->instance_id);

    if(priv->echo_pupm_entry) {
        remove_proc_entry(name_buf, NULL);
        priv->echo_pupm_entry = NULL;
    }

    snprintf(name_buf, sizeof(name_buf),
             "asguard/%d/proto_instances/%d/ping_multicast_pong_multicast",
             priv->sdev->ifindex, priv->ins->instance_id);

    if(priv->echo_pmpm_entry) {
        remove_proc_entry(name_buf, NULL);
        priv->echo_pmpm_entry = NULL;
    }

}
EXPORT_SYMBOL(remove_ping_ctrl_interfaces);

