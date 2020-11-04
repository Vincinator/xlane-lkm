#ifndef _ASGARD_LOGGER_H_
#define _ASGARD_LOGGER_H_

#include <linux/types.h>


#define ASGARD_DEBUG 1

#define MAX_LOGGER_NAME 32
#define LOGGER_EVENT_LIMIT 100000

#ifndef LOG_PREFIX
#define LOG_PREFIX "[ASGARD][UNKNOWN MODULE]"
#endif

#define LOG_LE_PREFIX "[LEADER ELECTION][LOG]"

/*
 * Prefixes NIC device ID and ASGARD Context (e.g. [ASGARD][NIC4][CONSENSUS])
 */
#if ASGARD_DEBUG
	#define asgard_dbg(format, arg...)						\
	({														\
		if (1)												\
			printk(KERN_DEBUG LOG_PREFIX format, ##arg);	\
	})
#else
	#define asgard_dbg(format, arg...)
#endif

#define asgard_error(format, arg...)						\
({														\
	if (1)												\
		printk(KERN_ERR LOG_PREFIX format, ##arg);	\
})

#define asgard_log_le(format, arg...)						\
({														\
	if (1)												\
		printk(KERN_ERR LOG_LE_PREFIX format, ##arg);	\
})



#endif  /* _ASGARD_LOGGER_H_ */
