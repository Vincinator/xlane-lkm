#ifndef _SASSY_DEV_H_
#define _SASSY_DEV_H_




/*
 * SASSY stack is 
 */
struct sassy_device {
	int 	ifindex; 								/* corresponds to ifindex of net_device */	
	struct 	mlx5_core_dev*			mlx5_dev; 		/* Pointer to network device structure */
	struct 	device* 				device;
	struct sassy_pacemaker_info 	pminfo;	
};

#endif /* _SASSY_DEV_H_ */