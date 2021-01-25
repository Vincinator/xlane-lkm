#pragma once

#include "libasraft.h"


int register_protocol_instance(struct asgard_device *sdev, int instance_id, int protocol_id);
struct proto_instance *generate_protocol_instance(struct asgard_device *sdev, int protocol_id);