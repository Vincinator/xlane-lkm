#ifndef _SASSY_LOGGER_H_
#define _SASSY_LOGGER_H_


/* 
 * Prefixes NIC device ID and SASSY Context (e.g. [SASSY][NIC4][CONSENSUS])
 */
#define sassy_dbg(dev, format, arg...)					\
({														\
	if (0)												\
		printk(KERN_DEBUG, sassy_info, format, ##arg);	\
})
#endif

#endif _SASSY_LOGGER_H_