/********************************************
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 ********************************************/

#include "sphcs_pcie.h"
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/reboot.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/atomic.h>
#define ELBI_BASE         0x200000  /* MMIO offset of ELBI registers */
#include "nnp_elbi.h"
#include "nnp_debug.h"
#include "sph_log.h"
#include "nnp_time.h"
#include "sphcs_hw_utils.h"
#include "sphcs_sw_counters.h"
#include "sphcs_trace.h"
#include "int_stats.h"
#include "nnp_boot_defs.h"

/*
 * SpringHill PCI card identity settings
 */
#define NNP_PCI_DEVICE_ID		0x45c2
#define NNP_PCI_VENDOR_ID		PCI_VENDOR_ID_INTEL
#define NNP_PCI_DEVFN                   0
#define NNP_PCI_MMIO_BAR                0

/*
 * DMA registers
 */
#define NUM_H2C_CHANNELS  4
#define NUM_C2H_CHANNELS  4
#define TOTAL_CHANNELS   (NUM_H2C_CHANNELS + NUM_C2H_CHANNELS)

#define DMA_READ_ENGINE_EN_OFF          0x38002C
#define DMA_READ_DOORBELL_OFF           0x380030
#define DMA_READ_INT_STATUS_OFF         0x3800A0
#define DMA_READ_INT_MASK_OFF           0x3800A8
#define DMA_READ_INT_CLEAR_OFF          0x3800AC
#define DMA_READ_ERR_STATUS_LOW_OFF     0x3800B4
#define DMA_READ_ERR_STATUS_HIGH_OFF    0x3800B8

#define DMA_WRITE_ENGINE_EN_OFF         0x38000C
#define DMA_WRITE_DOORBELL_OFF          0x380010
#define DMA_WRITE_INT_STATUS_OFF        0x38004C
#define DMA_WRITE_INT_MASK_OFF          0x380054
#define DMA_WRITE_INT_CLEAR_OFF         0x380058
#define DMA_WRITE_ERR_STATUS_OFF        0x38005C

#define DMA_CH_CONTROL1_OFF_WRCH(i)     (0x380200 + (i)*0x200)
#define DMA_CH_CONTROL2_OFF_WRCH(i)     (0x380204 + (i)*0x200)
#define DMA_TRANSFER_SIZE_OFF_WRCH(i)   (0x380208 + (i)*0x200)
#define DMA_SAR_LOW_OFF_WRCH(i)         (0x38020C + (i)*0x200)
#define DMA_SAR_HIGH_OFF_WRCH(i)        (0x380210 + (i)*0x200)
#define DMA_DAR_LOW_OFF_WRCH(i)         (0x380214 + (i)*0x200)
#define DMA_DAR_HIGH_OFF_WRCH(i)        (0x380218 + (i)*0x200)
#define DMA_LLP_LOW_OFF_WRCH(i)         (0x38021C + (i)*0x200)
#define DMA_LLP_HIGH_OFF_WRCH(i)        (0x380220 + (i)*0x200)

#define DMA_CH_CONTROL1_OFF_RDCH(i)     (0x380300 + (i)*0x200)
#define DMA_CH_CONTROL2_OFF_RDCH(i)     (0x380304 + (i)*0x200)
#define DMA_TRANSFER_SIZE_OFF_RDCH(i)   (0x380308 + (i)*0x200)
#define DMA_SAR_LOW_OFF_RDCH(i)         (0x38030C + (i)*0x200)
#define DMA_SAR_HIGH_OFF_RDCH(i)        (0x380310 + (i)*0x200)
#define DMA_DAR_LOW_OFF_RDCH(i)         (0x380314 + (i)*0x200)
#define DMA_DAR_HIGH_OFF_RDCH(i)        (0x380318 + (i)*0x200)
#define DMA_LLP_LOW_OFF_RDCH(i)         (0x38031C + (i)*0x200)
#define DMA_LLP_HIGH_OFF_RDCH(i)        (0x380320 + (i)*0x200)

/* LL mode local abort interrupt*/
#define DMA_WRITE_LL_ERR_EN_OFF 0x380090
#define DMA_READ_LL_ERR_EN_OFF  0x3800c4

/* DMA channel weights control registers */
#define DMA_WRITE_CHAN_ARB_WEIGHT_LOW_REG			0x380018
#define DMA_WRITE_CHAN_ARB_WEIGHT_HIGH_REG			0x38001C
#define DMA_READ_CHAN_ARB_WEIGHT_LOW_REG			0x380038
#define DMA_READ_CHAN_ARB_WEIGHT_HIGH_REG			0x38003C

/* DMA control register */
#define DMA_CTRL_CB				BIT(0)
#define DMA_CTRL_TCB				BIT(1)
#define DMA_CTRL_LLP				BIT(2)
#define DMA_CTRL_LIE				BIT(3)
#define DMA_CTRL_RIE				BIT(4)
#define DMA_CTRL_CCS				BIT(8)
#define DMA_CTRL_LLE				BIT(9)
#define DMA_CTRL_TD				BIT(26)

#define DMA_CTRL_CHANNEL_STATUS_OFF		(5)
#define DMA_CTRL_CHANNEL_STATUS_MASK		GENMASK(6, 5)
#define DMA_CTRL_CHANNEL_STATUS_RUNNING		(1)
#define DMA_CTRL_CHANNEL_STATUS_HALTED		(2)
#define DMA_CTRL_CHANNEL_STATUS_STOPPED		(3)

/* DOORBELL REG*/
#define DMA_DOORBELL_STOP_OFF			(31)

#define DMA_WR_ABORT_INT_STATUS_OFF		(16)
#define DMA_WR_DONE_INT_STATUS_OFF		(0)
#define DMA_WR_ABORT_INT_MASK_OFF		(16)
#define DMA_WR_DONE_INT_MASK_OFF		(0)
#define DMA_WR_ABORT_INT_CLEAR_OFF		(16)
#define DMA_WR_DONE_INT_CLEAR_OFF		(0)
#define DMA_WR_LL_ELEM_FETCH_ERR_OFF		(16)
#define DMA_WR_APP_READ_ERR_OFF			(0)

#define DMA_RD_ABORT_INT_STATUS_OFF		(16)
#define DMA_RD_DONE_INT_STATUS_OFF		(0)
#define DMA_RD_ABORT_INT_MASK_OFF		(16)
#define DMA_RD_DONE_INT_MASK_OFF		(0)
#define DMA_RD_ABORT_INT_CLEAR_OFF		(16)
#define DMA_RD_DONE_INT_CLEAR_OFF		(0)
#define DMA_RD_LLE_FETCH_ERR_OFF		(16)
#define DMA_RD_APP_WRITE_ERR_OFF		(0)
#define DMA_RD_DATA_POISIONING_OFF		(24)
#define DMA_RD_CPL_TIMEOUT_OFF			(16)
#define DMA_RD_CPL_ABORT_OFF			(8)
#define DMA_RD_UNSUPPORTED_REQ_OFF		(0)

#define DMA_WR_LLE_FETCH_ERR_OFF		(16)
#define DMA_WR_APP_READ_ERR_OFF			(0)

/* Channel LL Local/Remote Abort Interrupt Enable */
#define DMA_WR_CHAN_LLLAIE_OFF			(16)
#define DMA_WR_CHAN_LLRAIE_OFF			(0)
#define DMA_RD_CHAN_LLLAIE_OFF			(16)
#define DMA_RD_CHAN_LLRAIE_OFF			(0)

#define DMA_SET_CHAN_BIT(idx, offset)	(BIT(idx) << (offset))

/*
 * iATU registers
 */
#define IATU_REGION_CTRL_1_INBOUND_OFF(i)    (0x300000 + 0x100 + (i)*0x200)
#define IATU_REGION_CTRL_2_INBOUND_OFF(i)    (0x300000 + 0x104 + (i)*0x200)
#define IATU_LWR_BASE_ADDR_INBOUND_OFF(i)    (0x300000 + 0x108 + (i)*0x200)
#define IATU_UPPER_BASE_ADDR_INBOUND_OFF(i)  (0x300000 + 0x10C + (i)*0x200)
#define IATU_LIMIT_ADDR_INBOUND_OFF(i)       (0x300000 + 0x110 + (i)*0x200)
#define IATU_UPPER_LIMIT_ADDR_INBOUND_OFF(i) (0x300000 + 0x120 + (i)*0x200)
#define IATU_LWR_TARGET_ADDR_INBOUND_OFF(i)  (0x300000 + 0x114 + (i)*0x200)
#define IATU_UPPER_TARGET_ADDR_INBOUND_OFF(i)  (0x300000 + 0x118 + (i)*0x200)

/* Amount of mapped memory - 64 MB */
#define MAPPED_MEMORY_SIZE (64ULL << 20)

static const char nnp_driver_name[] = "nnp_pcie";
static struct sphcs_pcie_callbacks *s_callbacks;

#ifdef ULT
static struct dentry *s_debugfs_dir;
static DEFINE_INT_STAT(int_stats, 8);
#endif

bool no_dma_retries;
module_param(no_dma_retries,  bool, 0400);

static int dma_lli_split = 3;
module_param(dma_lli_split,  int, 0600);

static int dma_lli_min_split = 20;
module_param(dma_lli_min_split,  int, 0600);

/* interrupt mask bits we enable and handle at interrupt level */
static u32 s_host_status_int_mask =
		   ELBI_IOSF_STATUS_RESPONSE_FIFO_READ_UPDATE_MASK |
		   ELBI_IOSF_STATUS_BME_CHANGE_MASK |
		   ELBI_IOSF_STATUS_LINE_FLR_MASK |
		   ELBI_IOSF_STATUS_HOT_RESET_MASK |
		   ELBI_IOSF_STATUS_PME_TURN_OFF_MASK;

/* interrupt mask bits we enable and handle at threaded interrupt level */
static u32 s_host_status_threaded_mask =
		   ELBI_IOSF_STATUS_DMA_INT_MASK |
		   ELBI_IOSF_STATUS_COMMAND_FIFO_NEW_COMMAND_MASK |
		   ELBI_IOSF_STATUS_DOORBELL_MASK;

static enum {
	SPH_FLR_MODE_WARM = 0,
	SPH_FLR_MODE_COLD,
	SPH_FLR_MODE_IGNORE,
	SPH_FLR_MODE_CAPSULE
} s_flr_mode;

struct sph_dma_channel {
	u64   usTime;
};

struct nnp_pci_device {
	struct pci_dev *pdev;
	struct device  *dev;
	struct sphcs           *sphcs;
	struct sphcs_dma_sched *dmaSched;

	struct nnp_memdesc mmio;

	spinlock_t      irq_lock;
	u64             command_buf[ELBI_COMMAND_FIFO_DEPTH];
	atomic_t        new_command;
	atomic64_t      dma_status;
	atomic_t        doorbell_changed;

	spinlock_t      respq_lock;
	u32             respq_free_slots;
	u32             resp_fifo_read_update_count;

	u32               host_status;
	wait_queue_head_t host_status_wait;

	u32               host_doorbell_val;
	u32               card_doorbell_val;

	bool              bus_master_en;

	spinlock_t      dma_lock_irq;

	struct sph_dma_channel h2c_channels[NUM_H2C_CHANNELS];
	struct sph_dma_channel c2h_channels[NUM_C2H_CHANNELS];
};

struct sph_dma_data_element {
	uint32_t control;
	uint32_t transfer_size;
	uint32_t source_address_low;
	uint32_t source_address_high;
	uint32_t dest_address_low;
	uint32_t dest_address_high;
};
NNP_STATIC_ASSERT(sizeof(struct sph_dma_data_element) == 6 * sizeof(uint32_t), "struct sph_dma_data_element size mismatch");

struct sph_lli_header {
	struct sph_dma_data_element *cut_element;
	uint32_t cut_element_transfer_size;
	uint32_t cut_element_ctrl_flag;
	uint32_t cut_list_idx;
	uint32_t num_lists_keep;
	uint64_t xfer_size_keep;
	struct sph_dma_data_element cut_element_next;
	uint64_t size;
};

static int sphcs_sph_init_dma_engine(void *hw_handle);

static inline void nnp_mmio_write(struct nnp_pci_device *nnp_pci,
				  uint32_t               off,
				  uint32_t               val)
{
	//DO_TRACE(trace_pep_mmio('w', off - ELBI_BASE, val));
	iowrite32(val, nnp_pci->mmio.va + off);
}

static inline uint32_t nnp_mmio_read(struct nnp_pci_device *nnp_pci,
				     uint32_t               off)
{
	uint32_t ret;

	ret = ioread32(nnp_pci->mmio.va + off);
	//DO_TRACE(trace_pep_mmio('r', off - ELBI_BASE, ret));

	return ret;
}

static ssize_t sph_show_flr_mode(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	switch (s_flr_mode) {
	case SPH_FLR_MODE_COLD:
		return snprintf(buf, PAGE_SIZE, "cold\n");
	case SPH_FLR_MODE_IGNORE:
		return snprintf(buf, PAGE_SIZE, "ignore\n");
	case SPH_FLR_MODE_CAPSULE:
		return snprintf(buf, PAGE_SIZE, "capsule\n");
	case SPH_FLR_MODE_WARM:
	default:
		return snprintf(buf, PAGE_SIZE, "warm\n");
	}
}

static ssize_t sph_store_flr_mode(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct nnp_pci_device *nnp_pci;

	nnp_pci = pci_get_drvdata(pdev);
	if (!nnp_pci)
		return count;

	if (count >= 4 && !strncmp(buf, "warm", 4))
		s_flr_mode = SPH_FLR_MODE_WARM;
	else if (count >= 4 && !strncmp(buf, "cold", 4))
		s_flr_mode = SPH_FLR_MODE_COLD;
	else if (count >= 6 && !strncmp(buf, "ignore", 6))
		s_flr_mode = SPH_FLR_MODE_IGNORE;
	else if (count >= 7 && !strncmp(buf, "capsule", 7))
		s_flr_mode = SPH_FLR_MODE_CAPSULE;
	else
		return -EINVAL;

	nnp_mmio_write(nnp_pci,
		       ELBI_CPU_STATUS_2,
		       s_flr_mode);

	sph_log_debug(GENERAL_LOG, "wrote 0x%x to cpu_status_2 (0x%x)\n",
		      s_flr_mode,
		      nnp_mmio_read(nnp_pci, ELBI_CPU_STATUS_2));

	return count;
}

static DEVICE_ATTR(flr_mode, 0644, sph_show_flr_mode, sph_store_flr_mode);

static ssize_t sph_show_link_width(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct nnp_pci_device *nnp_pci;
	uint16_t link_status;
	ssize_t ret = 0;
	u8 pos;
	u16 ent;
	u8 ext;
	bool found = false;
	int loops = 0;

	nnp_pci = pci_get_drvdata(pdev);
	if (!nnp_pci)
		return -EINVAL;

	/*
	 * card config space as seen from host is mapped to offset 0 of BAR0
	 * walk the config to find pci express capability
	 */
	pos = ioread8(nnp_pci->mmio.va + PCI_CAPABILITY_LIST);
	do {
		pos &= ~3;
		if (pos < 0x40)
			break;
		ent = ioread16(nnp_pci->mmio.va + pos);
		ext = ent & 0xff;
		if (ext == PCI_CAP_ID_EXP) {
			found = true;
			break;
		} else if (ext == 0xff)
			break;

		pos = (ent >> 8) & 0xff;
	} while (loops++ < 20);

	if (!found) {
		ret += snprintf(&buf[ret], PAGE_SIZE - ret, "Could not find EXP cap\n");
	} else {
		link_status = ioread16(nnp_pci->mmio.va + pos + PCI_EXP_LNKSTA);
		ret += snprintf(&buf[ret], PAGE_SIZE - ret, "%d\n", (link_status >> 4) & 0x3f);
	}

	return ret;
}

static DEVICE_ATTR(link_width, 0444, sph_show_link_width, NULL);

static void nnp_process_commands(struct nnp_pci_device *nnp_pci)
{
	u32 command_iosf_control;
	u32 read_pointer;
	u32 write_pointer;
	u32 avail_slots;
	u32 low;
	u64 high;
	int i;

	command_iosf_control = nnp_mmio_read(nnp_pci,
					     ELBI_COMMAND_IOSF_CONTROL);
	read_pointer = ELBI_BF_GET(command_iosf_control,
				   ELBI_COMMAND_IOSF_CONTROL_READ_POINTER_MASK,
				   ELBI_COMMAND_IOSF_CONTROL_READ_POINTER_SHIFT);
	write_pointer = ELBI_BF_GET(command_iosf_control,
				    ELBI_COMMAND_IOSF_CONTROL_WRITE_POINTER_MASK,
				    ELBI_COMMAND_IOSF_CONTROL_WRITE_POINTER_SHIFT);
	/* Commands to read */
	avail_slots = write_pointer - read_pointer;

	if (!avail_slots)
		return;

	for (i = 0; i < avail_slots; i++) {
		read_pointer = (read_pointer + 1) % ELBI_COMMAND_FIFO_DEPTH;
		low = nnp_mmio_read(nnp_pci,
				    ELBI_COMMAND_FIFO_LOW(read_pointer));
		high = nnp_mmio_read(nnp_pci,
				     ELBI_COMMAND_FIFO_HIGH(read_pointer));
		nnp_pci->command_buf[i] = (high << 32) | low;
	}

	//
	// HW restriction - we cannot update the read pointer with the same
	// value it currently have. This will be the case if we need to advance
	// it by FIFO_DEPTH locations. In this case we will update it in two
	// steps, first advance by 1, then to the proper value.
	//
	if (avail_slots == ELBI_COMMAND_FIFO_DEPTH) {
		u32 next_read_pointer = (read_pointer + 1) % ELBI_COMMAND_FIFO_DEPTH;

		ELBI_BF_SET(command_iosf_control,
			    next_read_pointer,
			    ELBI_COMMAND_IOSF_CONTROL_READ_POINTER_MASK,
			    ELBI_COMMAND_IOSF_CONTROL_READ_POINTER_SHIFT);

		nnp_mmio_write(nnp_pci,
			       ELBI_COMMAND_IOSF_CONTROL,
			       command_iosf_control);
	}

	ELBI_BF_SET(command_iosf_control,
		    read_pointer,
		    ELBI_COMMAND_IOSF_CONTROL_READ_POINTER_MASK,
		    ELBI_COMMAND_IOSF_CONTROL_READ_POINTER_SHIFT);

	nnp_mmio_write(nnp_pci,
		       ELBI_COMMAND_IOSF_CONTROL,
		       command_iosf_control);

	if (nnp_pci->sphcs)
		s_callbacks->process_messages(nnp_pci->sphcs,
					      nnp_pci->command_buf,
					      avail_slots);
}

static void set_bus_master_state(struct nnp_pci_device *nnp_pci)
{
	u32 iosf_status;
	bool bme_en;

	iosf_status = nnp_mmio_read(nnp_pci, ELBI_IOSF_STATUS);
	bme_en = (iosf_status & ELBI_IOSF_STATUS_LINE_BME_MASK) != 0;
	if (bme_en != nnp_pci->bus_master_en) {
		/* init DMA engine when bus master transition to enable */
		if (bme_en)
			sphcs_sph_init_dma_engine(nnp_pci);

		nnp_pci->bus_master_en = bme_en;
	}
}

static void sph_warm_reset(void)
{
	if (s_flr_mode == SPH_FLR_MODE_COLD) {
		/*
		 * Cold reset - since reboot is configured for warm reset
		 * we directly issue cold reset to port CF9
		 */
		u8 boot_mode;
		u8 cf9;

		boot_mode = 0xe;  /* 0x6 = WARM, 0xe = COLD */
		cf9 = inb(0xcf9) & ~boot_mode;
		outb(cf9|2, 0xcf9); /* Request hard reset */
		udelay(50);
		outb(cf9|boot_mode, 0xcf9); /* Actually Do reset */
		udelay(50);
	} else if (s_flr_mode != SPH_FLR_MODE_IGNORE) {
		/*
		 * Warm reset
		 * Here we assume that the following argument was given
		 * in the linux command line: "reboot=p,w"
		 * That ensures that a warm reset will be initiated.
		 */
		emergency_restart();
	}
}

static void handle_dma_interrupt(struct nnp_pci_device *nnp_pci, u32 dma_read_status, u32 dma_write_status)
{
	u32 recovery_action;
	int i;

	/* handle h2c (READ channels) */
	if (dma_read_status) {
		u32 mask;

		for (i = 0; i < NUM_H2C_CHANNELS; i++) {
			u32 chan_status = 0;

			/* If DMA successfully completed */
			mask = DMA_SET_CHAN_BIT(i, DMA_RD_DONE_INT_STATUS_OFF);
			if (dma_read_status & mask) {
				chan_status = SPHCS_DMA_STATUS_DONE;
				recovery_action = SPHCS_RA_NONE;
			} else {
				/* If error occurred */
				mask = DMA_SET_CHAN_BIT(i, DMA_RD_ABORT_INT_STATUS_OFF);
				if (dma_read_status & mask) {
					u32 status_lo, status_hi;

					status_lo = nnp_mmio_read(nnp_pci,
								  DMA_READ_ERR_STATUS_LOW_OFF);
					status_hi = nnp_mmio_read(nnp_pci,
								  DMA_READ_ERR_STATUS_HIGH_OFF);

					chan_status = SPHCS_DMA_STATUS_FAILED;

					/* If Fatal error occurred */
					if ((status_lo & DMA_SET_CHAN_BIT(i, DMA_RD_APP_WRITE_ERR_OFF)) ||
							(status_lo & DMA_SET_CHAN_BIT(i, DMA_RD_LLE_FETCH_ERR_OFF)) ||
							(status_hi & DMA_SET_CHAN_BIT(i, DMA_RD_DATA_POISIONING_OFF)))
						recovery_action = SPHCS_RA_RESET_DMA;
					else if (!no_dma_retries)
						recovery_action = SPHCS_RA_RETRY_DMA;
					else
						recovery_action = SPHCS_RA_NONE;

					sph_log_err(DMA_LOG, "DMA error on read ch %d (no_dma_retries=%d) recovery=%s, status_hi=0x%x status_lo=0x%x\n",
						    i,
						    no_dma_retries,
						    recovery_action == SPHCS_RA_RESET_DMA ? "reset" :
						    recovery_action == SPHCS_RA_RETRY_DMA ? "retry" : "none",
						    status_hi,
						    status_lo);
				}
			}

			if (chan_status) {
				u64  usTime = (u64)0x0;

				if (nnp_pci->h2c_channels[i].usTime != 0)
					usTime = nnp_time_us() - nnp_pci->h2c_channels[i].usTime;

				/* send int upstream */
				if (nnp_pci->dmaSched)
					s_callbacks->dma.h2c_xfer_complete_int(nnp_pci->dmaSched, i, chan_status, recovery_action, (u32)usTime);

			}
		}
	}

	/* handle c2h (WRITE channels) */
	if (dma_write_status) {
		u32 mask;

		for (i = 0; i < NUM_C2H_CHANNELS; i++) {
			u32 chan_status = 0;

			mask = DMA_SET_CHAN_BIT(i, DMA_WR_DONE_INT_STATUS_OFF);
			if (dma_write_status & mask) {
				chan_status = SPHCS_DMA_STATUS_DONE;
				recovery_action = SPHCS_RA_NONE;
			} else {

				mask = DMA_SET_CHAN_BIT(i, DMA_WR_ABORT_INT_STATUS_OFF);
				if (dma_write_status & mask) {
					u32 status;

					chan_status = SPHCS_DMA_STATUS_FAILED;

					status = nnp_mmio_read(nnp_pci,
							       DMA_WRITE_ERR_STATUS_OFF);

					if ((status & DMA_SET_CHAN_BIT(i, DMA_WR_APP_READ_ERR_OFF)) ||
							(status & DMA_SET_CHAN_BIT(i, DMA_WR_LLE_FETCH_ERR_OFF)))
						recovery_action = SPHCS_RA_RESET_DMA;
					else
						recovery_action = SPHCS_RA_NONE;

					sph_log_err(DMA_LOG, "DMA error on write ch %d recovery=%s, status=0x%x\n",
						    i,
						    recovery_action == SPHCS_RA_RESET_DMA ? "reset" : "none",
						    status);
				}
			}


			if (chan_status) {
				u64 usTime = (u64)0x0;

				if (nnp_pci->c2h_channels[i].usTime > 0)
					usTime = nnp_time_us() - nnp_pci->c2h_channels[i].usTime;

				/* send int upstream */
				if (nnp_pci->dmaSched)
					s_callbacks->dma.c2h_xfer_complete_int(nnp_pci->dmaSched, i, chan_status, recovery_action, (u32)usTime);

			}
		}
	}
}

static void read_and_clear_dma_status(struct nnp_pci_device *nnp_pci, u32 *dma_read_status, u32 *dma_write_status)
{
	//get and store DMA interrupt status
	*dma_read_status  = nnp_mmio_read(nnp_pci, DMA_READ_INT_STATUS_OFF);
	*dma_write_status = nnp_mmio_read(nnp_pci, DMA_WRITE_INT_STATUS_OFF);

	/* handle h2c (READ channels) */
	if (*dma_read_status)
		/* clear int status */
		nnp_mmio_write(nnp_pci,
			       DMA_READ_INT_CLEAR_OFF,
			       *dma_read_status);

	/* handle c2h (WRITE channels) */
	if (*dma_write_status)
		/* clear int status */
		nnp_mmio_write(nnp_pci,
			       DMA_WRITE_INT_CLEAR_OFF,
			       *dma_write_status);
}


static irqreturn_t interrupt_handler(int irq, void *data)
{
	struct nnp_pci_device *nnp_pci = (struct nnp_pci_device *)data;
	irqreturn_t ret;
	unsigned long flags;
	u32 dma_read_status = 0, dma_write_status = 0;
	bool should_wake = false;

	NNP_SPIN_LOCK_IRQSAVE(&nnp_pci->irq_lock, flags);

	/*
	 * mask all interrupts (except LINE_FLR)
	 * We keep LINE_FLR un-masked since the p-code use that
	 * bit as an indication that our driver handle ANY warm reset
	 * request scenario. When this bit is set to 1, the p-code will handle
	 * the event.
	 */
	nnp_mmio_write(nnp_pci,
		       ELBI_IOSF_MSI_MASK,
		       ~((uint32_t)(ELBI_IOSF_STATUS_LINE_FLR_MASK)));

	nnp_pci->host_status = nnp_mmio_read(nnp_pci, ELBI_IOSF_STATUS);

#ifdef ULT
	INT_STAT_INC(int_stats,
		     (nnp_pci->host_status &
		      (s_host_status_int_mask | s_host_status_threaded_mask)));
#endif

	/* early exit if spurious interrupt */
	if ((nnp_pci->host_status &
	     (s_host_status_int_mask | s_host_status_threaded_mask)) == 0) {
		nnp_mmio_write(nnp_pci,
			       ELBI_IOSF_MSI_MASK,
			       ~(s_host_status_int_mask | s_host_status_threaded_mask));
		return IRQ_NONE;
	}

	if (nnp_pci->host_status &
	    ELBI_IOSF_STATUS_DMA_INT_MASK) {
		read_and_clear_dma_status(nnp_pci, &dma_read_status, &dma_write_status);
		atomic64_or((dma_read_status | (((u64)dma_write_status) << 32)), &nnp_pci->dma_status);
	}

	if (nnp_pci->host_status &
	    ELBI_IOSF_STATUS_COMMAND_FIFO_NEW_COMMAND_MASK) {
		atomic_set(&nnp_pci->new_command, 1);
	}

	nnp_mmio_write(nnp_pci,
		       ELBI_IOSF_STATUS,
		       nnp_pci->host_status & (s_host_status_int_mask | s_host_status_threaded_mask));

	if (nnp_pci->host_status &
	    ELBI_IOSF_STATUS_RESPONSE_FIFO_READ_UPDATE_MASK) {
		should_wake = true;
		nnp_pci->resp_fifo_read_update_count++;
	}

	if (nnp_pci->host_status &
	    ELBI_IOSF_STATUS_BME_CHANGE_MASK)
		set_bus_master_state(nnp_pci);

	if (nnp_pci->host_status &
	    ELBI_IOSF_STATUS_LINE_FLR_MASK) {
		sph_log_err(GENERAL_LOG, "FLR Requested from host !!\n");

		/* Let the host know the card is going down */
		nnp_mmio_write(nnp_pci, ELBI_HOST_PCI_DOORBELL_VALUE, 0);

		/* warm reset the card */
		sph_warm_reset();
	}

	if (nnp_pci->host_status &
	    ELBI_IOSF_STATUS_HOT_RESET_MASK) {
		sph_log_err(GENERAL_LOG, "Hot reset requested from host !!\n");

		/* Let the host know the card is going down */
		nnp_mmio_write(nnp_pci, ELBI_HOST_PCI_DOORBELL_VALUE, 0);

		/* warm reset the card */
		sph_warm_reset();
	}

	if (nnp_pci->host_status &
	    ELBI_IOSF_STATUS_PME_TURN_OFF_MASK) {
		sph_log_err(GENERAL_LOG, "PME turn off requested from host !!\n");

		/* Let the host know the card is going down */
		nnp_mmio_write(nnp_pci, ELBI_HOST_PCI_DOORBELL_VALUE, 0);

		/* warm reset the card */
		sph_warm_reset();
	}

	if (nnp_pci->sphcs &&
	    (nnp_pci->host_status &
	     ELBI_IOSF_STATUS_DOORBELL_MASK)) {
		u32 val = nnp_mmio_read(nnp_pci, ELBI_PCI_HOST_DOORBELL_VALUE);

		/* Issue card reset if requested from host */
		if (val & NNP_HOST_DRV_REQUEST_SELF_RESET_MASK) {
			sph_log_err(GENERAL_LOG, "Self reset requested from host !!\n");
			sph_warm_reset();
		} else if ((val & NNP_HOST_KEEP_ALIVE_MASK) !=
			   (nnp_pci->host_doorbell_val & NNP_HOST_KEEP_ALIVE_MASK)) {
			nnp_pci->card_doorbell_val &= ~(NNP_CARD_KEEP_ALIVE_MASK);
			nnp_pci->card_doorbell_val |= (val & NNP_HOST_KEEP_ALIVE_MASK);
			nnp_mmio_write(nnp_pci, ELBI_HOST_PCI_DOORBELL_VALUE, nnp_pci->card_doorbell_val);
		}

		nnp_pci->host_doorbell_val = val;
		atomic_set(&nnp_pci->doorbell_changed, 1);
	}

	if (nnp_pci->host_status & s_host_status_threaded_mask)
		ret = IRQ_WAKE_THREAD;
	else
		ret = IRQ_HANDLED;

	/* Enable desired interrupts */
	nnp_mmio_write(nnp_pci,
		       ELBI_IOSF_MSI_MASK,
		       ~(s_host_status_int_mask | s_host_status_threaded_mask));

	NNP_SPIN_UNLOCK_IRQRESTORE(&nnp_pci->irq_lock, flags);

	if (should_wake)
		wake_up_all(&nnp_pci->host_status_wait);

	return ret;
}

static irqreturn_t threaded_interrupt_handler(int irq, void *data)
{
	struct nnp_pci_device *nnp_pci = (struct nnp_pci_device *)data;
	u64 dma_status;

	if (atomic_xchg(&nnp_pci->doorbell_changed, 0))
		s_callbacks->host_doorbell_value_changed(nnp_pci->sphcs,
							 nnp_pci->host_doorbell_val);

	if (atomic_xchg(&nnp_pci->new_command, 0))
		nnp_process_commands(nnp_pci);

	dma_status = atomic64_xchg(&nnp_pci->dma_status, 0);
	if (dma_status != 0) {
		handle_dma_interrupt(nnp_pci,
				     (u32)(dma_status & 0xffffffff),
				     (u32)((dma_status >> 32) & 0xffffffff));
	}

	return IRQ_HANDLED;
}

static int nnp_setup_interrupts(struct nnp_pci_device *nnp_pci,
				struct pci_dev        *pdev)
{
	int rc;

	rc = pci_enable_msi(pdev);
	if (rc) {
		sph_log_err(START_UP_LOG, "Error enabling MSI. rc = %d\n", rc);
		return rc;
	}

	rc = request_threaded_irq(pdev->irq,
				  interrupt_handler,
				  threaded_interrupt_handler,
				  IRQF_ONESHOT,
				  "sph-msi",
				  nnp_pci);
	if (rc) {
		sph_log_err(START_UP_LOG, "Error allocating MSI interrupt\n");
		goto err_irq_req_fail;
	}

	sph_log_debug(START_UP_LOG, "nnp_pcie MSI irq setup done\n");

	return 0;

err_irq_req_fail:
	pci_disable_msi(pdev);
	return rc;
}

static void nnp_free_interrupts(struct nnp_pci_device *nnp_pci,
				struct pci_dev        *pdev)
{
	free_irq(pdev->irq, nnp_pci);
	pci_disable_msi(pdev);
}

static int sph_respq_write_mesg_nowait(struct nnp_pci_device *nnp_pci,
				       u64                   *msg,
				       u32                    size,
				       u32                   *read_update_count)
{
	u32 resp_pci_control;
	u32 read_pointer, write_pointer;
	unsigned long flags;
	int i;

	if (size < 1)
		return 0;

	NNP_SPIN_LOCK(&nnp_pci->respq_lock);

	if (nnp_pci->respq_free_slots < size) {
		/* read response fifo pointers and compute free slots in fifo */
		NNP_SPIN_LOCK_IRQSAVE(&nnp_pci->irq_lock, flags);
		resp_pci_control = nnp_mmio_read(nnp_pci,
						 ELBI_RESPONSE_PCI_CONTROL);
		read_pointer = ELBI_BF_GET(resp_pci_control,
					   ELBI_RESPONSE_PCI_CONTROL_READ_POINTER_MASK,
					   ELBI_RESPONSE_PCI_CONTROL_READ_POINTER_SHIFT);
		write_pointer = ELBI_BF_GET(resp_pci_control,
					    ELBI_RESPONSE_PCI_CONTROL_WRITE_POINTER_MASK,
					    ELBI_RESPONSE_PCI_CONTROL_WRITE_POINTER_SHIFT);

		nnp_pci->respq_free_slots = ELBI_RESPONSE_FIFO_DEPTH - (write_pointer - read_pointer);

		if (nnp_pci->respq_free_slots < size) {
			*read_update_count = nnp_pci->resp_fifo_read_update_count;
			NNP_SPIN_UNLOCK_IRQRESTORE(&nnp_pci->irq_lock, flags);
			NNP_SPIN_UNLOCK(&nnp_pci->respq_lock);
			return -EAGAIN;
		}
		NNP_SPIN_UNLOCK_IRQRESTORE(&nnp_pci->irq_lock, flags);
	}

	/* Write all but the last message without generating interrupt on host */
	for (i = 0; i < size-1; i++) {
		nnp_mmio_write(nnp_pci,
			       ELBI_RESPONSE_WRITE_WO_MSI_LOW,
			       lower_32_bits(msg[i]));
		nnp_mmio_write(nnp_pci,
			       ELBI_RESPONSE_WRITE_WO_MSI_HIGH,
			       upper_32_bits(msg[i]));
	}

	/* Write last message with generating interrupt on host */
	nnp_mmio_write(nnp_pci,
		       ELBI_RESPONSE_WRITE_W_MSI_LOW,
		       lower_32_bits(msg[i]));
	nnp_mmio_write(nnp_pci,
		       ELBI_RESPONSE_WRITE_W_MSI_HIGH,
		       upper_32_bits(msg[i]));

	nnp_pci->respq_free_slots -= size;

	NNP_SPIN_UNLOCK(&nnp_pci->respq_lock);

	return 0;
}

static int sph_respq_write_mesg(void *hw_handle, u64 *msg, u32 size)
{
	struct nnp_pci_device *nnp_pci = (struct nnp_pci_device *)hw_handle;
	int rc;
	u64 start = 0;
	u32 read_update_count = 0;
	bool update_sw_counter = NNP_SW_GROUP_IS_ENABLE(g_nnp_sw_counters,
							SPHCS_SW_COUNTERS_GROUP_IPC);

	rc = sph_respq_write_mesg_nowait(hw_handle, msg, size, &read_update_count);
	if (rc == -EAGAIN) {
		if (update_sw_counter)
			start = nnp_time_us();
	} else {
		goto end;
	}

	while (rc == -EAGAIN) {
		rc = wait_event_interruptible(nnp_pci->host_status_wait,
					      read_update_count != nnp_pci->resp_fifo_read_update_count);
		if (rc)
			break;
		rc = sph_respq_write_mesg_nowait(hw_handle, msg, size, &read_update_count);
	}

	if (update_sw_counter)
		NNP_SW_COUNTER_ADD(g_nnp_sw_counters, SPHCS_SW_COUNTERS_IPC_RESPONSES_WAIT_TIME, nnp_time_us() - start);
end:
	if (rc)
		sph_log_err(GENERAL_LOG, "Failed to write message size %d rc=%d!!\n", size, rc);

	return rc;
}

static void sphcs_nnp_reset_wr_dma_engine(void *hw_handle)
{
	struct nnp_pci_device *nnp_pci = (struct nnp_pci_device *)hw_handle;

	nnp_mmio_write(nnp_pci, DMA_WRITE_ENGINE_EN_OFF, 0);
	udelay(5);
	nnp_mmio_write(nnp_pci, DMA_WRITE_ENGINE_EN_OFF, 1);
}

static void sphcs_nnp_reset_rd_dma_engine(void *hw_handle)
{
	struct nnp_pci_device *nnp_pci = (struct nnp_pci_device *)hw_handle;

	nnp_mmio_write(nnp_pci, DMA_READ_ENGINE_EN_OFF, 0);
	udelay(5);
	nnp_mmio_write(nnp_pci, DMA_READ_ENGINE_EN_OFF, 1);
}

static int sphcs_sph_init_dma_engine(void *hw_handle)
{
	struct nnp_pci_device *nnp_pci = (struct nnp_pci_device *)hw_handle;
	u32 mask;
	int i;
	uint32_t reg;

	/* initalization read channels */
	for (i = 0; i < NUM_H2C_CHANNELS; i++) {
		nnp_mmio_write(nnp_pci,
			       DMA_READ_INT_CLEAR_OFF,
			       (DMA_SET_CHAN_BIT(i, DMA_RD_ABORT_INT_CLEAR_OFF) |
				DMA_SET_CHAN_BIT(i, DMA_RD_DONE_INT_CLEAR_OFF)));
		nnp_mmio_write(nnp_pci, DMA_CH_CONTROL1_OFF_RDCH(i), 0);
		nnp_mmio_write(nnp_pci, DMA_CH_CONTROL2_OFF_RDCH(i), 0);
		nnp_mmio_write(nnp_pci, DMA_TRANSFER_SIZE_OFF_RDCH(i), 0);
		nnp_mmio_write(nnp_pci, DMA_SAR_LOW_OFF_RDCH(i), 0);
		nnp_mmio_write(nnp_pci, DMA_SAR_HIGH_OFF_RDCH(i), 0);
		nnp_mmio_write(nnp_pci, DMA_DAR_LOW_OFF_RDCH(i), 0);
		nnp_mmio_write(nnp_pci, DMA_DAR_HIGH_OFF_RDCH(i), 0);
		nnp_mmio_write(nnp_pci, DMA_LLP_LOW_OFF_RDCH(i), 0);
		nnp_mmio_write(nnp_pci, DMA_LLP_HIGH_OFF_RDCH(i), 0);

		/* enable LL mode local interrupt on abort */
		reg = nnp_mmio_read(nnp_pci, DMA_READ_LL_ERR_EN_OFF);
		reg |= DMA_SET_CHAN_BIT(i, DMA_RD_CHAN_LLLAIE_OFF);
		nnp_mmio_write(nnp_pci, DMA_READ_LL_ERR_EN_OFF, reg);
	}

	/* enable interrupts for read channels */
	mask = ~(((1U<<NUM_H2C_CHANNELS)-1) << DMA_RD_DONE_INT_MASK_OFF |
		 ((1U<<NUM_H2C_CHANNELS)-1) << DMA_RD_ABORT_INT_MASK_OFF);
	nnp_mmio_write(nnp_pci,
		       DMA_READ_INT_MASK_OFF,
		       mask);

	/* initalization write channels */
	for (i = 0; i < NUM_C2H_CHANNELS; i++) {
		nnp_mmio_write(nnp_pci,
			       DMA_WRITE_INT_CLEAR_OFF,
			       (DMA_SET_CHAN_BIT(i, DMA_WR_ABORT_INT_CLEAR_OFF) |
				DMA_SET_CHAN_BIT(i, DMA_WR_DONE_INT_CLEAR_OFF)));
		nnp_mmio_write(nnp_pci, DMA_CH_CONTROL1_OFF_WRCH(i), 0);
		nnp_mmio_write(nnp_pci, DMA_CH_CONTROL2_OFF_WRCH(i), 0);
		nnp_mmio_write(nnp_pci, DMA_TRANSFER_SIZE_OFF_WRCH(i), 0);
		nnp_mmio_write(nnp_pci, DMA_SAR_LOW_OFF_WRCH(i), 0);
		nnp_mmio_write(nnp_pci, DMA_SAR_HIGH_OFF_WRCH(i), 0);
		nnp_mmio_write(nnp_pci, DMA_DAR_LOW_OFF_WRCH(i), 0);
		nnp_mmio_write(nnp_pci, DMA_DAR_HIGH_OFF_WRCH(i), 0);
		nnp_mmio_write(nnp_pci, DMA_LLP_LOW_OFF_WRCH(i), 0);
		nnp_mmio_write(nnp_pci, DMA_LLP_HIGH_OFF_WRCH(i), 0);

		/* enable LL mode local interrupt on abort */
		reg = nnp_mmio_read(nnp_pci, DMA_WRITE_LL_ERR_EN_OFF);
		reg |= DMA_SET_CHAN_BIT(i, DMA_RD_CHAN_LLLAIE_OFF);
		nnp_mmio_write(nnp_pci, DMA_WRITE_LL_ERR_EN_OFF, reg);
	}

	/* enable interrupts for write channels */
	mask = ~(((1U<<NUM_C2H_CHANNELS)-1) << DMA_WR_DONE_INT_MASK_OFF |
		 ((1U<<NUM_C2H_CHANNELS)-1) << DMA_WR_ABORT_INT_MASK_OFF);
	nnp_mmio_write(nnp_pci,
		       DMA_WRITE_INT_MASK_OFF,
		       mask);

	/* enable DMA engine */
	sphcs_nnp_reset_rd_dma_engine(hw_handle);
	sphcs_nnp_reset_wr_dma_engine(hw_handle);

	return 0;
}

static void *dma_set_lli_data_element(void *lliPtr,
				      dma_addr_t src,
				      dma_addr_t dst,
				      uint32_t size)
{
	struct lli_desc *lli = (struct lli_desc *)lliPtr;
	struct sph_dma_data_element *dataElement;
	u32 n, list_idx, list_off;
	bool is_last;

	if (lli->num_filled >= lli->num_elements || lli->num_lists == 0) {
		NNP_ASSERT(0);
		return lliPtr;
	}

	n = lli->num_elements / lli->num_lists;
	list_idx = lli->num_filled / n;
	if (list_idx >= lli->num_lists)
		list_idx = lli->num_lists - 1;
	list_off = lli->num_filled - (n * list_idx);
	dataElement = (struct sph_dma_data_element *)((uintptr_t)lli->vptr +
						      lli->offsets[list_idx] +
						      list_off * sizeof(struct sph_dma_data_element));
	dataElement->source_address_low = lower_32_bits(src);
	dataElement->source_address_high = upper_32_bits(src);
	dataElement->dest_address_low = lower_32_bits(dst);
	dataElement->dest_address_high = upper_32_bits(dst);
	dataElement->transfer_size = size;
	dataElement->control = DMA_CTRL_CB;

	lli->xfer_size[list_idx] += size;

	/* insert EOL element if last element in list */
	is_last = (list_idx == (lli->num_lists - 1) ?
			lli->num_filled == (lli->num_elements - 1) :
			list_off == (n - 1));
	if (is_last) {
		/* Set local interrupt enable bit */
		dataElement->control |= DMA_CTRL_LIE;

		/* Set end of list data element */
		dataElement++;
		dataElement->source_address_low = 0;
		dataElement->source_address_high = 0;
		dataElement->dest_address_low = 0;
		dataElement->dest_address_high = 0;
		dataElement->transfer_size = 0;
		dataElement->control = DMA_CTRL_LLP;
	}

	lli->num_filled = lli->num_filled + 1;

	return lliPtr;
}

static int sphcs_sph_dma_edit_lli_elem(struct lli_desc *lli, u32 elem_idx, dma_addr_t src, dma_addr_t dst)
{
	struct sph_dma_data_element *data_elem;

	if (!lli || lli->num_elements <= elem_idx || !lli->vptr)
		return -EINVAL;

	data_elem = lli->vptr + sizeof(struct sph_lli_header) + elem_idx * sizeof(struct sph_dma_data_element);
	data_elem->source_address_low = lower_32_bits(src);
	data_elem->source_address_high = upper_32_bits(src);
	data_elem->dest_address_low = lower_32_bits(dst);
	data_elem->dest_address_high = upper_32_bits(dst);

	return 0;
}

static void init_lli_desc(struct lli_desc *outLli, u32 nelements, bool single_list)
{
	u32 off;
	u32 i, n;

	outLli->num_elements = nelements;
	outLli->num_filled = 0;
	if (outLli->num_elements > 0 &&
	    outLli->num_elements >= dma_lli_min_split &&
	    dma_lli_split > 1 &&
	    !single_list)
		outLli->num_lists = (outLli->num_elements >= dma_lli_split ? dma_lli_split : outLli->num_elements);
	else
		outLli->num_lists = 1;

	off = sizeof(struct sph_lli_header);
	n = outLli->num_elements / outLli->num_lists;
	for (i = 0; i < outLli->num_lists; i++) {
		outLli->offsets[i] = off;
		off += ((n + 1) * sizeof(struct sph_dma_data_element));
	}

	outLli->size = (outLli->num_elements + outLli->num_lists) * sizeof(struct sph_dma_data_element) + sizeof(struct sph_lli_header);
}

int sphcs_sph_dma_init_lli(void            *hw_handle,
			   struct lli_desc *outLli,
			   struct sg_table *src,
			   struct sg_table *dst,
			   uint64_t         dst_offset,
			   bool single_list)
{
	u32 nelem;

	if (!outLli || !src || !dst)
		return -EINVAL;

	nelem = dma_calc_and_gen_lli(src, dst, NULL, dst_offset, 0, NULL, NULL);
	if (nelem == 0)
		return -EINVAL;

	init_lli_desc(outLli, nelem, single_list);

	return 0;
}

u64 sphcs_sph_dma_gen_lli(void            *hw_handle,
			  struct sg_table *src,
			  struct sg_table *dst,
			  struct lli_desc *outLli,
			  uint64_t         dst_offset)
{
	u32 num_of_elements;
	struct sph_lli_header *lli_header;
	uint64_t transfer_size = 0;

	if (hw_handle == NULL || src == NULL || dst == NULL || outLli == NULL)
		return 0;

	if (outLli->vptr == NULL || outLli->num_lists < 1)
		return 0;

	lli_header = (struct sph_lli_header *)outLli->vptr;

	lli_header->cut_element = NULL;

	/* Fill SGL */
	outLli->num_filled = 0;
	memset(&outLli->xfer_size, 0, sizeof(outLli->xfer_size));
	num_of_elements = dma_calc_and_gen_lli(src, dst, outLli, dst_offset, 0, dma_set_lli_data_element, &transfer_size);
	if (num_of_elements == 0 || outLli->num_filled != outLli->num_elements) {
		sph_log_err(EXECUTE_COMMAND_LOG, "ERROR: gen_lli cannot generate any data element.\n");
		return 0;
	}

	lli_header->size = transfer_size;

	return transfer_size;
}

static int sphcs_sph_dma_init_lli_vec(void *hw_handle, struct lli_desc *outLli, uint64_t dst_offset, genlli_get_next_cb cb, void *cb_ctx)
{
	struct sg_table *src;
	struct sg_table *dst;
	u64              max_size;
	u32              nelem = 0;

	if (hw_handle == NULL || cb == NULL || outLli == NULL)
		return -EINVAL;

	while ((*cb)(cb_ctx, &src, &dst, &max_size)) {
		nelem += dma_calc_and_gen_lli(src, dst, NULL, dst_offset, max_size, NULL, NULL);
		dst_offset = 0;
	}

	if (nelem == 0)
		return -EINVAL;

	init_lli_desc(outLli, nelem, false);

	return 0;
}

static u64 sphcs_sph_dma_gen_lli_vec(void *hw_handle, struct lli_desc *outLli, uint64_t dst_offset, genlli_get_next_cb cb, void *cb_ctx)
{
	struct sg_table *src;
	struct sg_table *dst;
	u64              max_size;
	u32 num_of_elements;
	struct sph_lli_header *lli_header;
	uint64_t transfer_size = 0;
	uint64_t total_transfer_size = 0;

	if (hw_handle == NULL || cb == NULL || outLli == NULL)
		return 0;

	if (outLli->vptr == NULL)
		return 0;

	lli_header = (struct sph_lli_header *)outLli->vptr;

	lli_header->cut_element = NULL;

	/* Fill SGL */
	outLli->num_filled = 0;
	while ((*cb)(cb_ctx, &src, &dst, &max_size)) {
		num_of_elements = dma_calc_and_gen_lli(src, dst, outLli, dst_offset, max_size, dma_set_lli_data_element, &transfer_size);
		if (num_of_elements == 0) {
			sph_log_err(EXECUTE_COMMAND_LOG, "ERROR: gen_lli cannot generate any data element.\n");
			return 0;
		}
		dst_offset = 0;
		total_transfer_size += transfer_size;
	}

	if (outLli->num_filled != outLli->num_elements) {
		sph_log_err(EXECUTE_COMMAND_LOG, "ERROR: gen_lli generated too few elements.\n");
		return 0;
	}

	lli_header->size = total_transfer_size;

	return total_transfer_size;
}


static void restore_lli(struct lli_desc *lli)
{
	struct sph_lli_header *lli_header = (struct sph_lli_header *)lli->vptr;

	if (lli_header->cut_element != NULL) {
		struct sph_dma_data_element *next_element = lli_header->cut_element + 1;

		lli_header->cut_element->transfer_size = lli_header->cut_element_transfer_size;
		lli_header->cut_element->control = lli_header->cut_element_ctrl_flag;

		//restore next element control
		memcpy(next_element, &lli_header->cut_element_next, sizeof(*next_element));

		lli->num_lists = lli_header->num_lists_keep;
		if (lli_header->cut_list_idx < SPH_LLI_MAX_LISTS)
			lli->xfer_size[lli_header->cut_list_idx] = lli_header->xfer_size_keep;
		lli_header->cut_element = NULL;
		lli_header->num_lists_keep = 0;
	}
}

int sphcs_sph_dma_edit_lli(void *hw_handle, struct lli_desc *outLli, uint64_t size)
{
	struct sph_lli_header *lli_header = (struct sph_lli_header *)outLli->vptr;
	struct sph_dma_data_element *data_element;
	uint64_t totalSize;
	uint32_t list_idx = 0;
	uint32_t i;

	if (size == 0) {
		NNP_ASSERT(!"edit_lli size should be greater than 0!");
		return -EINVAL;
	}

	if (lli_header->size == size)
		return 0;
	if (lli_header->cut_element)
		restore_lli(outLli);

	data_element = (struct sph_dma_data_element *)((uintptr_t)outLli->vptr + outLli->offsets[list_idx]);
	totalSize = data_element->transfer_size;
	while (totalSize < size) {
		data_element++;
		if (data_element->control & DMA_CTRL_LLP) {
			if (list_idx < outLli->num_lists - 1) {
				list_idx++;
				data_element = (struct sph_dma_data_element *)((uintptr_t)outLli->vptr + outLli->offsets[list_idx]);
			} else {
				sph_log_err(EXECUTE_COMMAND_LOG, "ERROR: edit size %llu is too big\n", size);
				return 0;
			}
		}
		totalSize += data_element->transfer_size;
	}

	if (totalSize == size &&
	    list_idx == outLli->num_lists - 1 &&
	    (data_element->control & DMA_CTRL_LIE) &&
	    ((data_element+1)->control & DMA_CTRL_LLP)) {
		/* no need to cut, size is of the full lli length */
		lli_header->size = size;

		return 0;
	}

	lli_header->cut_element = data_element;
	lli_header->cut_element_transfer_size = data_element->transfer_size;
	lli_header->cut_element_ctrl_flag  = data_element->control;

	if (totalSize > size)
		data_element->transfer_size -= totalSize - size;

	//Set local interrupt enable bit for last element
	data_element->control |= DMA_CTRL_LIE;

	//Set next element is link
	data_element++;
	memcpy(&lli_header->cut_element_next, data_element, sizeof(*data_element));
	memset(data_element, 0, sizeof(*data_element));
	data_element->control = DMA_CTRL_LLP;

	lli_header->size = size;
	lli_header->num_lists_keep = outLli->num_lists;
	lli_header->xfer_size_keep = outLli->xfer_size[list_idx];
	lli_header->cut_list_idx = list_idx;
	outLli->num_lists = list_idx + 1;
	outLli->xfer_size[list_idx] = size;
	for (i = 0; i < list_idx; i++)
		outLli->xfer_size[list_idx] -= outLli->xfer_size[i];

	return 0;
}

static inline void sphcs_sph_dma_set_ch_weights(struct nnp_pci_device *nnp_pci, int channel, u32 priority, u32 reg)
{
	uint32_t weight;
	uint32_t reg_val;
	unsigned long flags;

	NNP_SPIN_LOCK_IRQSAVE(&nnp_pci->dma_lock_irq, flags);
	weight = priority * SPHCS_DMA_PRIORITY_FACTOR;
	reg_val = nnp_mmio_read(nnp_pci, reg);
	reg_val &= ~(0x1F<<(channel*5));
	reg_val |= (weight<<(channel*5));
	nnp_mmio_write(nnp_pci, reg, reg_val);
	NNP_SPIN_UNLOCK_IRQRESTORE(&nnp_pci->dma_lock_irq, flags);
}

int sphcs_sph_dma_start_xfer_h2c(void      *hw_handle,
				 int        channel,
				 u32        priority,
				 dma_addr_t lli_addr)
{
	struct nnp_pci_device *nnp_pci = (struct nnp_pci_device *)hw_handle;

	if (!nnp_pci->bus_master_en)
		return -EACCES;

	/* program LLI mode */
	nnp_mmio_write(nnp_pci,
		       DMA_CH_CONTROL1_OFF_RDCH(channel),
		       (DMA_CTRL_LLE | DMA_CTRL_CCS | DMA_CTRL_TD));
	nnp_mmio_write(nnp_pci,
		       DMA_CH_CONTROL2_OFF_RDCH(channel),
		       0);
	nnp_mmio_write(nnp_pci,
		       DMA_TRANSFER_SIZE_OFF_RDCH(channel),
		       0);
	nnp_mmio_write(nnp_pci,
		       DMA_SAR_LOW_OFF_RDCH(channel),
		       0);
	nnp_mmio_write(nnp_pci,
		       DMA_SAR_HIGH_OFF_RDCH(channel),
		       0);
	nnp_mmio_write(nnp_pci,
		       DMA_DAR_LOW_OFF_RDCH(channel),
		       0);
	nnp_mmio_write(nnp_pci,
		       DMA_DAR_HIGH_OFF_RDCH(channel),
		       0);
	nnp_mmio_write(nnp_pci,
		       DMA_LLP_LOW_OFF_RDCH(channel),
		       lower_32_bits(lli_addr));
	nnp_mmio_write(nnp_pci,
		       DMA_LLP_HIGH_OFF_RDCH(channel),
		       upper_32_bits(lli_addr));

	sphcs_sph_dma_set_ch_weights(nnp_pci, channel, priority, DMA_READ_CHAN_ARB_WEIGHT_LOW_REG);

	/* get time stamp */
	if (NNP_SW_GROUP_IS_ENABLE(g_nnp_sw_counters, SPHCS_SW_COUNTERS_GROUP_DMA))
		nnp_pci->h2c_channels[channel].usTime = nnp_time_us();
	else
		nnp_pci->h2c_channels[channel].usTime = 0;

	/* start the channel */
	nnp_mmio_write(nnp_pci,
		       DMA_READ_DOORBELL_OFF,
		       channel);

	return 0;
}

int sphcs_sph_dma_start_xfer_c2h(void      *hw_handle,
				 int        channel,
				 u32        priority,
				 dma_addr_t lli_addr)
{
	struct nnp_pci_device *nnp_pci = (struct nnp_pci_device *)hw_handle;

	if (!nnp_pci->bus_master_en)
		return -EACCES;

	/* program LLI mode */
	nnp_mmio_write(nnp_pci,
		       DMA_CH_CONTROL1_OFF_WRCH(channel),
		       (DMA_CTRL_LLE | DMA_CTRL_CCS | DMA_CTRL_TD));
	nnp_mmio_write(nnp_pci,
		       DMA_CH_CONTROL2_OFF_WRCH(channel),
		       0);
	nnp_mmio_write(nnp_pci,
		       DMA_TRANSFER_SIZE_OFF_WRCH(channel),
		       0);
	nnp_mmio_write(nnp_pci,
		       DMA_SAR_LOW_OFF_WRCH(channel),
		       0);
	nnp_mmio_write(nnp_pci,
		       DMA_SAR_HIGH_OFF_WRCH(channel),
		       0);
	nnp_mmio_write(nnp_pci,
		       DMA_DAR_LOW_OFF_WRCH(channel),
		       0);
	nnp_mmio_write(nnp_pci,
		       DMA_DAR_HIGH_OFF_WRCH(channel),
		       0);
	nnp_mmio_write(nnp_pci,
		       DMA_LLP_LOW_OFF_WRCH(channel),
		       lower_32_bits(lli_addr));
	nnp_mmio_write(nnp_pci,
		       DMA_LLP_HIGH_OFF_WRCH(channel),
		       upper_32_bits(lli_addr));

	sphcs_sph_dma_set_ch_weights(nnp_pci, channel, priority, DMA_WRITE_CHAN_ARB_WEIGHT_LOW_REG);

	/* get time stamp */
	if (NNP_SW_GROUP_IS_ENABLE(g_nnp_sw_counters, SPHCS_SW_COUNTERS_GROUP_DMA))
		nnp_pci->c2h_channels[channel].usTime = nnp_time_us();
	else
		nnp_pci->c2h_channels[channel].usTime = 0;

	/* start the channel */
	nnp_mmio_write(nnp_pci,
		       DMA_WRITE_DOORBELL_OFF,
		       channel);

	return 0;
}

int sphcs_sph_dma_start_xfer_h2c_single(void      *hw_handle,
					int        channel,
					u32        priority,
					dma_addr_t src,
					dma_addr_t dst,
					u32        size)
{
	struct nnp_pci_device *nnp_pci = (struct nnp_pci_device *)hw_handle;

	if (!nnp_pci->bus_master_en)
		return -EACCES;

	/* program single mode */
	nnp_mmio_write(nnp_pci,
		       DMA_CH_CONTROL1_OFF_RDCH(channel),
		       (DMA_CTRL_LIE | DMA_CTRL_TD));
	nnp_mmio_write(nnp_pci,
		       DMA_CH_CONTROL2_OFF_RDCH(channel),
		       0);
	nnp_mmio_write(nnp_pci,
		       DMA_TRANSFER_SIZE_OFF_RDCH(channel),
		       size);
	nnp_mmio_write(nnp_pci,
		       DMA_SAR_LOW_OFF_RDCH(channel),
		       lower_32_bits(src));
	nnp_mmio_write(nnp_pci,
		       DMA_SAR_HIGH_OFF_RDCH(channel),
		       upper_32_bits(src));
	nnp_mmio_write(nnp_pci,
		       DMA_DAR_LOW_OFF_RDCH(channel),
		       lower_32_bits(dst));
	nnp_mmio_write(nnp_pci,
		       DMA_DAR_HIGH_OFF_RDCH(channel),
		       upper_32_bits(dst));
	nnp_mmio_write(nnp_pci,
		       DMA_LLP_LOW_OFF_RDCH(channel), 0);
	nnp_mmio_write(nnp_pci,
		       DMA_LLP_HIGH_OFF_RDCH(channel), 0);

	sphcs_sph_dma_set_ch_weights(nnp_pci, channel, priority, DMA_READ_CHAN_ARB_WEIGHT_LOW_REG);

	/* get time stamp */
	if (NNP_SW_GROUP_IS_ENABLE(g_nnp_sw_counters, SPHCS_SW_COUNTERS_GROUP_DMA))
		nnp_pci->h2c_channels[channel].usTime = nnp_time_us();
	else
		nnp_pci->h2c_channels[channel].usTime = 0;

	/* start the channel */
	nnp_mmio_write(nnp_pci,
		       DMA_READ_DOORBELL_OFF,
		       channel);

	return 0;
}

int sphcs_sph_dma_start_xfer_c2h_single(void      *hw_handle,
					int        channel,
					u32        priority,
					dma_addr_t src,
					dma_addr_t dst,
					u32        size)
{
	struct nnp_pci_device *nnp_pci = (struct nnp_pci_device *)hw_handle;

	if (!nnp_pci->bus_master_en)
		return -EACCES;

	/* program single mode */
	nnp_mmio_write(nnp_pci,
		       DMA_CH_CONTROL1_OFF_WRCH(channel),
		       (DMA_CTRL_LIE | DMA_CTRL_TD));
	nnp_mmio_write(nnp_pci,
		       DMA_CH_CONTROL2_OFF_WRCH(channel),
		       0);
	nnp_mmio_write(nnp_pci,
		       DMA_TRANSFER_SIZE_OFF_WRCH(channel),
		       size);
	nnp_mmio_write(nnp_pci,
		       DMA_SAR_LOW_OFF_WRCH(channel),
		       lower_32_bits(src));
	nnp_mmio_write(nnp_pci,
		       DMA_SAR_HIGH_OFF_WRCH(channel),
		       upper_32_bits(src));
	nnp_mmio_write(nnp_pci,
		       DMA_DAR_LOW_OFF_WRCH(channel),
		       lower_32_bits(dst));
	nnp_mmio_write(nnp_pci,
		       DMA_DAR_HIGH_OFF_WRCH(channel),
		       upper_32_bits(dst));
	nnp_mmio_write(nnp_pci,
		       DMA_LLP_LOW_OFF_WRCH(channel), 0);
	nnp_mmio_write(nnp_pci,
		       DMA_LLP_HIGH_OFF_WRCH(channel), 0);

	sphcs_sph_dma_set_ch_weights(nnp_pci, channel, priority, DMA_WRITE_CHAN_ARB_WEIGHT_LOW_REG);

	/* get time stamp */
	if (NNP_SW_GROUP_IS_ENABLE(g_nnp_sw_counters, SPHCS_SW_COUNTERS_GROUP_DMA))
		nnp_pci->c2h_channels[channel].usTime = nnp_time_us();
	else
		nnp_pci->c2h_channels[channel].usTime = 0;

	/* start the channel */
	nnp_mmio_write(nnp_pci,
		       DMA_WRITE_DOORBELL_OFF,
		       channel);

	return 0;
}

static void sphcs_sph_dma_abort_all_channles(struct nnp_pci_device *nnp_pci)
{
#if 0
	int i;

	for (i = 0; i < NUM_H2C_CHANNELS; i++)
		nnp_mmio_write(nnp_pci,
			       DMA_READ_DOORBELL_OFF,
			       BIT(DMA_DOORBELL_STOP_OFF) | i);

	for (i = 0; i < NUM_C2H_CHANNELS; i++)
		nnp_mmio_write(nnp_pci,
			       DMA_WRITE_DOORBELL_OFF,
			       BIT(DMA_DOORBELL_STOP_OFF) | i);

	sphcs_nnp_reset_dma_engine(nnp_pci);
#endif
}

int sphcs_sph_dma_xfer_c2h_single(void      *hw_handle,
				  dma_addr_t src,
				  dma_addr_t dst,
				  u32        size,
				  u32        timeout_ms,
				  int       *dma_status,
				  u32       *usTime)
{
	u64 start_dma_time = 0, end_dma_time;
	struct nnp_pci_device *nnp_pci = (struct nnp_pci_device *)hw_handle;
	u32 res_transfer_size, status, channel_status;
	unsigned long time;
	int channel = 0;

	if (!nnp_pci->bus_master_en)
		return -EACCES;

	/* Stop all DMA channels */
	sphcs_sph_dma_abort_all_channles(nnp_pci);

	/* program DMA without requesting local interrupt */
	nnp_mmio_write(nnp_pci,
		       DMA_CH_CONTROL1_OFF_WRCH(channel),
		       DMA_CTRL_TD);
	nnp_mmio_write(nnp_pci,
		       DMA_CH_CONTROL2_OFF_WRCH(channel),
		       0);
	nnp_mmio_write(nnp_pci,
		       DMA_TRANSFER_SIZE_OFF_WRCH(channel),
		       size);
	nnp_mmio_write(nnp_pci,
		       DMA_SAR_LOW_OFF_WRCH(channel),
		       lower_32_bits(src));
	nnp_mmio_write(nnp_pci,
		       DMA_SAR_HIGH_OFF_WRCH(channel),
		       upper_32_bits(src));
	nnp_mmio_write(nnp_pci,
		       DMA_DAR_LOW_OFF_WRCH(channel),
		       lower_32_bits(dst));
	nnp_mmio_write(nnp_pci,
		       DMA_DAR_HIGH_OFF_WRCH(channel),
		       upper_32_bits(dst));
	nnp_mmio_write(nnp_pci,
		       DMA_LLP_LOW_OFF_WRCH(channel), 0);
	nnp_mmio_write(nnp_pci,
		       DMA_LLP_HIGH_OFF_WRCH(channel), 0);

	if (NNP_SW_GROUP_IS_ENABLE(g_nnp_sw_counters, SPHCS_SW_COUNTERS_GROUP_DMA))
		start_dma_time = nnp_time_us();

	/* start the DMA */
	nnp_mmio_write(nnp_pci,
		       DMA_WRITE_DOORBELL_OFF,
		       channel);

	/* wait for the DMA completion */
	time = jiffies + msecs_to_jiffies(timeout_ms);
	do {
		status = nnp_mmio_read(nnp_pci,
				       DMA_CH_CONTROL1_OFF_WRCH(channel));
		channel_status = (status & DMA_CTRL_CHANNEL_STATUS_MASK) >> DMA_CTRL_CHANNEL_STATUS_OFF;
	} while ((channel_status == DMA_CTRL_CHANNEL_STATUS_RUNNING) &&
		 time_before(jiffies, time));

	/* Set error status */
	if (channel_status == DMA_CTRL_CHANNEL_STATUS_STOPPED) {
		/* Read the remaining transfer size*/
		res_transfer_size = nnp_mmio_read(nnp_pci,
						  DMA_TRANSFER_SIZE_OFF_WRCH(channel));

		if (res_transfer_size == 0)
			*dma_status = SPHCS_DMA_STATUS_DONE;
		else
			*dma_status = SPHCS_DMA_STATUS_FAILED;
	} else
		*dma_status = SPHCS_DMA_STATUS_FAILED;

	if (start_dma_time != 0) {
		end_dma_time = nnp_time_us();
		*usTime = (u32)(end_dma_time - start_dma_time);
	}

	return 0;
}

static u32 sph_get_host_doorbell_value(void *hw_handle)
{
	struct nnp_pci_device *nnp_pci = (struct nnp_pci_device *)hw_handle;
	u32 doorbell_val;

	doorbell_val = nnp_mmio_read(nnp_pci, ELBI_PCI_HOST_DOORBELL_VALUE);
	return doorbell_val;
}

static int sph_set_card_doorbell_value(void *hw_handle, u32 value)
{
	struct nnp_pci_device *nnp_pci = (struct nnp_pci_device *)hw_handle;
	unsigned long flags;

	NNP_SPIN_LOCK_IRQSAVE(&nnp_pci->irq_lock, flags);
	value &= ~(NNP_CARD_KEEP_ALIVE_MASK);
	value |= (nnp_pci->host_doorbell_val & NNP_HOST_KEEP_ALIVE_MASK);
	nnp_pci->card_doorbell_val = value;
	NNP_SPIN_UNLOCK_IRQRESTORE(&nnp_pci->irq_lock, flags);

	nnp_mmio_write(nnp_pci, ELBI_HOST_PCI_DOORBELL_VALUE, value);

	return 0;
}

static void sph_get_inbound_mem(void *hw_handle, dma_addr_t *base_addr, size_t *size)
{
	struct nnp_pci_device *nnp_pci = (struct nnp_pci_device *)hw_handle;
	u32 base_addr_lo, base_addr_hi;

	base_addr_lo = nnp_mmio_read(nnp_pci, IATU_LWR_TARGET_ADDR_INBOUND_OFF(0));
	base_addr_hi = nnp_mmio_read(nnp_pci, IATU_UPPER_TARGET_ADDR_INBOUND_OFF(0));

	*base_addr = ((u64)base_addr_hi << 32 | base_addr_lo);
	*size = MAPPED_MEMORY_SIZE;
}


#ifdef ULT
static int debug_cmdq_show(struct seq_file *m, void *v)
{
	struct nnp_pci_device *nnp_pci = (struct nnp_pci_device *)m->private;
	u32 command_iosf_control;
	u32 read_pointer;
	u32 write_pointer;
	u32 avail_slots;
	u32 low;
	u32 high;
	int i;

	if (unlikely(nnp_pci == NULL))
		return -EINVAL;


	command_iosf_control = nnp_mmio_read(nnp_pci,
					     ELBI_COMMAND_IOSF_CONTROL);
	read_pointer = ELBI_BF_GET(command_iosf_control,
				   ELBI_COMMAND_IOSF_CONTROL_READ_POINTER_MASK,
				   ELBI_COMMAND_IOSF_CONTROL_READ_POINTER_SHIFT);
	write_pointer = ELBI_BF_GET(command_iosf_control,
				    ELBI_COMMAND_IOSF_CONTROL_WRITE_POINTER_MASK,
				    ELBI_COMMAND_IOSF_CONTROL_WRITE_POINTER_SHIFT);
	/* Commands to read */
	avail_slots = write_pointer - read_pointer;

	seq_printf(m, "command_iosf_control = 0x%08x\n", command_iosf_control);
	seq_printf(m, "read_pointer=%u write_pointer=%u avail_slots=%u\n",
		   read_pointer,
		   write_pointer,
		   avail_slots);

	for (i = 0; i < ELBI_COMMAND_FIFO_DEPTH; i++) {
		low = nnp_mmio_read(nnp_pci,
				    ELBI_COMMAND_FIFO_LOW(i));
		high = nnp_mmio_read(nnp_pci,
				     ELBI_COMMAND_FIFO_HIGH(i));

		seq_printf(m, "%s %u: 0x%08x 0x%08x\n",
			   (i == read_pointer ? "r->" :
			    i == write_pointer ? "w->" :
			    "   "),
			   i, high, low);
	}

	return 0;
}

static int debug_cmdq_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, debug_cmdq_show, inode->i_private);
}

static const struct file_operations debug_cmdq_fops = {
	.open		= debug_cmdq_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

DEFINE_INT_STAT_DEBUGFS(int_stats);

void nnp_init_debugfs(struct nnp_pci_device *nnp_pci)
{
	struct dentry *f;

	if (s_debugfs_dir)
		return;

	s_debugfs_dir = debugfs_create_dir("pep", NULL);
	if (IS_ERR_OR_NULL(s_debugfs_dir)) {
		sph_log_err(START_UP_LOG, "Failed to initialize pep debugfs\n");
		s_debugfs_dir = NULL;
	}

	f = debugfs_create_file("cmdq",
				0444,
				s_debugfs_dir,
				nnp_pci,
				&debug_cmdq_fops);
	if (IS_ERR_OR_NULL(f))
		goto err;

	f = INT_STAT_DEBUGFS_CREATE(int_stats, s_debugfs_dir);
	if (IS_ERR_OR_NULL(f))
		goto err;

	return;

err:
	debugfs_remove_recursive(s_debugfs_dir);
	s_debugfs_dir = NULL;
}
#endif

static struct sphcs_pcie_hw_ops s_pcie_nnp_ops = {
	.write_mesg = sph_respq_write_mesg,
	.get_host_doorbell_value = sph_get_host_doorbell_value,
	.set_card_doorbell_value = sph_set_card_doorbell_value,
	.get_inbound_mem = sph_get_inbound_mem,


	.dma.reset_rd_dma_engine = sphcs_nnp_reset_rd_dma_engine,
	.dma.reset_wr_dma_engine = sphcs_nnp_reset_wr_dma_engine,
	.dma.init_dma_engine = sphcs_sph_init_dma_engine,
	.dma.init_lli = sphcs_sph_dma_init_lli,
	.dma.gen_lli = sphcs_sph_dma_gen_lli,
	.dma.edit_lli = sphcs_sph_dma_edit_lli,
	.dma.init_lli_vec = sphcs_sph_dma_init_lli_vec,
	.dma.gen_lli_vec = sphcs_sph_dma_gen_lli_vec,
	.dma.edit_lli_elem = sphcs_sph_dma_edit_lli_elem,
	.dma.start_xfer_h2c = sphcs_sph_dma_start_xfer_h2c,
	.dma.start_xfer_c2h = sphcs_sph_dma_start_xfer_c2h,
	.dma.start_xfer_h2c_single = sphcs_sph_dma_start_xfer_h2c_single,
	.dma.start_xfer_c2h_single = sphcs_sph_dma_start_xfer_c2h_single,
	.dma.xfer_c2h_single = sphcs_sph_dma_xfer_c2h_single,
};

static int nnp_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	struct nnp_pci_device *nnp_pci = NULL;
	u32 doorbell_val, status;
	int rc = -ENODEV;
	u32 dma_rs, dma_ws;

	if (PCI_FUNC(pdev->devfn) != NNP_PCI_DEVFN) {
		sph_log_err(START_UP_LOG, "unsupported pci.devfn=%u (driver only supports pci.devfn=%u)\n", PCI_FUNC(pdev->devfn), NNP_PCI_DEVFN);
		return -ENODEV;
	}

	nnp_pci = kzalloc(sizeof(*nnp_pci), GFP_KERNEL);
	if (!nnp_pci) {
		rc = -ENOMEM;
		sph_log_err(START_UP_LOG, "nnp_pci kmalloc failed rc %d\n", rc);
		goto alloc_fail;
	}

	nnp_pci->pdev = pdev;
	nnp_pci->dev = &pdev->dev;
	pci_set_drvdata(pdev, nnp_pci);

	/* enable device */
	rc = pci_enable_device(pdev);
	if (rc) {
		sph_log_err(START_UP_LOG, "failed to enable pci device. rc=%d\n", rc);
		goto free_nnp_pci;
	}

	/* enable bus master capability on device */
	pci_set_master(pdev);

	rc = pci_request_regions(pdev, nnp_driver_name);
	if (rc) {
		sph_log_err(START_UP_LOG, "failed to get pci regions.\n");
		goto disable_device;
	}

	nnp_pci->mmio.pa = pci_resource_start(pdev, NNP_PCI_MMIO_BAR);
	nnp_pci->mmio.len = pci_resource_len(pdev, NNP_PCI_MMIO_BAR);
	nnp_pci->mmio.va = pci_ioremap_bar(pdev, NNP_PCI_MMIO_BAR);
	if (!nnp_pci->mmio.va) {
		sph_log_err(START_UP_LOG, "Cannot remap MMIO BAR\n");
		rc = -EIO;
		goto release_regions;
	}

	sph_log_debug(START_UP_LOG, "nnp_pcie mmio_start is 0x%llx\n", nnp_pci->mmio.pa);
	sph_log_debug(START_UP_LOG, "nnp_pcie mmio_len   is 0x%zx\n", nnp_pci->mmio.len);

	rc = pci_set_dma_mask(pdev, DMA_BIT_MASK(64));
	if (rc) {
		sph_log_err(START_UP_LOG, "Cannot set DMA mask\n");
		goto unmap_mmio;
	}

	/* clear interrupts mask and status */
	nnp_mmio_write(nnp_pci,
		       ELBI_IOSF_MSI_MASK,
		       UINT_MAX);

	read_and_clear_dma_status(nnp_pci,
				  &dma_rs,
				  &dma_ws);

	nnp_mmio_write(nnp_pci,
		       ELBI_IOSF_STATUS,
		       (s_host_status_int_mask | s_host_status_threaded_mask));


	atomic64_set(&nnp_pci->dma_status, 0);
	atomic_set(&nnp_pci->new_command, 0);
	atomic_set(&nnp_pci->doorbell_changed, 0);

	rc = nnp_setup_interrupts(nnp_pci, pdev);
	if (rc) {
		sph_log_err(START_UP_LOG, "nnp_setup_interrupts failed %d\n", rc);
		goto unmap_mmio;
	}

	init_waitqueue_head(&nnp_pci->host_status_wait);
	spin_lock_init(&nnp_pci->respq_lock);
	spin_lock_init(&nnp_pci->irq_lock);
	spin_lock_init(&nnp_pci->dma_lock_irq);

	/* done setting up the device, create the upper level sphcs object */
	rc = s_callbacks->create_sphcs(nnp_pci,
				       nnp_pci->dev,
				       &s_pcie_nnp_ops,
				       &nnp_pci->sphcs,
				       &nnp_pci->dmaSched);
	if (rc) {
		sph_log_err(START_UP_LOG, "Create sphcs failed rc=%d", rc);
		goto free_interrupts;
	}

	/* create sysfs attributes */
	rc = device_create_file(nnp_pci->dev, &dev_attr_flr_mode);
	if (rc) {
		sph_log_err(START_UP_LOG, "Failed to create attr rc=%d", rc);
		goto free_interrupts;
	}

	rc = device_create_file(nnp_pci->dev, &dev_attr_link_width);
	if (rc) {
		sph_log_err(START_UP_LOG, "Failed to create attr rc=%d", rc);
		device_remove_file(nnp_pci->dev, &dev_attr_flr_mode);
		goto free_interrupts;
	}

	/* update bus master state - enable DMA if bus master is set */
	set_bus_master_state(nnp_pci);

	/* update default flr_mode in card status reg */
	nnp_mmio_write(nnp_pci,
		       ELBI_CPU_STATUS_2,
		       s_flr_mode);

	/* Update sphcs with current host doorbell value */
	doorbell_val = nnp_mmio_read(nnp_pci, ELBI_PCI_HOST_DOORBELL_VALUE);
	nnp_pci->host_doorbell_val = doorbell_val;
	s_callbacks->host_doorbell_value_changed(nnp_pci->sphcs, doorbell_val);
	status = nnp_mmio_read(nnp_pci, ELBI_IOSF_STATUS);
	if (status & ELBI_IOSF_STATUS_DOORBELL_MASK)
		nnp_mmio_write(nnp_pci, ELBI_IOSF_STATUS, ELBI_IOSF_STATUS_DOORBELL_MASK);

	/* Enable desired interrupts */
	nnp_mmio_write(nnp_pci,
		       ELBI_IOSF_MSI_MASK,
		       ~(s_host_status_int_mask | s_host_status_threaded_mask));

	/* check available message in command hwQ */
	nnp_process_commands(nnp_pci);

#ifdef ULT
	nnp_init_debugfs(nnp_pci);
#endif

	sph_log_debug(START_UP_LOG, "nnp_pcie probe done.\n");

	return 0;

free_interrupts:
	nnp_free_interrupts(nnp_pci, pdev);
unmap_mmio:
	iounmap(nnp_pci->mmio.va);
release_regions:
	pci_release_regions(pdev);
disable_device:
	pci_disable_device(pdev);
free_nnp_pci:
	kfree(nnp_pci);
alloc_fail:
	sph_log_err(START_UP_LOG, "Probe failed rc %d\n", rc);
	return rc;
}

static void nnp_remove(struct pci_dev *pdev)
{
	struct nnp_pci_device *nnp_pci = NULL;
	int rc;

	nnp_pci = pci_get_drvdata(pdev);
	if (!nnp_pci)
		return;

	device_remove_file(nnp_pci->dev, &dev_attr_flr_mode);
	device_remove_file(nnp_pci->dev, &dev_attr_link_width);

#ifdef ULT
	debugfs_remove_recursive(s_debugfs_dir);
	s_debugfs_dir = NULL;
#endif

	rc = s_callbacks->destroy_sphcs(nnp_pci->sphcs);
	if (rc)
		sph_log_err(GO_DOWN_LOG, "FAILED to destroy sphcs during device remove !!!!\n");

	/*
	 * mask all interrupts
	 * Especcially it is important to set LINE_FLR bit in the mask
	 * to flag the p-code that the driver unbound and any warm-reset
	 * request should be handled directly by p-code.
	 */
	nnp_mmio_write(nnp_pci,
		       ELBI_IOSF_MSI_MASK,
		       UINT_MAX);

	nnp_free_interrupts(nnp_pci, pdev);
	iounmap(nnp_pci->mmio.va);
	pci_release_regions(pdev);
	pci_disable_device(pdev);
	kfree(nnp_pci);
}

static const struct pci_device_id nnp_pci_tbl[] = {
	{PCI_DEVICE(NNP_PCI_VENDOR_ID, NNP_PCI_DEVICE_ID)},
	/* required last entry */
	{ 0, }
};

static struct pci_driver nnp_driver = {
	.name = nnp_driver_name,
	.id_table = nnp_pci_tbl,
	.probe = nnp_probe,
	.remove = nnp_remove
};

int sphcs_hw_init(struct sphcs_pcie_callbacks *callbacks)
{
	int ret;

	sph_log_debug(START_UP_LOG, "nnp_pci hw_init vendor=0x%x device_id=0x%x\n",
		      NNP_PCI_VENDOR_ID, NNP_PCI_DEVICE_ID);

	s_callbacks = callbacks;

	ret = pci_register_driver(&nnp_driver);
	if (ret) {
		sph_log_err(START_UP_LOG, "pci_register_driver failed ret %d\n", ret);
		goto error;
	}


	return ret;

error:
	sph_log_err(START_UP_LOG, "init failed ret %d\n", ret);
	return ret;
}

int sphcs_hw_cleanup(void)
{
	sph_log_debug(GO_DOWN_LOG, "Cleanup");
	pci_unregister_driver(&nnp_driver);
	return 0;
}
