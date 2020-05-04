#include "asgard_ping_ctrl.h"


#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include <asguard/asguard.h>
#include <asguard/logger.h>
#include <asguard/consensus.h>

#include "asguard_echo.h"


static ssize_t asguard_ping_ctrl_write(struct file *file,
                                       const char __user *user_buffer, size_t count,
                                       loff_t *data)
{
    struct asguard_echo_priv *priv =
            (struct asguard_echo_priv *)PDE_DATA(file_inode(file));
    char kernel_buffer[ASGUARD_NUMBUF];
    int eval_selection = -3;
    size_t size;
    int err;

    size = min(sizeof(kernel_buffer) - 1, count);

    memset(kernel_buffer, 0, sizeof(kernel_buffer));

    err = copy_from_user(kernel_buffer, user_buffer, count);
    if (err) {
        asguard_error("Copy from user failed%s\n", __func__);
        goto error;
    }

    kernel_buffer[size] = '\0';
    err = kstrtoint(kernel_buffer, 10, &eval_selection);
    if (err) {
        asguard_dbg("Error converting input buffer: %s, todevid: 0x%x\n",
                    kernel_buffer, eval_selection);
        goto error;
    }

    /* TODO: trigger unicast ping to target. Add -1 case to ping all cluster nodes */



    return count;
error:
    asguard_error("Error during parsing of input.%s\n", __func__);
    return err;

}

static int asguard_ping_ctrl_show(struct seq_file *m, void *v)
{
    struct asguard_echo_priv *priv =
            (struct asguard_echo_priv *)m->private;

    if (!priv)
        return -ENODEV;

    //seq_printf(m, "%pUB\n", priv->uuid.b);

    return 0;
}

static int asguard_ping_ctrl_open(struct inode *inode, struct file *file)
{
    return single_open(file, asguard_ping_ctrl_show,
                       PDE_DATA(file_inode(file)));
}

static ssize_t asguard_ping_multicast_ctrl_write(struct file *file,
                                       const char __user *user_buffer, size_t count,
                                       loff_t *data)
{
    struct asguard_echo_priv *priv =
            (struct asguard_echo_priv *)PDE_DATA(file_inode(file));
    char kernel_buffer[ASGUARD_NUMBUF];
    int eval_selection = -3;
    size_t size;
    int err;

    size = min(sizeof(kernel_buffer) - 1, count);

    memset(kernel_buffer, 0, sizeof(kernel_buffer));

    err = copy_from_user(kernel_buffer, user_buffer, count);
    if (err) {
        asguard_error("Copy from user failed%s\n", __func__);
        goto error;
    }

    kernel_buffer[size] = '\0';
    err = kstrtoint(kernel_buffer, 10, &eval_selection);
    if (err) {
        asguard_dbg("Error converting input buffer: %s, todevid: 0x%x\n",
                    kernel_buffer, eval_selection);
        goto error;
    }

    /* TODO: trigger multicast ping (ignore input) */

    return count;
    error:
    asguard_error("Error during parsing of input.%s\n", __func__);
    return err;

}

static int asguard_ping_multicast_ctrl_show(struct seq_file *m, void *v)
{
    struct asguard_echo_priv *priv =
            (struct asguard_echo_priv *)m->private;

    if (!priv)
        return -ENODEV;

    //seq_printf(m, "%pUB\n", priv->uuid.b);

    return 0;
}

static int asguard_ping_multicast_ctrl_open(struct inode *inode, struct file *file)
{
    return single_open(file, asguard_ping_multicast_ctrl_show,
                       PDE_DATA(file_inode(file)));
}

static const struct file_operations asguard_ping_multicast_ops = {
        .owner = THIS_MODULE,
        .open = asguard_ping_multicast_ctrl_open,
        .write = asguard_ping_multicast_ctrl_write,
        .read = seq_read,
        .llseek = seq_lseek,
        .release = single_release,
};
static const struct file_operations asguard_ping_unicast_ops = {
        .owner = THIS_MODULE,
        .open = asguard_ping_ctrl_open,
        .write = asguard_ping_ctrl_write,
        .read = seq_read,
        .llseek = seq_lseek,
        .release = single_release,
};

void init_ping_ctrl_interfaces(struct asguard_echo_priv *priv)
{
    char name_buf[MAX_ASGUARD_PROC_NAME];

    snprintf(name_buf, sizeof(name_buf),
             "asguard/%d/proto_instances/%d/ping_unicast",
             priv->sdev->ifindex, priv->ins->instance_id);

    priv->echo_ping_unicast_entry = proc_create_data(name_buf, S_IRWXU | S_IRWXO, NULL, &asguard_ping_unicast_ops, priv);

    snprintf(name_buf, sizeof(name_buf),
             "asguard/%d/proto_instances/%d/ping_multicast",
             priv->sdev->ifindex, priv->ins->instance_id);

    priv->echo_ping_multicast_entry = proc_create_data(name_buf, S_IRWXU | S_IRWXO, NULL, &asguard_ping_multicast_ops, priv);



}
EXPORT_SYMBOL(init_eval_ctrl_interfaces);

void remove_ping_ctrl_interfaces(struct asguard_echo_priv *priv)
{
    char name_buf[MAX_ASGUARD_PROC_NAME];

    snprintf(name_buf, sizeof(name_buf),
             "asguard/%d/proto_instances/%d/ping_unicast",
             priv->sdev->ifindex, priv->ins->instance_id);

    if(priv->echo_ping_unicast_entry) {
        remove_proc_entry(name_buf, NULL);
        priv->echo_ping_unicast_entry = NULL;
    }

    snprintf(name_buf, sizeof(name_buf),
             "asguard/%d/proto_instances/%d/ping_multicast",
             priv->sdev->ifindex, priv->ins->instance_id);

    if(priv->echo_ping_multicast_entry) {
        remove_proc_entry(name_buf, NULL);
        priv->echo_ping_multicast_entry = NULL;
    }

}
EXPORT_SYMBOL(remove_eval_ctrl_interfaces);

