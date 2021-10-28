#include <linux/module.h>
#include <linux/slab.h>
#include <linux/random.h>
#include "descriptor.h"
#include "sdbp.h"
#include "communication.h"
#include "debug.h"

void print_descriptor(struct Slot *slot, struct Descriptor *descriptor_sdbp)
{
	u8 *tmp_buffer;
	int char_cnt;

	tmp_buffer = kcalloc(MAXIMUM_FRAME_SIZE, sizeof(u8), GFP_KERNEL);

	char_cnt = snprintf(tmp_buffer, descriptor_sdbp->vendor_product_id_len, (u8 *) descriptor_sdbp->vendor_product_id);
	PRINT_SLOT_NORM("Vendor product id: %s", slot->number, tmp_buffer);

	char_cnt = snprintf(tmp_buffer, descriptor_sdbp->vendor_name_len, (u8 *) descriptor_sdbp->vendor_name);
	PRINT_SLOT_NORM("Vendor name: %s", slot->number, tmp_buffer);

	char_cnt = snprintf(tmp_buffer, descriptor_sdbp->product_name_len, (u8 *) descriptor_sdbp->product_name);
	PRINT_SLOT_NORM("Product name: %s", slot->number, tmp_buffer);

	PRINT_SLOT_NORM
	    ("Protocol/Hardware/Firmware version: %d.%d.%d/%d.%d.%d/%c.%d.%d.%d",
	     slot->number, descriptor_sdbp->protocol_version.major,
	     descriptor_sdbp->protocol_version.minor,
	     descriptor_sdbp->protocol_version.patch,
	     descriptor_sdbp->hw_version.major,
	     descriptor_sdbp->hw_version.minor,
	     descriptor_sdbp->hw_version.patch,
	     descriptor_sdbp->fw_version.stability, descriptor_sdbp->fw_version.major, descriptor_sdbp->fw_version.minor, descriptor_sdbp->fw_version.patch);

	PRINT_SLOT_NORM("Maximum SCLK speed: %d kHz", slot->number, descriptor_sdbp->max_sclk_speed);

	PRINT_SLOT_NORM("Maximum frame size: %d bytes", slot->number, descriptor_sdbp->max_frame_size);

	PRINT_SLOT_NORM("Maximum power 3V3/5V0/12V: %d/%d/%d mW", slot->number,
			descriptor_sdbp->max_power_3v3, descriptor_sdbp->max_power_5v0, descriptor_sdbp->max_power_12v);

	char_cnt = snprintf(tmp_buffer, 16 * 2 + 7 + 1,
			    "%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X",
			    descriptor_sdbp->serial_code[0],
			    descriptor_sdbp->serial_code[1],
			    descriptor_sdbp->serial_code[2],
			    descriptor_sdbp->serial_code[3],
			    descriptor_sdbp->serial_code[4],
			    descriptor_sdbp->serial_code[5],
			    descriptor_sdbp->serial_code[6],
			    descriptor_sdbp->serial_code[7],
			    descriptor_sdbp->serial_code[8],
			    descriptor_sdbp->serial_code[9],
			    descriptor_sdbp->serial_code[10],
			    descriptor_sdbp->serial_code[11],
			    descriptor_sdbp->serial_code[12], descriptor_sdbp->serial_code[13], descriptor_sdbp->serial_code[14],
			    descriptor_sdbp->serial_code[15]
	    );

	PRINT_SLOT_NORM("Serial code: %s", slot->number, tmp_buffer);

	if (descriptor_sdbp->bootloader_state == 0)
		PRINT_SLOT_NORM("Bootloader state: No bootloader support (%d)", slot->number, descriptor_sdbp->bootloader_state);
	else if (descriptor_sdbp->bootloader_state == 1)
		PRINT_SLOT_NORM("Bootloader state: Bootloader support (%d)", slot->number, descriptor_sdbp->bootloader_state);
	else if (descriptor_sdbp->bootloader_state == 2)
		PRINT_SLOT_NORM("Bootloader state: Device in bootloader mode (%d)", slot->number, descriptor_sdbp->bootloader_state);
	else
		PRINT_SLOT_NORM("Bootloader state: (%d)", slot->number, descriptor_sdbp->bootloader_state);

	kfree(tmp_buffer);
}

int get_descriptor(struct Slot *slot, struct Descriptor *descriptor_sdbp, u8 force, u32 old_rid)
{
	u8 *rx_buffer;
	u8 i;
	u16 length;
	u8 chaining;
	u8 pos_cnt;
	u32 rand;
	bool chaining_active;
	slot->descriptor_old = slot->descriptor;
	atomic_inc(&descriptor_sdbp->is_valid);

	rx_buffer = kcalloc(MAXIMUM_FRAME_SIZE, sizeof(u8), GFP_KERNEL);	// Allocate memory
	if (!force)
		atomic_set(&slot->write_count, 0);

	if (exchange_sdbp(slot, (u8 *) DESCRIPTOR_GET_PROTOCOL_VERSION, rx_buffer, LOG_LVL_SILENT) != 0)
		goto cleanup;

	if (rx_buffer[6] == DESCRIPTOR_ERROR_CODE) {
		PRINT_SLOT_DBG("Descriptor error code returned!", slot->number);
		goto cleanup;
	}

	length = (rx_buffer[1] << 8) | rx_buffer[2];
	if (length != 13) {
		PRINT_SLOT_ERR("Length of Descriptor field invalid!\n", slot->number);
		goto cleanup;
	}

	descriptor_sdbp->protocol_version.major = rx_buffer[7] << 8 | rx_buffer[8];
	descriptor_sdbp->protocol_version.minor = rx_buffer[9] << 8 | rx_buffer[10];
	descriptor_sdbp->protocol_version.patch = rx_buffer[11] << 8 | rx_buffer[12];

	if (descriptor_sdbp->protocol_version.major >= 2) {
		PRINT_SLOT_ERR
		    ("SDBP version of device not supported! (V%u.%u.%u)\n",
		     slot->number, descriptor_sdbp->protocol_version.major, descriptor_sdbp->protocol_version.minor, descriptor_sdbp->protocol_version.patch);
		goto cleanup;
	}

	chaining = 1;
	pos_cnt = 0;
	chaining_active = false;
	descriptor_sdbp->vendor_product_id_len = 0;
	while (chaining != 0) {
		if (exchange_sdbp(slot, (u8 *) DESCRIPTOR_GET_VENDOR_PRODUCT_ID, rx_buffer, LOG_LVL_SILENT) != 0)
			goto cleanup;

		if (rx_buffer[6] == DESCRIPTOR_ERROR_CODE) {
			PRINT_SLOT_DBG("Descriptor error code returned!", slot->number);
			goto cleanup;
		}

		chaining = rx_buffer[7];
		length = rx_buffer[8];
		if (chaining != 0 && chaining_active == false)
			chaining_active = true;
		if (chaining_active)
			PRINT_SLOT_DBG("VENDOR_PRODUCT_ID: chaining: %d length %d", slot->number, chaining, length);
		descriptor_sdbp->vendor_product_id_len = descriptor_sdbp->vendor_product_id_len + length;

		for (i = 0; i < length; i++) {
			descriptor_sdbp->vendor_product_id[i + pos_cnt] = rx_buffer[i + 9];
		}
		pos_cnt += length;
	}

	chaining = 1;
	pos_cnt = 0;
	chaining_active = false;
	descriptor_sdbp->vendor_name_len = 0;
	while (chaining != 0) {
		if (exchange_sdbp(slot, (u8 *) DESCRIPTOR_GET_VENDOR_NAME, rx_buffer, LOG_LVL_SILENT)
		    != 0)
			goto cleanup;

		if (rx_buffer[6] == DESCRIPTOR_ERROR_CODE) {
			PRINT_SLOT_DBG("Descriptor error code returned!", slot->number);
			goto cleanup;
		}

		chaining = rx_buffer[7];
		length = rx_buffer[8];
		if (chaining != 0 && chaining_active == false)
			chaining_active = true;
		if (chaining_active)
			PRINT_SLOT_DBG("VENDOR_NAME: chaining: %d length %d", slot->number, chaining, length);
		descriptor_sdbp->vendor_name_len = descriptor_sdbp->vendor_name_len + length;

		for (i = 0; i < length; i++) {
			descriptor_sdbp->vendor_name[i + pos_cnt] = rx_buffer[i + 9];
		}
		pos_cnt += length;
	}

	chaining = 1;
	pos_cnt = 0;
	chaining_active = false;
	descriptor_sdbp->product_name_len = 0;
	while (chaining != 0) {
		if (exchange_sdbp(slot, (u8 *) DESCRIPTOR_GET_PRODUCT_NAME, rx_buffer, LOG_LVL_SILENT)
		    != 0)
			goto cleanup;

		if (rx_buffer[6] == DESCRIPTOR_ERROR_CODE) {
			PRINT_SLOT_DBG("Descriptor error code returned!", slot->number);
			goto cleanup;
		}
		chaining = rx_buffer[7];
		length = rx_buffer[8];
		if (chaining != 0 && chaining_active == false)
			chaining_active = true;
		if (chaining_active)
			PRINT_SLOT_DBG("PRODUCT_NAME: chaining: %d length %d", slot->number, chaining, length);
		descriptor_sdbp->product_name_len = descriptor_sdbp->product_name_len + length;

		for (i = 0; i < length; i++) {
			descriptor_sdbp->product_name[i + pos_cnt] = rx_buffer[i + 9];
		}
		pos_cnt += length;
	}

	if (exchange_sdbp(slot, (u8 *) DESCRIPTOR_GET_HW_VERSION, rx_buffer, LOG_LVL_SILENT) != 0)
		goto cleanup;

	if (rx_buffer[6] == DESCRIPTOR_ERROR_CODE) {
		PRINT_SLOT_DBG("Descriptor error code returned!", slot->number);
		goto cleanup;
	}

	length = (rx_buffer[1] << 8) | rx_buffer[2];
	if (length != 13) {
		PRINT_SLOT_ERR("Length of Descriptor field invalid!\n", slot->number);
		goto cleanup;
	}

	descriptor_sdbp->hw_version.major = rx_buffer[7] << 8 | rx_buffer[8];
	descriptor_sdbp->hw_version.minor = rx_buffer[9] << 8 | rx_buffer[10];
	descriptor_sdbp->hw_version.patch = rx_buffer[11] << 8 | rx_buffer[12];

	if (exchange_sdbp(slot, (u8 *) DESCRIPTOR_GET_FW_VERSION, rx_buffer, LOG_LVL_SILENT) != 0)
		goto cleanup;

	if (rx_buffer[6] == DESCRIPTOR_ERROR_CODE) {
		PRINT_SLOT_DBG("Descriptor error code returned!", slot->number);
		goto cleanup;
	}

	length = (rx_buffer[1] << 8) | rx_buffer[2];
	if (length != 14) {
		PRINT_SLOT_ERR("Length of Descriptor field invalid!\n", slot->number);
		goto cleanup;
	}

	if (rx_buffer[7] == 1)
		descriptor_sdbp->fw_version.stability = 'A';
	else if (rx_buffer[7] == 2)
		descriptor_sdbp->fw_version.stability = 'B';
	else if (rx_buffer[7] == 3)
		descriptor_sdbp->fw_version.stability = 'S';
	descriptor_sdbp->fw_version.major = rx_buffer[8] << 8 | rx_buffer[9];
	descriptor_sdbp->fw_version.minor = rx_buffer[10] << 8 | rx_buffer[11];
	descriptor_sdbp->fw_version.patch = rx_buffer[12] << 8 | rx_buffer[13];

	if (exchange_sdbp(slot, (u8 *) DESCRIPTOR_GET_MAX_SCLK_SPEED, rx_buffer, LOG_LVL_SILENT)
	    != 0)
		goto cleanup;

	if (rx_buffer[6] == DESCRIPTOR_ERROR_CODE) {
		PRINT_SLOT_DBG("Descriptor error code returned!", slot->number);
		goto cleanup;
	}

	length = (rx_buffer[1] << 8) | rx_buffer[2];
	if (length != 11) {
		PRINT_SLOT_ERR("Length of Descriptor field invalid!\n", slot->number);
		goto cleanup;
	}

	descriptor_sdbp->max_sclk_speed = rx_buffer[7] << 24 | rx_buffer[8] << 16 | rx_buffer[9] << 8 | rx_buffer[10];

	if (exchange_sdbp(slot, (u8 *) DESCRIPTOR_GET_MAX_FRAME_SIZE, rx_buffer, LOG_LVL_SILENT)
	    != 0)
		goto cleanup;

	if (rx_buffer[6] == DESCRIPTOR_ERROR_CODE) {
		PRINT_SLOT_DBG("Descriptor error code returned!", slot->number);
		goto cleanup;
	}

	length = (rx_buffer[1] << 8) | rx_buffer[2];
	if (length != 9) {
		PRINT_SLOT_ERR("Length of Descriptor field invalid!\n", slot->number);
		goto cleanup;
	}

	descriptor_sdbp->max_frame_size = rx_buffer[7] << 8 | rx_buffer[8];

	if (exchange_sdbp(slot, (u8 *) DESCRIPTOR_GET_MAX_POWER_3V3, rx_buffer, LOG_LVL_SILENT)
	    != 0)
		goto cleanup;

	if (rx_buffer[6] == DESCRIPTOR_ERROR_CODE) {
		PRINT_SLOT_DBG("Descriptor error code returned!", slot->number);
		goto cleanup;
	}

	length = (rx_buffer[1] << 8) | rx_buffer[2];
	if (length != 11) {
		PRINT_SLOT_ERR("Length of Descriptor field invalid!\n", slot->number);
		goto cleanup;
	}

	descriptor_sdbp->max_power_3v3 = rx_buffer[7] << 24 | rx_buffer[8] << 16 | rx_buffer[9] << 8 | rx_buffer[10];

	if (exchange_sdbp(slot, (u8 *) DESCRIPTOR_GET_MAX_POWER_5V0, rx_buffer, LOG_LVL_SILENT)
	    != 0)
		goto cleanup;

	if (rx_buffer[6] == DESCRIPTOR_ERROR_CODE) {
		PRINT_SLOT_DBG("Descriptor error code returned!", slot->number);
		goto cleanup;
	}

	length = (rx_buffer[1] << 8) | rx_buffer[2];
	if (length != 11) {
		PRINT_SLOT_ERR("Length of Descriptor field invalid!\n", slot->number);
		goto cleanup;
	}

	descriptor_sdbp->max_power_5v0 = rx_buffer[7] << 24 | rx_buffer[8] << 16 | rx_buffer[9] << 8 | rx_buffer[10];

	if (exchange_sdbp(slot, (u8 *) DESCRIPTOR_GET_MAX_POWER_12V0, rx_buffer, LOG_LVL_SILENT)
	    != 0)
		goto cleanup;

	if (rx_buffer[6] == DESCRIPTOR_ERROR_CODE) {
		PRINT_SLOT_DBG("Descriptor error code returned!", slot->number);
		goto cleanup;
	}

	length = (rx_buffer[1] << 8) | rx_buffer[2];
	if (length != 11) {
		PRINT_SLOT_ERR("Length of Descriptor field invalid!\n", slot->number);
		goto cleanup;
	}

	descriptor_sdbp->max_power_12v = rx_buffer[7] << 24 | rx_buffer[8] << 16 | rx_buffer[9] << 8 | rx_buffer[10];

	if (exchange_sdbp(slot, (u8 *) DESCRIPTOR_GET_SERIAL_CODE, rx_buffer, LOG_LVL_SILENT) != 0)
		goto cleanup;

	if (rx_buffer[6] == DESCRIPTOR_ERROR_CODE) {
		PRINT_SLOT_DBG("Descriptor error code returned!", slot->number);
		goto cleanup;
	}

	length = (rx_buffer[1] << 8) | rx_buffer[2];
	if (length != 23) {
		PRINT_SLOT_ERR("Length of Descriptor field invalid!\n", slot->number);
		goto cleanup;
	}

	for (i = 0; i < 16; i++) {
		descriptor_sdbp->serial_code[i] = rx_buffer[i + 7];
	}

	if (exchange_sdbp(slot, (u8 *) DESCRIPTOR_GET_BOOTLOADER_STATE, rx_buffer, LOG_LVL_SILENT) != 0)
		goto cleanup;

	if (rx_buffer[6] == DESCRIPTOR_ERROR_CODE) {
		PRINT_SLOT_DBG("Descriptor error code returned!", slot->number);
		goto cleanup;
	}

	length = (rx_buffer[1] << 8) | rx_buffer[2];
	if (length != 8) {
		PRINT_SLOT_ERR("Length of Descriptor field invalid!\n", slot->number);
		goto cleanup;
	}

	descriptor_sdbp->bootloader_state = rx_buffer[7];

	if (old_rid == 0) {
		prandom_bytes(&rand, sizeof(rand));
		while (rand == descriptor_sdbp->rid) {
			prandom_bytes(&rand, sizeof(rand));
		}
		descriptor_sdbp->rid = rand;
	} else {
		descriptor_sdbp->rid = old_rid;
	}

	if (!force)
		print_descriptor(slot, descriptor_sdbp);

	atomic_dec(&descriptor_sdbp->is_valid);
	kfree(rx_buffer);
	if (!force)
		atomic_set(&slot->write_count, -1);
	return 0;

 cleanup:
	slot->session_stats.descriptor_failed++;
	kfree(rx_buffer);
	atomic_dec(&descriptor_sdbp->is_valid);
	return -1;
}
