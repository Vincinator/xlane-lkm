#pragma once
#include "../core/libasraft.h"

void remove_proto_instance_ctrl(struct asgard_device *sdev);
void init_proto_instance_ctrl(struct asgard_device *sdev);
void init_proto_instance_root(struct asgard_device *sdev, int instance_id);
