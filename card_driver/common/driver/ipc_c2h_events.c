#include "ipc_c2h_events.h"
#include "sph_log.h"
#include "sph_error.h"
#include <linux/slab.h>
#include "ipc_protocol.h"

int eventValToSphErrno(enum event_val eventVal)
{
	sph_log_info(GENERAL_LOG, "Got error. eventVal: %d\n", eventVal);

	switch (eventVal) {
	case SPH_IPC_NO_SUCH_CONTEXT: {
		return -SPHER_NO_SUCH_CONTEXT;
	}
	case SPH_IPC_NO_SUCH_DEVRES: {
		return -SPHER_NO_SUCH_RESOURCE;
	}
	case SPH_IPC_NO_SUCH_COPY: {
		return -EFAULT;
	}
	case SPH_IPC_NO_SUCH_NET: {
		return -EFAULT;
	}
	case SPH_IPC_NO_SUCH_INFREQ: {
		return -EFAULT;
	}
	case SPH_IPC_ALREADY_EXIST: {
		return -EFAULT;
	}
	case SPH_IPC_NO_DAEMON: {
		return -EFAULT;
	}
	case SPH_IPC_NO_MEMORY: {
		return -ENOMEM;
	}
	case SPH_IPC_RUNTIME_FAILED: {
		return -EFAULT;
	}
	case SPH_IPC_RUNTIME_LAUNCH_FAILED: {
		return -EFAULT;
	}
	case SPH_IPC_DMA_ERROR: {
		return -EFAULT;
	}
	case SPH_IPC_RUNTIME_NOT_SUPPORTED: {
		return -SPHER_NOT_SUPPORTED;
	}
	case SPH_IPC_RUNTIME_INVALID_EXECUTABLE_NETWORK_BINARY: {
		return -SPHER_INVALID_EXECUTABLE_NETWORK_BINARY;
	}
	case SPH_IPC_RUNTIME_INFER_MISSING_RESOURCE: {
		return -SPHER_INFER_MISSING_RESOURCE;
	}
	case SPH_IPC_RUNTIME_INFER_EXEC_ERROR: {
		return -SPHER_INFER_EXEC_ERROR;
	}
	case SPH_IPC_RUNTIME_INFER_SCHEDULE_ERROR: {
		return -SPHER_INFER_SCHEDULE_ERROR;
	}
	case SPH_IPC_CONTEXT_BROKEN: {
		return -SPHER_CONTEXT_BROKEN;
	}
	default: {
		return -EFAULT;
	}
	}
	return -EFAULT;
}

#ifdef _DEBUG
const char *event_code_name(u32 eventCode)
{
	switch (eventCode) {
	case SPH_IPC_CREATE_CONTEXT_SUCCESS:
		return "CREATE_CONTEXT_SUCCESS";
	case SPH_IPC_CREATE_DEVRES_SUCCESS:
		return "CREATE_DEVRES_SUCCESS";
	case SPH_IPC_CREATE_COPY_SUCCESS:
		return "CREATE_COPY_SUCCESS";
	case SPH_IPC_EXECUTE_COPY_SUCCESS:
		return "EXECUTE_COPY_SUCCESS";
	case SPH_IPC_DEVRES_DESTROYED:
		return "DEVRES_DESTROYED";
	case SPH_IPC_COPY_DESTROYED:
		return "COPY_DESTROYED";
	case SPH_IPC_CONTEXT_DESTROYED:
		return "CONTEXT_DESTROYED";
	case SPH_IPC_CREATE_DEVNET_SUCCESS:
		return "CREATE_DEVNET_SUCCESS";
	case SPH_IPC_DEVNET_DESTROYED:
		return "DEVNET_DESTROYED";
	case SPH_IPC_CREATE_INFREQ_SUCCESS:
		return "CREATE_INFREQ_SUCCESS";
	case SPH_IPC_INFREQ_DESTROYED:
		return "INFREQ_DESTROYED";
	case SPH_IPC_RECOVER_CONTEXT_SUCCESS:
		return "RECOVER_CONTEXT_SUCCESS";
	case SPH_IPC_THERMAL_TRIP_EVENT:
		return "THERMAL_TRIP_EVENT";
	case SPH_IPC_CREATE_CONTEXT_FAILED:
		return "CREATE_CONTEXT_FAILED";
	case SPH_IPC_CREATE_DEVRES_FAILED:
		return "CREATE_DEVRES_FAILED";
	case SPH_IPC_CREATE_COPY_FAILED:
		return "CREATE_COPY_FAILED";
	case SPH_IPC_DESTROY_CONTEXT_FAILED:
		return "DESTROY_CONTEXT_FAILED";
	case SPH_IPC_DESTROY_DEVRES_FAILED:
		return "DESTROY_DEVRES_FAILED";
	case SPH_IPC_DESTROY_COPY_FAILED:
		return "DESTROY_COPY_FAILED";
	case SPH_IPC_CREATE_SYNC_FAILED:
		return "CREATE_SYNC_FAILED";
	case SPH_IPC_ERROR_SUB_RESOURCE_LOAD_FAILED:
		return "ERROR_SUB_RESOURCE_LOAD_FAILED";
	case SPH_IPC_CREATE_DEVNET_FAILED:
		return "CREATE_DEVNET_FAILED";
	case SPH_IPC_DESTROY_DEVNET_FAILED:
		return "DESTROY_DEVNET_FAILED";
	case SPH_IPC_CREATE_INFREQ_FAILED:
		return "CREATE_INFREQ_FAILED";
	case SPH_IPC_DESTROY_INFREQ_FAILED:
		return "DESTROY_INFREQ_FAILED";
	case SPH_IPC_RECOVER_CONTEXT_FAILED:
		return "RECOVER_CONTEXT_FAILED";
	case SPH_IPC_ERROR_MCE_CORRECTABLE:
		return "ERROR_MCE_CORRECTABLE";
	case SPH_IPC_ERROR_MCE_UNCORRECTABLE:
		return "ERROR_MCE_UNCORRECTABLE";
	case SPH_IPC_ERROR_RUNTIME_LAUNCH:
		return "ERROR_RUNTIME_LAUNCH";
	case SPH_IPC_ERROR_RUNTIME_DIED:
		return "ERROR_RUNTIME_DIED";
	case SPH_IPC_EXECUTE_COPY_FAILED:
		return "EXECUTE_COPY_FAILED";
	case SPH_IPC_SCHEDULE_INFREQ_FAILED:
		return "SCHEDULE_INFREQ_FAILED";
	case SPH_IPC_ERROR_OS_CRASHED:
		return "ERROR_OS_CRASHED";
	case SPH_IPC_ERROR_PCI_ERROR:
		return "ERROR_PCI_ERROR";
	case SPH_IPC_ERROR_CARD_RESET:
		return "ERROR_CARD_RESET";
	case SPH_IPC_ERROR_MCE_UNCORRECTABLE_FATAL:
		return "ERROR_MCE_UNCORRECTABLE_FATAL";
	default:
		return "Unknown event code";
	}
}

void log_c2h_event(const char *msg, const union c2h_EventReport *ev)
{
	sph_log_debug(IPC_LOG, "%s: %s val=%u ctx_id=%u (valid=%u) objID=%u (valid=%u) objID_2=%u (valid=%u)\n",
		      msg,
		      event_code_name(ev->eventCode),
		      ev->eventVal,
		      ev->contextID, ev->ctxValid,
		      ev->objID, ev->objValid,
		      ev->objID_2, ev->objValid_2);
}
#endif
