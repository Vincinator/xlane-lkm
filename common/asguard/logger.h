#ifndef _ASGUARD_LOGGER_H_
#define _ASGUARD_LOGGER_H_

#include <linux/types.h>


#define ASGUARD_DEBUG 1

#define MAX_LOGGER_NAME 16
#define LOGGER_EVENT_LIMIT 10000


#ifndef LOG_PREFIX
#define LOG_PREFIX "[ASGUARD][UNKNOWN MODULE]"
#endif

#define LOG_LE_PREFIX "[LEADER ELECTION][LOG]"

/* 
 * Prefixes NIC device ID and ASGUARD Context (e.g. [ASGUARD][NIC4][CONSENSUS])
 */
#if ASGUARD_DEBUG
	#define asguard_dbg(format, arg...)						\
	({														\
		if (1)												\
			printk(KERN_DEBUG LOG_PREFIX format, ##arg);	\
	})
#else
	#define asguard_dbg(format, arg...)
#endif

#define asguard_error(format, arg...)						\
({														\
	if (1)												\
		printk(KERN_ERR LOG_PREFIX format, ##arg);	\
})

#define asguard_log_le(format, arg...)						\
({														\
	if (1)												\
		printk(KERN_ERR LOG_LE_PREFIX format, ##arg);	\
})



#endif  /* _ASGUARD_LOGGER_H_ */