#ifndef ASGARD_ASGARD_PING_CTRL_H
#define ASGARD_ASGARD_PING_CTRL_H

#include <linux/slab.h>
#include <linux/uaccess.h>
#include <asgard/asgard_echo.h>

int read_pingpong_user_input(const char *user_buffer, size_t count, const struct echo_priv *priv,
                              char *kernel_buffer,  int *target_cluster_id,
                              int *remote_lid);
                              
int read_user_input_int(const char *user_buffer, size_t count, const struct echo_priv *priv,
                        char *kernel_buffer, int *target_cluster_id);

#endif //ASGARD_ASGARD_PING_CTRL_H
