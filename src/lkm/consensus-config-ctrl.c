

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>


#include "consensus-config-ctrl.h"




// /proc/asgard/<ifindex/some_blah_
static ssize_t asgard_le_config_write(struct file *file, const char __user *user_buffer, size_t count, loff_t *data)
{
    struct consensus_priv *priv =
            (struct consensus_priv *)PDE_DATA(file_inode(file));

    int err;
    char kernel_buffer[ASGARD_TARGETS_BUF];
    char *search_str;
    size_t size = min(sizeof(kernel_buffer) - 1, count);
    char *input_str;
    static const char delimiters[] = " ,;()";
    int state = 0;
    int fmin_tmp, fmax_tmp, cmin_tmp, cmax_tmp, max_entries_per_pkt_tmp;
    int tmp;

    max_entries_per_pkt_tmp = -1;
    fmin_tmp = -1;
    fmax_tmp = -1;
    cmin_tmp = -1;
    cmax_tmp = -1;

    if (!priv)
        return -ENODEV;

    memset(kernel_buffer, 0, sizeof(kernel_buffer));

    err = copy_from_user(kernel_buffer, user_buffer, count);
    if (err) {
        asgard_error("Copy from user failed%s\n", __func__);
        goto error;
    }

    kernel_buffer[size] = '\0';

    search_str = kstrdup(kernel_buffer, GFP_KERNEL);
    while ((input_str = strsep(&search_str, delimiters)) != NULL) {
        if (!input_str || strlen(input_str) <= 0)
            continue;

        err = kstrtoint(input_str, 10, &tmp);

        if (err) {
            asgard_error("error converting '%s' to an integer", input_str);
            goto error;
        }

        if (state == 0) {
            fmin_tmp = tmp;
            state = 1;
        } else if (state == 1) {
            fmax_tmp = tmp;
            state = 2;
        } else if (state == 2) {
            cmin_tmp = tmp;
            state = 3;
        } else if (state == 3) {
            cmax_tmp = tmp;
            state = 4;
        } else if (state == 4) {
            max_entries_per_pkt_tmp = tmp;
            break;
        }
    }

    if (!(fmin_tmp < fmax_tmp && cmin_tmp < cmax_tmp)) {
        asgard_error("Invalid Ranges! Must assure that fmin < fmax and cmin < cmax\n");
        asgard_error("input order: fmin, fmax, cmin, cmax, max_entries_per_pkt_tmp\n");
        goto error;
    }

    if (!(max_entries_per_pkt_tmp > 0 && max_entries_per_pkt_tmp <= MAX_AE_ENTRIES_PER_PKT)) {
        asgard_error("Invalid for entries per consensus payload!\n");
        asgard_error("Must be in (0,%d) interval!\n", MAX_AE_ENTRIES_PER_PKT);
        goto error;
    }

    priv->max_entries_per_pkt = max_entries_per_pkt_tmp;


    return count;
error:
    asgard_error("Error during parsing of input.%s\n", __func__);
    return err;

}

static int asgard_le_config_show(struct seq_file *m, void *v)
{
    struct consensus_priv *priv =
            (struct consensus_priv *)m->private;

    if (!priv)
        return -ENODEV;

    seq_printf(m, "%d\n",
               priv->max_entries_per_pkt);

    return 0;
}

static int asgard_le_config_open(struct inode *inode, struct file *file)
{
    return single_open(file, asgard_le_config_show,
                       PDE_DATA(file_inode(file)));
}

static const struct proc_ops asgard_le_config_ops = {
        .proc_open = asgard_le_config_open,
        .proc_write = asgard_le_config_write,
        .proc_read = seq_read,
        .proc_lseek = seq_lseek,
        .proc_release = single_release,
};
static ssize_t asgard_eval_uuid_write(struct file *file, const char __user *user_buffer, size_t count, loff_t *data)
{
    struct consensus_priv *priv =
        (struct consensus_priv *)PDE_DATA(file_inode(file));

    char kernel_buffer[ASGARD_UUID_BUF];
    size_t size = min(sizeof(kernel_buffer) - 1, count);
    int err;

    if (!priv)
        return -ENODEV;

    if(size < count) {
        asgard_error("Invalid input! Too many characters.\n");
        err = -EINVAL;
        goto error;
    }
    memset(kernel_buffer, 0, sizeof(kernel_buffer));

    // requires previous check - it must be: kernel buffer size > count!
    err = copy_from_user(kernel_buffer, user_buffer, count);

    if (err) {
        asgard_error("Copy from user failed%s\n", __func__);
        goto error;
    }

    kernel_buffer[size] = '\0';

    uuid_parse(kernel_buffer, &priv->uuid);

    return count;
error:
    asgard_error("Error during parsing of input.%s\n", __func__);
    return err;

}

static int asgard_eval_uuid_show(struct seq_file *m, void *v)
{
    struct consensus_priv *priv =
            (struct consensus_priv *)m->private;

    if (!priv)
        return -ENODEV;

    seq_printf(m, "%pUB\n", priv->uuid.b);

    return 0;
}

static int asgard_eval_uuid_open(struct inode *inode, struct file *file)
{
    return single_open(file, asgard_eval_uuid_show,
                       PDE_DATA(file_inode(file)));
}

static const struct proc_ops asgard_eval_uuid_ops = {
        .proc_open = asgard_eval_uuid_open,
        .proc_write = asgard_eval_uuid_write,
        .proc_read = seq_read,
        .proc_lseek = seq_lseek,
        .proc_release = single_release,
};

void init_le_config_ctrl_interfaces(struct consensus_priv *priv)
{
    char name_buf[MAX_ASGARD_PROC_NAME];

    snprintf(name_buf, sizeof(name_buf),
             "asgard/%d/proto_instances/%d/le_config",
             priv->sdev->ifindex, priv->ins->instance_id);

    priv->le_config_entry = proc_create_data(name_buf, S_IRWXU | S_IRWXO, NULL, &asgard_le_config_ops, priv);

    snprintf(name_buf, sizeof(name_buf),
             "asgard/%d/proto_instances/%d/uuid",
             priv->sdev->ifindex, priv->ins->instance_id);

    priv->uuid_entry = proc_create_data(name_buf, S_IRWXU | S_IRWXO, NULL, &asgard_eval_uuid_ops, priv);

}
EXPORT_SYMBOL(init_le_config_ctrl_interfaces);

void remove_le_config_ctrl_interfaces(struct consensus_priv *priv)
{
    char name_buf[MAX_ASGARD_PROC_NAME];

    snprintf(name_buf, sizeof(name_buf),
             "asgard/%d/proto_instances/%d/le_config",
             priv->sdev->ifindex, priv->ins->instance_id);

    if(priv->le_config_entry) {
        remove_proc_entry(name_buf, NULL);
        priv->le_config_entry = NULL;
    }

    snprintf(name_buf, sizeof(name_buf),
             "asgard/%d/proto_instances/%d/uuid",
             priv->sdev->ifindex, priv->ins->instance_id);

    if(priv->uuid_entry) {
        remove_proc_entry(name_buf, NULL);
        priv->uuid_entry = NULL;
    }

}
EXPORT_SYMBOL(remove_le_config_ctrl_interfaces);
