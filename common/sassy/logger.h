#ifndef _SASSY_LOGGER_H_
#define _SASSY_LOGGER_H_


#ifndef LOG_PREFIX
#define LOG_PREFIX "[SASSY][UNKNOWN MODULE]"
#endif

/* 
 * Prefixes NIC device ID and SASSY Context (e.g. [SASSY][NIC4][CONSENSUS])
 */
#define sassy_dbg(format, arg...)						\
({														\
	if (1)												\
		printk(KERN_DEBUG LOG_PREFIX format, ##arg);	\
})

#define sassy_error(format, arg...)						\
({														\
	if (1)												\
		printk(KERN_ERR LOG_PREFIX format, ##arg);	\
})

#endif  /* _SASSY_LOGGER_H_ */
