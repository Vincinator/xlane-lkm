//
// Created by vincent on 1/25/21.
//

#include "consensus-eval-ctrl.h"

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>



static ssize_t asgard_eval_ctrl_write(struct file *file, const char __user *user_buffer, size_t count,
        loff_t *data)
{
    struct consensus_priv *priv =
            (struct consensus_priv *)PDE_DATA(file_inode(file));
    char kernel_buffer[ASGARD_NUMBUF];
    int eval_selection = -3;
    size_t size;
    int err;

    size = min(sizeof(kernel_buffer) - 1, count);

    memset(kernel_buffer, 0, sizeof(kernel_buffer));

    err = copy_from_user(kernel_buffer, user_buffer, count);
    if (err) {
        asgard_error("Copy from user failed%s\n", __func__);
        goto error;
    }

    kernel_buffer[size] = '\0';
    err = kstrtoint(kernel_buffer, 10, &eval_selection);

    if (err) {
        asgard_dbg("Error converting input buffer: %s, todevid: 0x%x\n",
        kernel_buffer, eval_selection);
        goto error;
    }

    switch (eval_selection) {
        case 0:
            testcase_stop_timer(priv);
            break;
        case -1:
            // one shot
            testcase_one_shot_big_log(priv);
            break;
        default:
            if (eval_selection < 0) {
                asgard_error("Invalid input: %d - %s\n", eval_selection, __func__);
                err = -EINVAL;
                goto error;
            }
            // asgard_dbg("Appending %d entries to log every second\n", eval_selection);
            testcase_X_requests_per_sec(priv, eval_selection);

    }

    return count;
error:
    asgard_error("Error during parsing of input.%s\n", __func__);
    return err;

}

static int asgard_eval_ctrl_show(struct seq_file *m, void *v)
{
    struct consensus_priv *priv =
            (struct consensus_priv *)m->private;

    if (!priv)
        return -ENODEV;

    //seq_printf(m, "%pUB\n", priv->uuid.b);

    return 0;
}

static int asgard_eval_ctrl_open(struct inode *inode, struct file *file)
{
    return single_open(file, asgard_eval_ctrl_show,
                       PDE_DATA(file_inode(file)));
}


static const struct proc_ops asgard_eval_ctrl_ops = {
        .proc_open = asgard_eval_ctrl_open,
        .proc_write = asgard_eval_ctrl_write,
        .proc_read = seq_read,
        .proc_lseek = seq_lseek,
        .proc_release = single_release,
};

void init_eval_ctrl_interfaces(struct consensus_priv *priv)
{
    char name_buf[MAX_ASGARD_PROC_NAME];

    if(!priv || !priv->sdev || priv->ins) {
        asgard_error("Catched NUll pointer in %s\n", __FUNCTION__);
        return;
    }

    snprintf(name_buf, sizeof(name_buf),
             "asgard/%d/proto_instances/%d/consensus_eval_ctrl",
             priv->sdev->ifindex, priv->ins->instance_id);

    priv->consensus_eval_ctrl_entry = proc_create_data(name_buf, S_IRWXU | S_IRWXO, NULL, &asgard_eval_ctrl_ops, priv);


}
EXPORT_SYMBOL(init_eval_ctrl_interfaces);

void remove_eval_ctrl_interfaces(struct consensus_priv *priv)
{
    char name_buf[MAX_ASGARD_PROC_NAME];

    snprintf(name_buf, sizeof(name_buf),
             "asgard/%d/proto_instances/%d/consensus_eval_ctrl",
             priv->sdev->ifindex, priv->ins->instance_id);

    if(priv->consensus_eval_ctrl_entry) {
        remove_proc_entry(name_buf, NULL);
        priv->consensus_eval_ctrl_entry = NULL;
    }

}
EXPORT_SYMBOL(remove_eval_ctrl_interfaces);
