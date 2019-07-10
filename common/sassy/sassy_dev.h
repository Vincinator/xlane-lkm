#ifndef _SASSY_DEV_H_
#define _SASSY_DEV_H_


#include <sassy/sassy_pm.h>

struct sassy_device {
	int ifindex;  /* corresponds to ifindex of net_device */	


	/* SASSY CTRL Structures */
	struct sassy_pacemaker_info 	pminfo;	

};

#endif /* _SASSY_DEV_H_ */