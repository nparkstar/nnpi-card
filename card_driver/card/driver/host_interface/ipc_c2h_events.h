/********************************************
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 ********************************************/
#ifndef _SPH_IPC_C2H_EVENTS_H
#define _SPH_IPC_C2H_EVENTS_H

/**
 * The following describes the possible values for a c2h_EventReport message
 * sent from card to host to report on some error or other events.
 *
 * The c2h_EventReport message has the following fields available to describe
 * the event:
 *    eventCode  - 8 bits value describing the type of event
 *    eventVal   - 8 bits value - interpetation depends on eventCode
 *    contextID  - the protocolID of the context in which the event was occurred.
 *    objID      - 16 bits, interpretation depends on eventCode, usually used
 *                 to hold an inference object (devre, network, ...) protocol ID.
 *    objID_2    - 16 bits, in case objID is not enough to describe the object
 * In this file we define the possible values for the above fields and document
 * each field meaning for each possible eventCode.
 */

/**
 * Event codes ranges
 *
 * Those ranges should match the definition of RuntimeEventCodes
 * defined in include/sphcsInference.h and used by the daemon/runtime
 * interface !!!
 *
 * error codes are grouped into the following ranges:
 *     0 -  31   ==> non error events generated by daemon/runtime
 *    32 -  63   ==> non error events generated by card kernel driver
 *    64 -  95   ==> non-critical error events generated by daemon/runtime
 *    96 - 127   ==> non-critical error events generatd by kernel driver
 *   128 - 159   ==> context-critical error events generated by daemon/runtime
 *   160 - 191   ==> context-critical error events generated by kernel driver
 *   192 - 223   ==> card-critical error events generated by daemon/runtime
 *   224 - 255   ==> card-critical error events generated by kernel driver
 *
 * context-critical error event is one that puts the infer context in an
 * un-recovarable error state.
 * card-critical error event is one that make the card not useful for inference
 * request until it is reset.
 */
#define EVENT_NON_ERR_START             0
#define EVENT_NON_ERR_DRV_START        16
#define EVENT_ERR_START                48
#define EVENT_ERR_DRV_START            64
#define EVENT_CONTEXT_FATAL_START      96
#define EVENT_CONTEXT_FATAL_DRV_START 104
#define EVENT_CARD_FATAL_START        112
#define EVENT_CARD_FATAL_DRV_START    120

#define is_context_fatal_event(e)  ((e) >= EVENT_CONTEXT_FATAL_START && e < EVENT_CARD_FATAL_START)
#define is_card_fatal_event(e)     ((e) >= EVENT_CARD_FATAL_START)

#define SPH_IPC_RUNTIME_DONE   (EVENT_NON_ERR_START + 1)

/* non-error event codes */
#define SPH_IPC_CREATE_CONTEXT_SUCCESS   (EVENT_NON_ERR_DRV_START + 0)
#define SPH_IPC_CREATE_DEVRES_SUCCESS    (EVENT_NON_ERR_DRV_START + 1)
#define SPH_IPC_CREATE_COPY_SUCCESS      (EVENT_NON_ERR_DRV_START + 2)
#define SPH_IPC_EXECUTE_COPY_SUCCESS     (EVENT_NON_ERR_DRV_START + 3)
#define SPH_IPC_DEVRES_DESTROYED         (EVENT_NON_ERR_DRV_START + 4)
#define SPH_IPC_COPY_DESTROYED           (EVENT_NON_ERR_DRV_START + 5)
#define SPH_IPC_CONTEXT_DESTROYED        (EVENT_NON_ERR_DRV_START + 6)
#define SPH_IPC_CREATE_DEVNET_SUCCESS    (EVENT_NON_ERR_DRV_START + 7)
#define SPH_IPC_DEVNET_DESTROYED         (EVENT_NON_ERR_DRV_START + 8)
#define SPH_IPC_CREATE_INFREQ_SUCCESS    (EVENT_NON_ERR_DRV_START + 9)
#define SPH_IPC_INFREQ_DESTROYED         (EVENT_NON_ERR_DRV_START + 10)
#define SPH_IPC_RECOVER_CONTEXT_SUCCESS  (EVENT_NON_ERR_DRV_START + 11)
#define SPH_IPC_THERMAL_TRIP_EVENT       (EVENT_NON_ERR_DRV_START + 12)
#define SPH_IPC_DEVNET_ADD_RES_SUCCESS   (EVENT_NON_ERR_DRV_START + 13)
#define SPH_IPC_DEVICE_STATE_CHANGED     (EVENT_NON_ERR_DRV_START + 14)
#define SPH_IPC_DEVNET_RESOURCES_RESERVATION_SUCCESS  (EVENT_NON_ERR_DRV_START + 15)
#define SPH_IPC_DEVNET_RESOURCES_RELEASE_SUCCESS  (EVENT_NON_ERR_DRV_START + 16)
#define SPH_IPC_CREATE_CHANNEL_SUCCESS   (EVENT_NON_ERR_DRV_START + 17)
#define SPH_IPC_CHANNEL_DESTROYED        (EVENT_NON_ERR_DRV_START + 18)
#define SPH_IPC_CHANNEL_SET_RB_SUCCESS   (EVENT_NON_ERR_DRV_START + 19)
#define SPH_IPC_CHANNEL_MAP_HOSTRES_SUCCESS   (EVENT_NON_ERR_DRV_START + 20)
#define SPH_IPC_CHANNEL_UNMAP_HOSTRES_SUCCESS (EVENT_NON_ERR_DRV_START + 21)
#define SPH_IPC_ABORT_REQUEST            (EVENT_NON_ERR_DRV_START + 22)
#define SPH_IPC_GET_FIFO                 (EVENT_NON_ERR_DRV_START + 23)
#define SPH_IPC_CREATE_CMD_SUCCESS       (EVENT_NON_ERR_DRV_START + 24)
#define SPH_IPC_CMD_DESTROYED            (EVENT_NON_ERR_DRV_START + 25)
#define SPH_IPC_EXECUTE_CMD_COMPLETE     (EVENT_NON_ERR_DRV_START + 26)
#define SPH_IPC_DEVNET_SET_PROPERTY_SUCCESS  (EVENT_NON_ERR_DRV_START + 27)
#define SPH_IPC_EXECUTE_CPYLST_SUCCESS   (EVENT_NON_ERR_DRV_START + 28)

/* non-critical error event codes */
#define SPH_IPC_CREATE_CONTEXT_FAILED    (EVENT_ERR_DRV_START + 0)
#define SPH_IPC_CREATE_DEVRES_FAILED     (EVENT_ERR_DRV_START + 1)
#define SPH_IPC_CREATE_COPY_FAILED       (EVENT_ERR_DRV_START + 2)
#define SPH_IPC_DESTROY_CONTEXT_FAILED   (EVENT_ERR_DRV_START + 3)
#define SPH_IPC_DESTROY_DEVRES_FAILED    (EVENT_ERR_DRV_START + 4)
#define SPH_IPC_DESTROY_COPY_FAILED      (EVENT_ERR_DRV_START + 5)
#define SPH_IPC_CREATE_SYNC_FAILED       (EVENT_ERR_DRV_START + 6)
#define SPH_IPC_ERROR_SUB_RESOURCE_LOAD_FAILED      (EVENT_ERR_DRV_START + 7)
#define SPH_IPC_CREATE_DEVNET_FAILED     (EVENT_ERR_DRV_START + 8)
#define SPH_IPC_DESTROY_DEVNET_FAILED    (EVENT_ERR_DRV_START + 9)
#define SPH_IPC_CREATE_INFREQ_FAILED     (EVENT_ERR_DRV_START + 10)
#define SPH_IPC_DESTROY_INFREQ_FAILED    (EVENT_ERR_DRV_START + 11)
#define SPH_IPC_RECOVER_CONTEXT_FAILED   (EVENT_ERR_DRV_START + 12)
#define SPH_IPC_ERROR_MCE_CORRECTABLE    (EVENT_ERR_DRV_START + 13)
#define SPH_IPC_ERROR_MCE_UNCORRECTABLE  (EVENT_ERR_DRV_START + 14)
#define SPH_IPC_DEVNET_ADD_RES_FAILED    (EVENT_ERR_DRV_START + 15)
#define SPH_IPC_DEVNET_RESOURCES_RESERVATION_FAILED (EVENT_ERR_DRV_START + 16)
#define SPH_IPC_DEVNET_RESOURCES_RELEASE_FAILED     (EVENT_ERR_DRV_START + 17)
#define SPH_IPC_CREATE_CHANNEL_FAILED    (EVENT_ERR_DRV_START + 18)
#define SPH_IPC_DESTROY_CHANNEL_FAILED   (EVENT_ERR_DRV_START + 19)
#define SPH_IPC_CHANNEL_SET_RB_FAILED    (EVENT_ERR_DRV_START + 20)
#define SPH_IPC_CREATE_CMD_FAILED        (EVENT_ERR_DRV_START + 21)
#define SPH_IPC_DESTROY_CMD_FAILED       (EVENT_ERR_DRV_START + 22)
#define SPH_IPC_CHANNEL_MAP_HOSTRES_FAILED   (EVENT_ERR_DRV_START + 23)
#define SPH_IPC_CHANNEL_UNMAP_HOSTRES_FAILED (EVENT_ERR_DRV_START + 24)
#define SPH_IPC_DEVNET_SET_PROPERTY_FAILED  (EVENT_ERR_DRV_START + 25)

/* context critical error event codes */
#define SPH_IPC_ERROR_RUNTIME_LAUNCH     (EVENT_CONTEXT_FATAL_START + 0)
#define SPH_IPC_ERROR_RUNTIME_DIED       (EVENT_CONTEXT_FATAL_START + 1)

#define SPH_IPC_EXECUTE_COPY_FAILED      (EVENT_CONTEXT_FATAL_DRV_START + 0)
#define SPH_IPC_SCHEDULE_INFREQ_FAILED   (EVENT_CONTEXT_FATAL_DRV_START + 1)
#define SPH_IPC_CTX_MCE_UNCORRECTABLE    (EVENT_CONTEXT_FATAL_DRV_START + 2)
#define SPH_IPC_EXECUTE_CPYLST_FAILED      (EVENT_CONTEXT_FATAL_DRV_START + 3)

/* card critical error event codes */
#define SPH_IPC_ERROR_OS_CRASHED          (EVENT_CARD_FATAL_START + 0)
#define SPH_IPC_ERROR_PCI_ERROR           (EVENT_CARD_FATAL_START + 1)
#define SPH_IPC_ERROR_CARD_RESET          (EVENT_CARD_FATAL_START + 2)
#define SPH_IPC_ERROR_MCE_UNCORRECTABLE_FATAL  (EVENT_CARD_FATAL_START + 3)

enum event_val {
	SPH_IPC_NO_SUCH_CONTEXT		= 1,
	SPH_IPC_NO_SUCH_DEVRES		= 2,
	SPH_IPC_NO_SUCH_COPY		= 3,
	SPH_IPC_NO_SUCH_NET		= 4,
	SPH_IPC_NO_SUCH_INFREQ		= 5,
	SPH_IPC_ALREADY_EXIST		= 6,
	SPH_IPC_NO_DAEMON		= 7,
	SPH_IPC_NO_MEMORY		= 8,
	SPH_IPC_RUNTIME_FAILED		= 9,
	SPH_IPC_RUNTIME_LAUNCH_FAILED	= 10,
	SPH_IPC_DMA_ERROR		= 11,
	SPH_IPC_RUNTIME_NOT_SUPPORTED	= 12,
	SPH_IPC_RUNTIME_INVALID_EXECUTABLE_NETWORK_BINARY = 13,
	SPH_IPC_RUNTIME_INFER_MISSING_RESOURCE        = 14,
	SPH_IPC_RUNTIME_INFER_EXEC_ERROR              = 15,
	SPH_IPC_RUNTIME_INFER_SCHEDULE_ERROR          = 16,
	SPH_IPC_CONTEXT_BROKEN                        = 17,
	SPH_IPC_DEVNET_RESERVE_INSUFFICIENT_RESOURCES = 18,
	SPH_IPC_TIMEOUT_EXCEEDED        = 19,
	SPH_IPC_ECC_ALLOC_FAILED        = 20,
	SPH_IPC_NO_SUCH_CHANNEL         = 21,
	SPH_IPC_NO_SUCH_CMD             = 22,
	SPH_IPC_NO_SUCH_HOSTRES         = 23,
	SPH_IPC_DEVNET_EDIT_BUSY        = 24,
	SPH_IPC_DEVNET_EDIT_ERROR       = 25,
	SPH_IPC_NOT_SUPPORTED           = 26,

	//Non failure events
	SPH_IPC_CMDLIST_FINISHED       = 128,
};

int eventValToSphErrno(enum event_val eventVal);

/**
 *
 * eventCode                        eventVal         ContextID   objID                         objID_2
 * ---------                        --------         ---------   ------                        ------
 * SPH_IPC_CREATE_CONTEXT_SUCCESS   0                Valid       Not-Valid                     Not-Valid
 * SPH_IPC_CREATE_DEVRES_SUCCESS    bid (for p2p     Valid       Device resource protocolID    offset (valid for p2p resource only)
 * SPH_IPC_CREATE_COPY_SUCCESS      0                Valid       Copy handle protocolID        Not-Valid
 * SPH_IPC_CREATE_DEVNET_SUCCESS    0                Valid       Device network protocolID     Not-Valid
 * SPH_IPC_CREATE_INFREQ_SUCCESS    devnetID         Valid       inf req protocolID            Not-Valid
 * SPH_IPC_EXECUTE_COPY_SUCCESS     0                Valid       Copy handle protocolID        Not-Valid or Cmd list protocolID(in case Cmd list complete)
 * SPH_IPC_CREATE_CHANNEL_SUCCESS   0                Not-Valid   Channel protocolID            Not-Valid
 * SPH_IPC_CHANNEL_SET_RB_SUCCESS   0                Not-Valid   Channel protocolID            rb_id
 * SPH_IPC_CHANNEL_MAP_HOSTRES_SUCCESS   0           Not-Valid   Channel protocolID            Hostres id
 * SPH_IPC_CHANNEL_UNMAP_HOSTRES_SUCCESS 0           Not-Valid   Channel protocolID            Hostres id
 * SPH_IPC_DEVRES_DESTROYED         0                Valid       Device resource protocolID    Not-Valid
 * SPH_IPC_COPY_DESTROYED           0                Valid       Copy handle protocolID        Not-Valid
 * SPH_IPC_CONTEXT_DESTROYED        0                Valid       Not-Valid                     Not-Valid
 * SPH_IPC_DEVNET_DESTROYED         0                Valid       Device network protocolID     Not-Valid
 * SPH_IPC_INFREQ_DESTROYED         0                Valid       inf req protocolID            devnetID
 * SPH_IPC_CHANNEL_DESTROYED        0                Not-valid   channel protocolID            Not-Valid
 * SPH_IPC_CHANNEL_SET_RB_FAILED    0                Not-valid   channel protocolID            rb_id
 * SPH_IPC_CREATE_CMD_SUCCESS       0                Valid       Cmd list protocolID           Not-Valid
 * SPH_IPC_CMD_DESTROYED            0                Valid       Cmd list protocolID           Not-Valid
 * SPH_IPC_EXECUTE_CMD_COMPLETE     0                Valid       Cmd list protocolID           Not-Valid
 *
 * SPH_IPC_EXECUTE_CPYLST_SUCCESS   0 or
 *                                  enum event_val
 *                                 (SPH_IPC_EXECUTE_CPYLST_SUCCESS)
 *                                                   Valid  Cmd list protocolID           Copy index in Cmd list
 *
 * SPH_IPC_CREATE_CONTEXT_FAILED    enum event_val   Valid       Not-Valid                     Not-Valid
 * SPH_IPC_CREATE_DEVRES_FAILED     enum event_val   Valid       Device resource protocolID    Not-Valid
 * SPH_IPC_CREATE_COPY_FAILED       enum event_val   Valid       Copy handle protocolID        Not-Valid
 * SPH_IPC_CREATE_DEVNET_FAILED     enum event_val   Valid       Device network protocolID     Not-Valid
 * SPH_IPC_CREATE_INFREQ_FAILED     enum event_val   Valid       inf req protocolID            devnetID
 * SPH_IPC_DESTROY_CONTEXT_FAILED   enum event_val   Valid       Not-Valid                     Not-Valid
 * SPH_IPC_DESTROY_DEVRES_FAILED    enum event_val   Valid       Device resource protocolID    Not-Valid
 * SPH_IPC_DESTROY_COPY_FAILED      enum event_val   Valid       Copy handle protocolID        Not-Valid
 * SPH_IPC_DESTROY_DEVNET_FAILED    enum event_val   Valid       Device network protocolID     Not-Valid
 * SPH_IPC_DESTROY_INFREQ_FAILED    enum event_val   Valid       inf req protocolID            devnetID
 * SPH_IPC_EXECUTE_COPY_FAILED      enum event_val   Valid       Copy handle protocolID        Not-Valid or Cmd list protocolID
 * SPH_IPC_SCHEDULE_INFREQ_FAILED   enum event_val   Valid       inf req protocolID            devnetID
 * SPH_IPC_CREATE_SYNC_FAILED       enum event_val   Valid       syncSeq value                 Not-Valid
 * SPH_IPC_CREATE_CHANNEL_FAILED    enum event_val   Not-Valid   Channel protocolID            Not-Valid
 * SPH_IPC_DESTROY_CHANNEL_FAILED   enum event_val   Not-Valid   Channel protocolID            Not-Valid
 * SPH_IPC_CHANNEL_MAP_HOSTRES_FAILED   enum         Not-Valid   Channel protocolID            Hostres id
 * SPH_IPC_CHANNEL_UNMAP_HOSTRES_FAILED enum         Not-Valid   Channel protocolID            Hostres id
 * SPH_IPC_ERROR_RUNTIME_LAUNCH     0                Valid       Not-Valid                     Not-Valid
 * SPH_IPC_ERROR_RUNTIME_DIED       0                Valid       Not-Valid                     Not-Valid
 * SPH_IPC_ERROR_OS_CRASHED         0                Not-Valid   dump_size (low 16bits)        dump_size (high 16bits)
 * SPH_IPC_ERROR_PCI_ERROR          error_type (1)   Not-Valid   Not-Valid                     Not-Valid
 * SPH_IPC_ERROR_CARD_RESET         0 (2)            Not-Valid   Not-Valid                     Not-Valid
 * SPH_IPC_THERMAL_TRIP_EVENT       trip_point_num   Not-Valid   trip temperature              event temperature
 * SPH_IPC_DEVICE_STATE_CHANGED     0 (3)            DevState[0-15] DevState[16-31]            Not-Valid
 * SPH_IPC_GET_FIFO                 tr_id            NotValid    base address offset in pages  Not-Valid
 * SPH_IPC_CREATE_CMD_FAILED        enum event_val   Valid       Cmd list protocolID           Not-Valid
 * SPH_IPC_DESTROY_CMD_FAILED       enum event_val   Valid       Cmd list protocolID           Not-Valid
 * SPH_IPC_EXECUTE_CPYLST_FAILED    enum event_val   Valid       Cmd list protocolID           Copy index in Cmd list
 * SPH_IPC_ABORT_REQUEST            0 (4)            Not-Valid   Not-Valid                     Not-Valid
 *
 * Comments:
 *    (1) - this event is generated by the host. error_type is value passed to
 *    pci_error_detected callback, see src/driver/host/sphdrv_pcie.h for
 *    possible values.
 *
 *    (2) - this event is generated by the host when a card reset is forced.
 *    (3) - host generated event when device state changes
 *    (4) - host generated event when "nnpi_ctl disable -abort" is requested
 */
#endif
