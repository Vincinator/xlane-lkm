#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <net/net_namespace.h>

#include <sassy/sassy.h>
#include <sassy/logger.h>

#include "event_logger.h"



int init_le_logging(struct sassy_device *sdev) 
{
	char name_buf[MAX_SASSY_PROC_NAME];
	int err;
	int i;
	
	if (sdev->verbose)
		sassy_dbg(" sassy device setup started %s\n", __FUNCTION__);

	if (!sdev) {
		err = -EINVAL;
		sassy_error(" sassy device is NULL %s\n", __FUNCTION__);
		goto error;
	}

	sdev->le_logs = kmalloc(sizeof(struct le_event_logs), GFP_KERNEL);

	if (!sdev->le_logs) {
		err = -ENOMEM;
		sassy_error(" Could not allocate memory for leader election stats. %s\n",
			    __FUNCTION__);
		goto error;
	}

	sdev->le_logs->events = kmalloc_array(LE_EVENT_LOG_LIMIT,
									sizeof(struct le_event),
									GFP_KERNEL);

	if (!sdev->le_logs->events) {
		err = -ENOMEM;
		sassy_error(
			"Could not allocate memory for leader election logs intems: %d.\n", i);
		goto error;
	}

	sdev->le_logs->current_entries = 0;

	init_le_log_ctrl(sdev);


error:
	return err;
}