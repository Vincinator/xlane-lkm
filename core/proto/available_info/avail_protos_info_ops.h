#pragma once


static ssize_t proto_info_write(struct file *file, const char __user *user_buffer, size_t count, loff_t *data);
static int proto_info_open(struct inode *inode, struct file *file);