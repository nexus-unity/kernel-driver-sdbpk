#ifndef DESCRIPTOR_H_
#define DESCRIPTOR_H_

#include "sdbp.h"
#include "communication.h"

struct Version {
	u8 stability;
	u16 major;
	u16 minor;
	u16 patch;
};

struct notification {
	atomic_t length;
	u8 data[PAGE_SIZE];	// sysfs max. size is PAGE_SIZE
	wait_queue_head_t wait_for_notification;
	atomic_t lock;
};

struct Descriptor {
	u8 vendor_product_id[256];
	u8 vendor_product_id_len;
	u8 serial_code[16];
	struct Version fw_version;
	struct Version hw_version;
	u32 max_sclk_speed;
	u32 max_frame_size;
	struct Version protocol_version;
	u8 vendor_name[256];
	u8 vendor_name_len;
	u8 product_name[256];
	u8 product_name_len;
	u8 bootloader_state;
	u32 max_power_3v3;
	u32 max_power_5v0;
	u32 max_power_12v;
	u32 rid;
	atomic_t is_valid;
};

struct Slot {
	u8 number;
	u8 valid;
	u32 speed_sclk;
	u32 frame_size;
	u8 crc_size;
	u8 spi_bus;
	u8 spi_chip_select;
	u8 cs_pin_alt;
	u8 interrupt_pin;
	int irq_number;
	struct spi_device *spi_device;
	atomic_t interrupt_arrived;
	atomic_t notification_arrived;
	wait_queue_head_t queue;
	struct task_struct *thread;
	struct device *sdbp_device;
	struct Descriptor descriptor;
	struct Descriptor descriptor_old;
	u8 *tx_buffer;
	u8 *rx_buffer;
	u16 rx_len;
	u16 tx_len;
	wait_queue_head_t wait_queue_for_read;
	wait_queue_head_t wait_queue_for_write;
	atomic_t access_count;
	atomic_t write_count;
	atomic_t stop;
	struct notification notification;
	struct completion dev_obj_is_free;
	struct ErrorStatistics session_stats;
};

static const u8 DESCRIPTOR_GET_VENDOR_PRODUCT_ID[] = { 0x01, 0x00, 0x07, 0x00, 0x01, 0x02, 0x02 };
static const u8 DESCRIPTOR_GET_SERIAL_CODE[] = { 0x01, 0x00, 0x07, 0x00, 0x01, 0x02, 0x03 };
static const u8 DESCRIPTOR_GET_FW_VERSION[] = { 0x01, 0x00, 0x07, 0x00, 0x01, 0x02, 0x04 };
static const u8 DESCRIPTOR_GET_HW_VERSION[] = { 0x01, 0x00, 0x07, 0x00, 0x01, 0x02, 0x05 };
static const u8 DESCRIPTOR_GET_MAX_SCLK_SPEED[] = { 0x01, 0x00, 0x07, 0x00, 0x01, 0x02, 0x06 };
static const u8 DESCRIPTOR_GET_MAX_FRAME_SIZE[] = { 0x01, 0x00, 0x07, 0x00, 0x01, 0x02, 0x07 };
static const u8 DESCRIPTOR_GET_PROTOCOL_VERSION[] = { 0x01, 0x00, 0x07, 0x00, 0x01, 0x02, 0x08 };
static const u8 DESCRIPTOR_GET_VENDOR_NAME[] = { 0x01, 0x00, 0x07, 0x00, 0x01, 0x02, 0x09 };
static const u8 DESCRIPTOR_GET_PRODUCT_NAME[] = { 0x01, 0x00, 0x07, 0x00, 0x01, 0x02, 0x0A };
static const u8 DESCRIPTOR_GET_BOOTLOADER_STATE[] = { 0x01, 0x00, 0x07, 0x00, 0x01, 0x02, 0x0B };
static const u8 DESCRIPTOR_GET_MAX_POWER_3V3[] = { 0x01, 0x00, 0x07, 0x00, 0x01, 0x02, 0x0C };
static const u8 DESCRIPTOR_GET_MAX_POWER_5V0[] = { 0x01, 0x00, 0x07, 0x00, 0x01, 0x02, 0x0D };
static const u8 DESCRIPTOR_GET_MAX_POWER_12V0[] = { 0x01, 0x00, 0x07, 0x00, 0x01, 0x02, 0x0E };

static const u8 DESCRIPTOR_ERROR_CODE = 0x01;

static const u8 CONTROL_SET_FRAME_SIZE_DEFAULT[] = { 0x01, 0x00, 0x09, 0x00, 0x01, 0x03, 0x07, 0x00, 64 };
static const u8 CONTROL_SET_MODE_SUSPEND[] = { 0x01, 0x00, 0x07, 0x00, 0x01, 0x03, 0x02 };
static const u8 CONTROL_UPDATE_DESCRIPTOR[] = { 0x01, 0x00, 0x07, 0x00, 0x01, 0x03, 0x09 };

static const u8 DUMMY_DUMMY[] = { 0x04, 0x00, 0x07, 0x00, 0x01, 0x04, 0x01 };
static const u8 NOTIFICATION[] = { 0x01, 0x00, 0x07, 0x00, 0x01, 0x06, 0x02 };

int get_descriptor(struct Slot *slot, struct Descriptor *descriptor, u8 force, u32 old_rid);

#endif
