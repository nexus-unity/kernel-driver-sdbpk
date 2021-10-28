#include <linux/spi/spi.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include "descriptor.h"
#include "communication.h"
#include "sdbp.h"
#include "crc16ccitt.h"
#include "debug.h"

ssize_t spi_api_exchange(struct Slot * slot, u8 * tx_buffer, u8 * rx_buffer)
{

	struct spi_transfer t = {.tx_buf = tx_buffer,.rx_buf = rx_buffer,.len = slot->frame_size,.speed_hz = slot->speed_sclk,
	};

	struct spi_message m;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	return spi_sync(slot->spi_device, &m);
}

int prepare_frame(struct Slot *slot, u8 * data)
{
	u16 cnt;
	u16 length;
	u16 calc_crc;

	length = (data[1] << 8) | data[2];

	if (length <= MAXIMUM_FRAME_SIZE)
		slot->crc_size = DEFAULT_CRC_SIZE;
	else
		slot->crc_size = CRC32_SIZE;

	if (length > (slot->frame_size - slot->crc_size)) {
		PRINT_SLOT_ERR("Frame size/length error!\n", slot->number);
		return -1;
	}

	for (cnt = length; cnt < (slot->frame_size - slot->crc_size); cnt++) {
		data[cnt] = DUMMY_PATTERN;
	}

	if (slot->crc_size == DEFAULT_CRC_SIZE) {
		// calc_crc =
		//              crc_ccitt(0, (unsigned char *)data, slot->frame_size - slot->crc_size);
		calc_crc = crc16_ccitt((unsigned char *)data, slot->frame_size - slot->crc_size, 0);

		data[slot->frame_size - slot->crc_size] = (calc_crc & 0xff00) >> 8;
		data[slot->frame_size - slot->crc_size + 1] = (calc_crc & 0x00ff);
	} else {
		PRINT_SLOT_ERR("CRC32 not implemented!\n", slot->number);
		return -1;
	}

	return 0;
}

void print_frame(struct Slot *slot, u8 * data)
{
	u16 i;
	u16 length;
	length = (data[1] << 8) | data[2];

	if (length > slot->frame_size)
		length = slot->frame_size;

	if (length == 0)
		PRINT_SLOT_ERR("Corrupted data: Length is zero!\n", slot->number);
	else {
		PRINT_SLOT_ERR("Corrupted data: ", slot->number);
		for (i = 0; i < length; i++) {
			printk(KERN_CONT "%#02x ", data[i]);
		}
		printk(KERN_CONT "[0x7F,...,0x7F] ");
		for (i = slot->frame_size - slot->crc_size; i < slot->frame_size; i++) {
			printk(KERN_CONT "%#02x ", data[i]);
		}
	}
}

int check_crc(struct Slot *slot, u8 * data, u8 log_lvl)
{
	u16 calc_crc;
	u16 rec_crc;

	if (slot->crc_size == DEFAULT_CRC_SIZE) {
		calc_crc = crc16_ccitt((unsigned char *)data, slot->frame_size - slot->crc_size, 0);
		rec_crc = ((data[slot->frame_size - slot->crc_size] << 8 & 0xff00) | data[slot->frame_size - slot->crc_size + 1]);
	} else {
		PRINT_SLOT_ERR("CRC32 check not supported!\n", slot->number);
		return -1;
	}

	if (calc_crc != rec_crc) {
		if (log_lvl > LOG_LVL_SILENT)
			PRINT_SLOT_ERR("CRC ERROR: (calc) %#04x!=%#04x (rec).\n", slot->number, calc_crc, rec_crc);
		return -1;
	}

	return 0;
}

int wait_for_interrupt(struct Slot *slot, u16 timeout_ms)
{
	long ret = 0;
	int interrupt = atomic_read(&slot->interrupt_arrived);

	if (interrupt != 1) {

		ret = wait_event_timeout(slot->queue, atomic_read(&slot->interrupt_arrived) > 0, msecs_to_jiffies(timeout_ms));

		if (ret == 0) {
			//PRINT_SLOT_DBG("Interrupt timeout!\n",slot->number); //DEBUGGED BY CALLING FUNCTION
			return -1;
		} else if (ret == -ERESTARTSYS) {
			PRINT_SLOT_DBG("Interrupted by system!\n", slot->number);
			return -2;
		}
	}

	return 0;
}

int get_notification(struct Slot *slot)
{
	int ret;
	u8 *rx_buffer;
	u16 length;
	rx_buffer = kcalloc(MAXIMUM_FRAME_SIZE, sizeof(u8), GFP_KERNEL);	// Allocate memory
	ret = 0;

	if (atomic_read(&slot->notification.length) > 0) {
		PRINT_SLOT_DBG("Notification is not received because buffer is full!\n", slot->number);
		ret = -2;
	} else {
		if (exchange_sdbp(slot, (u8 *) NOTIFICATION, rx_buffer, LOG_LVL_SILENT)
		    != 0) {
			PRINT_SLOT_DBG("Notification exchange failed!\n", slot->number);
			ret = -1;
			slot->session_stats.transmission_errors--;
			slot->session_stats.notifications_failed++;
		} else {
			length = (rx_buffer[1] << 8) | rx_buffer[2];
			if (length >= MAXIMUM_FRAME_SIZE || length >= PAGE_SIZE) {	// sysfs max. size is PAGE_SIZE
				PRINT_SLOT_DBG("Notification length invalid!\n", slot->number);
				slot->session_stats.notifications_failed++;
				ret = -1;
			} else {
				atomic_set(&slot->notification.length, length - 4);
				memcpy(slot->notification.data, rx_buffer + 4, length - 4);
				slot->session_stats.notifications++;
				wake_up(&slot->notification.wait_for_notification);
				ret = 0;
			}
		}
	}
	kfree(rx_buffer);
	return ret;
}

int sync_com(struct Slot *slot)
{
	int ret, i;
	u8 *tx_buffer, *rx_buffer;
	tx_buffer = kcalloc(MAXIMUM_FRAME_SIZE, sizeof(u8), GFP_KERNEL);	// Allocate memory
	rx_buffer = kcalloc(MAXIMUM_FRAME_SIZE, sizeof(u8), GFP_KERNEL);	// Allocate memory
	ret = 0;
	i = 0;
	do {

		if (exchange_sdbp(slot, (u8 *) DESCRIPTOR_GET_PROTOCOL_VERSION, rx_buffer, LOG_LVL_SILENT) != 0) {
			usleep_range(3000, 3500);
		} else {
			PRINT_SLOT_DBG("State sync successful.\n", slot->number);
			break;
		}
		i++;
	}
	while (i != 3);

	if (i == 3)
		ret = -1;
	else
		ret = 0;

	kfree(tx_buffer);
	kfree(rx_buffer);
	return ret;
}

int check_frame_size_change(u8 * data, u16 length, u32 max_frame_size, struct Slot *slot)
{
	if ((data[4] == 0x01) && (data[5] == 0x03) && (data[6] == 0x07)
	    && (length == 9)) {
		u32 frame_size_request = data[7] << 8 | data[8];
		if (max_frame_size >= frame_size_request && frame_size_request >= 64) {
			PRINT_SLOT_DBG("Frame size changed requested to %d bytes\n", slot->number, frame_size_request);
			return frame_size_request;
		} else
			PRINT_SLOT_ERR("Frame size change request invalid!\n", slot->number);
		return -1;
	} else
		return 0;
}

int change_frame_size(u8 * data, u16 length, u32 max_frame_size, struct Slot *slot)
{
	if ((data[4] == 0x01) && (data[5] == 0x03) && (data[6] == 0x07)
	    && (data[7] == 0x00) && (length == 8)) {
		PRINT_SLOT_DBG("Frame size changed from %d bytes to %d bytes\n", slot->number, slot->frame_size, max_frame_size);
		slot->frame_size = max_frame_size;
		return 1;
	} else
		return 0;
}

int check_sclk_change(u8 * data, u16 length, u32 max_speed_khz, struct Slot *slot)
{
	if ((data[4] == 0x03) && (data[5] == 0x03) && (length > 6)) {
		PRINT_SLOT_DBG("Send PWR_MGMT %d\n", slot->number, data[6]);
	}

	if ((data[4] == 0x01) && (data[5] == 0x03) && (data[6] == 0x02)) {
		PRINT_SLOT_DBG("Send MODE_SUSPEND\n", slot->number);
	}
	//if ((data[4] == 0x01) && (data[5] == 0x03) && (data[6] == 0x03)) {
	//            PRINT_SLOT_DBG("Send MODE_RUN\n", slot->number);
	//}

	if ((data[4] == 0x01) && (data[5] == 0x03) && (data[6] == 0x08)
	    && (length == 11)) {
		u32 speed_request = data[7] << 24 | data[8] << 16 | data[9] << 8 | data[10];
		if (max_speed_khz >= speed_request && speed_request >= 100) {
			PRINT_SLOT_DBG("Speed changed requested to %d kHz\n", slot->number, speed_request);
			return speed_request;
		} else
			PRINT_SLOT_ERR("Speed change request invalid!\n", slot->number);
		return -1;
	} else
		return 0;
}

int change_sclk(u8 * data, u16 length, u32 speed_khz, struct Slot *slot)
{
	if ((data[4] == 0x01) && (data[5] == 0x03) && (data[6] == 0x08)
	    && (data[7] == 0x00) && (length == 8)) {
		PRINT_SLOT_DBG("Speed changed from %d kHz to %d kHz\n", slot->number, slot->speed_sclk / 1000, speed_khz);
		slot->speed_sclk = speed_khz * 1000;
		return 1;
	} else
		return 0;
}

int update_descriptor(u8 * data, u16 length, struct Slot *slot)
{
	if ((data[4] == 0x01) && (data[5] == 0x03) && (data[6] == 0x09)
	    && (data[7] == 0x00) && (length == 8)) {
		PRINT_SLOT_DBG("Update descriptor requested.\n", slot->number);
		if (!gpio_get_value(slot->interrupt_pin)) {
			usleep_range(500, 1000);	// Delay until interrupt goes high
		}
		if (get_descriptor(slot, &slot->descriptor, 1, slot->descriptor.rid) != 0) {
			PRINT_SLOT_DBG("Update descriptor failed.\n", slot->number);
			return -1;
		}
		PRINT_SLOT_DBG("Update descriptor done.\n", slot->number);
		return 1;
	} else
		return 0;
}

int exchange_sdbp(struct Slot *slot, u8 * data, u8 * rx_buffer, u8 log_lvl)
{
	u8 *tx_buffer_tmp;
	u8 *dummy_buffer;
	u16 length;
	u32 sclk_change;
	u32 frame_size_change;
	u32 wait_timeout = 250;	//ms
	u8 wait = false;
	u8 cleanup_later = false;
	u8 retransmit = false;
	tx_buffer_tmp = kcalloc(MAXIMUM_FRAME_SIZE, sizeof(u8), GFP_KERNEL);	// Allocate memory
	dummy_buffer = kcalloc(MAXIMUM_FRAME_SIZE, sizeof(u8), GFP_KERNEL);	// Allocate memory
	length = (data[1] << 8) | data[2];
	if (length > (MAXIMUM_FRAME_SIZE - DEFAULT_CRC_SIZE)) {
		PRINT_SLOT_ERR("Frame size bigger than 4096 bytes is not supported!", slot->number);
		goto cleanup;
	}

	memcpy(tx_buffer_tmp, data, length);
	sclk_change = check_sclk_change(tx_buffer_tmp, length, slot->descriptor.max_sclk_speed, slot);
	frame_size_change = check_frame_size_change(tx_buffer_tmp, length, slot->descriptor.max_frame_size, slot);
	if (prepare_frame(slot, tx_buffer_tmp) != 0) {
		goto cleanup;
	}

	do {
		atomic_set(&slot->interrupt_arrived, 0);
		if (spi_api_exchange(slot, tx_buffer_tmp, dummy_buffer) < 0)
			PRINT_SLOT_ERR("Low level spi transfer failed (send)!\n", slot->number);
		do {
			if (wait_for_interrupt(slot, wait_timeout) != 0) {
				if (log_lvl > LOG_LVL_SILENT)
					PRINT_SLOT_ERR("Interrupt timed out after %d ms!\n", slot->number, wait_timeout);
				goto cleanup;
			}
			wait = false;
			if (!retransmit) {
				memcpy(tx_buffer_tmp, DUMMY_DUMMY, sizeof(DUMMY_DUMMY));
				if (prepare_frame(slot, tx_buffer_tmp) != 0) {
					goto cleanup;
				}

				atomic_set(&slot->interrupt_arrived, 0);
				if (spi_api_exchange(slot, tx_buffer_tmp, rx_buffer) < 0)
					PRINT_SLOT_ERR("Low level spi transfer failed (received)!\n", slot->number);
				wait_for_interrupt(slot, 3);	// CTS, legacy devices do not trigger an interrupt therefore timeout silently
				atomic_set(&slot->interrupt_arrived, 0);	// Do this after retransmit check
				length = (dummy_buffer[1] << 8) | dummy_buffer[2];
				if (check_crc(slot, dummy_buffer, log_lvl) != 0 || length == 0 || length > (slot->frame_size - slot->crc_size)) {
					if (log_lvl > LOG_LVL_SILENT) {
						PRINT_SLOT_ERR("Dummy invalid because of crc or length issue!\n", slot->number);
						print_frame(slot, dummy_buffer);
					}
					cleanup_later = true;
				}
			} else {
				memcpy(rx_buffer, dummy_buffer, MAXIMUM_FRAME_SIZE);
			}

			length = (rx_buffer[1] << 8) | rx_buffer[2];
			if (check_crc(slot, rx_buffer, log_lvl) != 0 || length == 0 || length > (slot->frame_size - slot->crc_size)) {
				if ((!retransmit)) {
					PRINT_SLOT_DBG("Retransmit because of CRC error in response!\n", slot->number);
					usleep_range(2000, 2500);
					memcpy(tx_buffer_tmp, DUMMY_DUMMY, sizeof(DUMMY_DUMMY));
					retransmit = true;
					break;
				} else
					cleanup_later = true;
				if (log_lvl > LOG_LVL_SILENT) {
					PRINT_SLOT_ERR("Response invalid because of crc or length issue!\n", slot->number);
					print_frame(slot, rx_buffer);
				}
			}

			if (rx_buffer[0] == SDBP_MSG_TYPE_ACKNOWLEDGEMENT && !retransmit) {
				PRINT_SLOT_DBG("Retransmit message because type is acknowledgement!\n", slot->number);
				usleep_range(1000, 1500);
				memcpy(tx_buffer_tmp, DUMMY_DUMMY, sizeof(DUMMY_DUMMY));
				retransmit = true;
				break;
			}

			if (cleanup_later) {
				if (log_lvl > LOG_LVL_SILENT)
					PRINT_SLOT_ERR("Transmission aborted because of previous error(s)!\n", slot->number);
				goto cleanup;
			}

			if (rx_buffer[0] != SDBP_MSG_TYPE_RESPONSE) {
				if (log_lvl > LOG_LVL_SILENT) {
					PRINT_SLOT_ERR("Received message type is wrong (not 0x02)!\n", slot->number);
					print_frame(slot, rx_buffer);
				}
				goto cleanup;
			} else {
				if (rx_buffer[4] == SDBP_CLASSID_CORE && rx_buffer[5] == SDBP_C_TRANSACTION_ERROR) {
					if (rx_buffer[6] == SDBP_C_TRANSACTION_ERROR_FRAME_CRC) {
						if (log_lvl > LOG_LVL_SILENT)
							PRINT_SLOT_ERR("Last message received by device with CRC error!\n", slot->number);
					} else {
						if (log_lvl > LOG_LVL_SILENT) {
							char error[100];
							if (rx_buffer[6] == SDBP_C_TRANSACTION_ERROR_MESSAGE_TYPE_INVALID)
								strncpy(error, "MESSAGE_TYPE_INVALID\0", sizeof(error));
							else if (rx_buffer[6] == SDBP_C_TRANSACTION_ERROR_CLASS_IDENTIFIER_INVALID)
								strncpy(error, "CLASS_IDENTIFIER_INVALID\0", sizeof(error));
							else if (rx_buffer[6] == SDBP_C_TRANSACTION_ERROR_CLASS_INVALID)
								strncpy(error, "CLASS_INVALID\0", sizeof(error));
							else if (rx_buffer[6] == SDBP_C_TRANSACTION_ERROR_DATA_LENGTH_INVALID)
								strncpy(error, "DATA_LENGTH_INVALID\0", sizeof(error));
							else if (rx_buffer[6] == SDBP_C_TRANSACTION_ERROR_DEVICE_WRONG_MODE)
								strncpy(error, "DEVICE_WRONG_MODE\0", sizeof(error));
							else
								strncpy(error, "UNKNOWN", sizeof(error));
							PRINT_SLOT_ERR("Last message received by device with transaction error! (%s)\n", slot->number, error);
						}
					}

					if (log_lvl > LOG_LVL_SILENT)
						print_frame(slot, rx_buffer);
					goto cleanup;
				}
			}

			if ((rx_buffer[4] == 0x03) && (rx_buffer[5] == 0x03) && (length > 6)) {
				PRINT_SLOT_DBG("Recv PWR_MGMT %d\n", slot->number, rx_buffer[6]);
			}

			if (rx_buffer[4] == SDBP_CLASSID_CORE && rx_buffer[5] == SDBP_C_WAIT && rx_buffer[6] == SDBP_C_WAIT_WAIT && length == 11) {
				wait_timeout = rx_buffer[7] << 24 | rx_buffer[8] << 16 | rx_buffer[9] << 8 | rx_buffer[10];
				wait = true;
				wait_timeout = wait_timeout / 1000;
				PRINT_SLOT_DBG("Device requested wait time: %dms", slot->number, wait_timeout);
			} else {
				if (sclk_change > 0)
					change_sclk(rx_buffer, length, sclk_change, slot);
				if (frame_size_change > 0)
					change_frame_size(rx_buffer, length, frame_size_change, slot);
				if (update_descriptor(rx_buffer, length, slot) < 0) {
					if (log_lvl > LOG_LVL_SILENT)
						PRINT_SLOT_ERR("Updating descriptor failed!\n", slot->number);
					goto cleanup;
				}
			}
			retransmit = false;
		} while (wait);
	} while (retransmit);
	kfree(dummy_buffer);
	kfree(tx_buffer_tmp);
	return 0;
 cleanup:
	kfree(tx_buffer_tmp);
	kfree(dummy_buffer);
	slot->session_stats.transmission_errors++;
	return -1;
}

int init_slot(struct Slot *slot)
{
	u8 ret;
	struct spi_master *master;
	int irq_number;
	int i;
	int TRIES = 20;
	//Register information about your slave device:
	struct spi_board_info spi_device_info = {
		.modalias = "sdbpk",
		.max_speed_hz = slot->speed_sclk,	//speed your device (slave) can handle
		.bus_num = slot->spi_bus,
		.chip_select = slot->spi_chip_select,
		.mode = 0,
	};
	ret = 0;
	for (i = 0; i < TRIES; i++) {
		msleep_interruptible(1000);
		master = spi_busnum_to_master(spi_device_info.bus_num);
		if (!master) {
			PRINT_SLOT_ERR("SPI bus %d not found.\n", slot->number, slot->spi_bus);
			ret = -ENODEV;
			continue;
		}
		// create a new slave device, given the master and device info
		slot->spi_device = spi_new_device(master, &spi_device_info);
		if (!slot->spi_device) {
			PRINT_SLOT_DBG("Failed to create SPI slave %d %d.\n", slot->number, slot->spi_bus, slot->spi_chip_select);
			ret = -ENODEV;
			continue;
		}
		ret = 0;
		break;
	}

	if (ret != 0) {
		PRINT_SLOT_ERR("Failed to find SPI bus.cs %d.%d after %d attempts.\n", slot->number, slot->spi_bus, slot->spi_chip_select, i);
		return ret;
	} else {
		PRINT_SLOT_DBG("Found SPI bus.cs %d.%d after %d attempts.\n", slot->number, slot->spi_bus, slot->spi_chip_select, i);
	}

	slot->spi_device->bits_per_word = 8;
	ret = spi_setup(slot->spi_device);
	if (ret) {
		PRINT_SLOT_ERR("Failed to setup SPI slave!\n", slot->number);
		spi_unregister_device(slot->spi_device);
		return -ENODEV;
	}

	{			// GPIO init
		if (gpio_request(slot->interrupt_pin, "sysfs") != 0) {
			spi_unregister_device(slot->spi_device);
			PRINT_SLOT_ERR("Failed to request GPIO!\n", slot->number);
			return -EIO;
		}
		if (gpio_direction_input(slot->interrupt_pin) != 0 || gpio_export(slot->interrupt_pin, false) != 0) {
			spi_unregister_device(slot->spi_device);
			gpio_free(slot->interrupt_pin);
			PRINT_SLOT_ERR("Failed to set GPIO input!\n", slot->number);
			return -EIO;
		}

		irq_number = gpio_to_irq(slot->interrupt_pin);
		if (request_irq(irq_number, gpio_rising_interrupt, IRQF_TRIGGER_FALLING | IRQF_ONESHOT, "gpio_rising", NULL)) {
			PRINT_SLOT_ERR("Failed requesting IRQ %d!", slot->number, irq_number);
			gpio_unexport(slot->interrupt_pin);
			gpio_free(slot->interrupt_pin);
			spi_unregister_device(slot->spi_device);
			return (-EIO);
		} else {
			slot->irq_number = irq_number;
		}
	}

	init_waitqueue_head(&slot->queue);
	PRINT_SLOT_NORM("connected to bus:%d cs:%d int:%d\n", slot->number, slot->spi_bus, slot->spi_chip_select, slot->interrupt_pin);
	slot->valid = 1;
	return 0;
}
