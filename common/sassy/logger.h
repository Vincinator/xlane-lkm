#ifndef _SASSY_LOGGER_H_
#define _SASSY_LOGGER_H_

#include <linux/types.h>


#define SASSY_DEBUG 1

#define MAX_LOGGER_NAME 16
#define LOGGER_EVENT_LIMIT 10000


#ifndef LOG_PREFIX
#define LOG_PREFIX "[SASSY][UNKNOWN MODULE]"
#endif

#define LOG_LE_PREFIX "[LEADER ELECTION][LOG]"

/* 
 * Prefixes NIC device ID and SASSY Context (e.g. [SASSY][NIC4][CONSENSUS])
 */
#if SASSY_DEBUG
	#define sassy_dbg(format, arg...)						\
	({														\
		if (1)												\
			printk(KERN_DEBUG LOG_PREFIX format, ##arg);	\
	})
#else
	#define sassy_dbg(format, arg...)
#endif

#define sassy_error(format, arg...)						\
({														\
	if (1)												\
		printk(KERN_ERR LOG_PREFIX format, ##arg);	\
})

#define sassy_log_le(format, arg...)						\
({														\
	if (1)												\
		printk(KERN_ERR LOG_LE_PREFIX format, ##arg);	\
})




enum logger_state {
	LOGGER_RUNNING,
	LOGGER_READY, 	/* Initialized but not active*/
	LOGGER_UNINIT,
	LOGGER_LOG_FULL,
};

struct logger_event {
	uint64_t timestamp_tcs;
	int type;
};

struct event_logs {

	/* Size is defined by LOGGER_EVENT_LIMIT */
	struct logger_event *events;

	/* Last valid log entry in the le_event array */
	int current_entries;
	
	struct proc_dir_entry	*proc_dir;	

	char *name;

};

struct logger {

	int ifindex; 

	enum logger_state state;

	char name[MAX_LOGGER_NAME];

	struct event_logs *logs;

	int current_entries;

};

int init_logger(struct logger *slog);

int sassy_log_stop(struct logger *slog);
int sassy_log_start(struct logger *slog);
int sassy_log_reset(struct logger *slog);

const char *logger_state_string(enum logger_state state);

#endif  /* _SASSY_LOGGER_H_ */
