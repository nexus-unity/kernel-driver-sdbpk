#include "attributes.h"
#include <linux/module.h>
#include "sdbp.h"
#include "descriptor.h"
#include "debug.h"

int validate(struct device *dev)
{
	int index = find_slot(dev->devt);
	if (index < 0) {
		PRINT_ERR("Index not found!\n");
		return -EIO;
	}
	// if (!atomic_inc_and_test(&get_slot(index)->descriptor.is_valid)) {
	//      PRINT_SLOT_ERR("Descriptor is not available.\n", get_slot(index)->number);
	//      atomic_dec(&get_slot(index)->descriptor.is_valid);
	//      return -EBUSY;
	// }
	return index;
}

int validate_notification(struct device *dev)
{
	int index = find_slot(dev->devt);
	if (index < 0) {
		PRINT_ERR("Index not found!\n");
		return -EIO;
	}

	if (!atomic_inc_and_test(&get_slot(index)->notification.lock)) {
		PRINT_SLOT_ERR("Notification is already locked! %d \n", get_slot(index)->number, atomic_read(&get_slot(index)->notification.lock));
		atomic_dec(&get_slot(index)->notification.lock);
		return -EBUSY;
	}
	return index;
}

ssize_t get_vendor_name(struct device * dev, struct device_attribute * attr, char *buf)
{
	int char_cnt;
	int index = validate(dev);
	if (index < 0)
		return index;
	if (atomic_read(&get_slot(index)->descriptor.is_valid) < 0)
		char_cnt = snprintf(buf, get_slot(index)->descriptor.vendor_name_len, (u8 *) get_slot(index)->descriptor.vendor_name);
	else {
		if (atomic_read(&get_slot(index)->descriptor_old.is_valid) >= 0)
			return -EAGAIN;
		char_cnt = snprintf(buf, get_slot(index)->descriptor_old.vendor_name_len, (u8 *) get_slot(index)->descriptor_old.vendor_name);
	}

	return char_cnt + 1;
}

ssize_t get_product_name(struct device * dev, struct device_attribute * attr, char *buf)
{
	int char_cnt;
	int index = validate(dev);
	if (index < 0)
		return index;
	if (atomic_read(&get_slot(index)->descriptor.is_valid) < 0)
		char_cnt = snprintf(buf, get_slot(index)->descriptor.product_name_len, (u8 *) get_slot(index)->descriptor.product_name);
	else {
		if (atomic_read(&get_slot(index)->descriptor_old.is_valid) >= 0)
			return -EAGAIN;
		char_cnt = snprintf(buf, get_slot(index)->descriptor_old.product_name_len, (u8 *) get_slot(index)->descriptor_old.product_name);
	}

	return char_cnt + 1;
}

ssize_t get_vendor_product_id(struct device * dev, struct device_attribute * attr, char *buf)
{
	int char_cnt;
	int index = validate(dev);
	if (index < 0)
		return index;
	if (atomic_read(&get_slot(index)->descriptor.is_valid) < 0)
		char_cnt = snprintf(buf, get_slot(index)->descriptor.vendor_product_id_len, (u8 *) get_slot(index)->descriptor.vendor_product_id);
	else {
		if (atomic_read(&get_slot(index)->descriptor_old.is_valid) >= 0)
			return -EAGAIN;
		char_cnt = snprintf(buf, get_slot(index)->descriptor_old.vendor_product_id_len, (u8 *) get_slot(index)->descriptor_old.vendor_product_id);
	}

	return char_cnt + 1;
}

ssize_t get_max_power_3v3(struct device * dev, struct device_attribute * attr, char *buf)
{
	u32 value;
	int char_cnt;
	int index = validate(dev);
	if (index < 0)
		return index;

	if (atomic_read(&get_slot(index)->descriptor.is_valid) < 0)
		value = get_slot(index)->descriptor.max_power_3v3;
	else {
		if (atomic_read(&get_slot(index)->descriptor_old.is_valid) >= 0)
			return -EAGAIN;
		value = get_slot(index)->descriptor_old.max_power_3v3;
	}
	char_cnt = snprintf(buf, 10 + 1, "%u", value);

	return char_cnt + 1;
}

ssize_t get_max_power_5v0(struct device * dev, struct device_attribute * attr, char *buf)
{
	u32 value;
	int char_cnt;
	int index = validate(dev);
	if (index < 0)
		return index;

	if (atomic_read(&get_slot(index)->descriptor.is_valid) < 0)
		value = get_slot(index)->descriptor.max_power_5v0;
	else {
		if (atomic_read(&get_slot(index)->descriptor_old.is_valid) >= 0)
			return -EAGAIN;
		value = get_slot(index)->descriptor_old.max_power_5v0;
	}

	char_cnt = snprintf(buf, 10 + 1, "%u", value);

	return char_cnt + 1;
}

ssize_t get_max_power_12v(struct device * dev, struct device_attribute * attr, char *buf)
{
	u32 value;
	int char_cnt;
	int index = validate(dev);
	if (index < 0)
		return index;

	if (atomic_read(&get_slot(index)->descriptor.is_valid) < 0)
		value = get_slot(index)->descriptor.max_power_12v;
	else {
		if (atomic_read(&get_slot(index)->descriptor_old.is_valid) >= 0)
			return -EAGAIN;
		value = get_slot(index)->descriptor_old.max_power_12v;
	}

	char_cnt = snprintf(buf, 10 + 1, "%u", value);

	return char_cnt + 1;
}

ssize_t get_max_sclk_speed(struct device * dev, struct device_attribute * attr, char *buf)
{
	u32 value;
	int char_cnt;
	int index = validate(dev);
	if (index < 0)
		return index;

	if (atomic_read(&get_slot(index)->descriptor.is_valid) < 0)
		value = get_slot(index)->descriptor.max_sclk_speed;
	else {
		if (atomic_read(&get_slot(index)->descriptor_old.is_valid) >= 0)
			return -EAGAIN;
		value = get_slot(index)->descriptor_old.max_sclk_speed;
	}

	char_cnt = snprintf(buf, 10 + 1, "%u", value);

	return char_cnt + 1;
}

ssize_t get_max_frame_size(struct device * dev, struct device_attribute * attr, char *buf)
{
	u32 value;
	int char_cnt;
	int index = validate(dev);
	if (index < 0)
		return index;

	if (atomic_read(&get_slot(index)->descriptor.is_valid) < 0)
		value = get_slot(index)->descriptor.max_frame_size;
	else {
		if (atomic_read(&get_slot(index)->descriptor_old.is_valid) >= 0)
			return -EAGAIN;
		value = get_slot(index)->descriptor_old.max_frame_size;
	}

	char_cnt = snprintf(buf, 5 + 1, "%u", value);

	return char_cnt + 1;
}

ssize_t get_bootloader_state(struct device * dev, struct device_attribute * attr, char *buf)
{
	u32 value;
	int char_cnt;
	int index = validate(dev);
	if (index < 0)
		return index;

	if (atomic_read(&get_slot(index)->descriptor.is_valid) < 0)
		value = get_slot(index)->descriptor.bootloader_state;
	else {
		if (atomic_read(&get_slot(index)->descriptor_old.is_valid) >= 0)
			return -EAGAIN;
		value = get_slot(index)->descriptor_old.bootloader_state;
	}

	char_cnt = snprintf(buf, 3 + 1, "%u", value);

	return char_cnt + 1;
}

ssize_t get_fw_version(struct device * dev, struct device_attribute * attr, char *buf)
{
	int char_cnt;
	int index = validate(dev);
	if (index < 0)
		return index;
	if (atomic_read(&get_slot(index)->descriptor.is_valid) < 0)
		char_cnt = snprintf(buf, 19 + 1, "%c.%05u.%05u.%05u",
				    get_slot(index)->descriptor.fw_version.stability,
				    get_slot(index)->descriptor.fw_version.major,
				    get_slot(index)->descriptor.fw_version.minor, get_slot(index)->descriptor.fw_version.patch);
	else {
		if (atomic_read(&get_slot(index)->descriptor_old.is_valid) >= 0)
			return -EAGAIN;
		char_cnt = snprintf(buf, 19 + 1, "%c.%05u.%05u.%05u",
				    get_slot(index)->descriptor_old.fw_version.stability,
				    get_slot(index)->descriptor_old.fw_version.major,
				    get_slot(index)->descriptor_old.fw_version.minor, get_slot(index)->descriptor_old.fw_version.patch);
	}

	return char_cnt + 1;
}

ssize_t get_hw_version(struct device * dev, struct device_attribute * attr, char *buf)
{
	int char_cnt;
	int index = validate(dev);
	if (index < 0)
		return index;

	if (atomic_read(&get_slot(index)->descriptor.is_valid) < 0)
		char_cnt = snprintf(buf, 17 + 1, "%05u.%05u.%05u",
				    get_slot(index)->descriptor.hw_version.major,
				    get_slot(index)->descriptor.hw_version.minor, get_slot(index)->descriptor.hw_version.patch);
	else {
		if (atomic_read(&get_slot(index)->descriptor_old.is_valid) >= 0)
			return -EAGAIN;
		char_cnt = snprintf(buf, 17 + 1, "%05u.%05u.%05u",
				    get_slot(index)->descriptor_old.hw_version.major,
				    get_slot(index)->descriptor_old.hw_version.minor, get_slot(index)->descriptor_old.hw_version.patch);
	}

	return char_cnt + 1;
}

ssize_t get_protocol_version(struct device * dev, struct device_attribute * attr, char *buf)
{
	int char_cnt;
	int index = validate(dev);
	if (index < 0)
		return index;

	if (atomic_read(&get_slot(index)->descriptor.is_valid) < 0)
		char_cnt = snprintf(buf, 17 + 1, "%05u.%05u.%05u",
				    get_slot(index)->descriptor.protocol_version.major,
				    get_slot(index)->descriptor.protocol_version.minor, get_slot(index)->descriptor.protocol_version.patch);
	else {
		if (atomic_read(&get_slot(index)->descriptor_old.is_valid) >= 0)
			return -EAGAIN;
		char_cnt = snprintf(buf, 17 + 1, "%05u.%05u.%05u",
				    get_slot(index)->descriptor_old.protocol_version.major,
				    get_slot(index)->descriptor_old.protocol_version.minor, get_slot(index)->descriptor_old.protocol_version.patch);
	}

	return char_cnt + 1;
}

ssize_t get_serial_code(struct device * dev, struct device_attribute * attr, char *buf)
{
	int char_cnt;
	int index = validate(dev);
	if (index < 0)
		return index;

	if (atomic_read(&get_slot(index)->descriptor.is_valid) < 0)
		char_cnt = snprintf(buf, 16 * 2 + 7 + 1,
				    "%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X",
				    get_slot(index)->descriptor.serial_code[0],
				    get_slot(index)->descriptor.serial_code[1],
				    get_slot(index)->descriptor.serial_code[2],
				    get_slot(index)->descriptor.serial_code[3],
				    get_slot(index)->descriptor.serial_code[4],
				    get_slot(index)->descriptor.serial_code[5],
				    get_slot(index)->descriptor.serial_code[6],
				    get_slot(index)->descriptor.serial_code[7],
				    get_slot(index)->descriptor.serial_code[8],
				    get_slot(index)->descriptor.serial_code[9],
				    get_slot(index)->descriptor.serial_code[10],
				    get_slot(index)->descriptor.serial_code[11],
				    get_slot(index)->descriptor.serial_code[12],
				    get_slot(index)->descriptor.serial_code[13],
				    get_slot(index)->descriptor.serial_code[14], get_slot(index)->descriptor.serial_code[15]
		    );
	else {
		if (atomic_read(&get_slot(index)->descriptor_old.is_valid) >= 0)
			return -EAGAIN;
		char_cnt = snprintf(buf, 16 * 2 + 7 + 1,
				    "%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X",
				    get_slot(index)->descriptor_old.serial_code[0],
				    get_slot(index)->descriptor_old.serial_code[1],
				    get_slot(index)->descriptor_old.serial_code[2],
				    get_slot(index)->descriptor_old.serial_code[3],
				    get_slot(index)->descriptor_old.serial_code[4],
				    get_slot(index)->descriptor_old.serial_code[5],
				    get_slot(index)->descriptor_old.serial_code[6],
				    get_slot(index)->descriptor_old.serial_code[7],
				    get_slot(index)->descriptor_old.serial_code[8],
				    get_slot(index)->descriptor_old.serial_code[9],
				    get_slot(index)->descriptor_old.serial_code[10],
				    get_slot(index)->descriptor_old.serial_code[11],
				    get_slot(index)->descriptor_old.serial_code[12],
				    get_slot(index)->descriptor_old.serial_code[13],
				    get_slot(index)->descriptor_old.serial_code[14], get_slot(index)->descriptor_old.serial_code[15]
		    );
	}

	return char_cnt + 1;
}

ssize_t get_notification_data(struct device * dev, struct device_attribute * attr, char *buf)
{
	int char_cnt;
	int i;
	int ret;
	int index = validate_notification(dev);

	if (index < 0)
		return index;

	PRINT_SLOT_DBG("Notification socket opened.\n", get_slot(index)->number);
	if (atomic_read(&get_slot(index)->notification.length) == 0) {
		ret = wait_event_interruptible(get_slot(index)->notification.wait_for_notification, atomic_read(&get_slot(index)->notification.length) != 0);
		if (ret == -ERESTARTSYS) {
			PRINT_SLOT_DBG("Notification socket call aborted.\n", get_slot(index)->number);
			atomic_dec(&get_slot(index)->notification.lock);
			return -EIO;
		}
	}

	if (atomic_read(&get_slot(index)->notification.length) < 0) {
		atomic_dec(&get_slot(index)->notification.lock);
		return -ENODEV;
	}

	buf[0] = '0';
	buf[1] = 'x';
	char_cnt = 2;

	for (i = 0; i < atomic_read(&get_slot(index)->notification.length); i++) {
		char_cnt += snprintf(buf + char_cnt, 2 + 1, "%02X", get_slot(index)->notification.data[i]);
	}

	atomic_set(&get_slot(index)->notification.length, 0);
	atomic_dec(&get_slot(index)->notification.lock);
	return char_cnt + 1;
}

ssize_t get_stats_failed_transmissions(struct device * dev, struct device_attribute * attr, char *buf)
{
	int char_cnt;
	int index = validate(dev);
	if (index < 0)
		return index;

	char_cnt = snprintf(buf, 10 + 1, "%u", get_slot(index)->session_stats.transmission_errors);

	return char_cnt + 1;
}

ssize_t get_stats_notifications(struct device * dev, struct device_attribute * attr, char *buf)
{
	int char_cnt;
	int index = validate(dev);
	if (index < 0)
		return index;

	char_cnt = snprintf(buf, 10 + 1, "%u", get_slot(index)->session_stats.notifications);

	return char_cnt + 1;
}

ssize_t get_stats_failed_notifications(struct device * dev, struct device_attribute * attr, char *buf)
{
	int char_cnt;
	int index = validate(dev);
	if (index < 0)
		return index;

	char_cnt = snprintf(buf, 10 + 1, "%u", get_slot(index)->session_stats.notifications_failed);

	return char_cnt + 1;
}

ssize_t get_stats_failed_descriptors(struct device * dev, struct device_attribute * attr, char *buf)
{
	int char_cnt;
	int index = validate(dev);
	if (index < 0)
		return index;

	char_cnt = snprintf(buf, 10 + 1, "%u", get_slot(index)->session_stats.descriptor_failed);

	return char_cnt + 1;
}

ssize_t get_rid(struct device * dev, struct device_attribute * attr, char *buf)
{
	u32 value;
	int char_cnt;
	int index = validate(dev);
	if (index < 0)
		return index;

	if (atomic_read(&get_slot(index)->descriptor.is_valid) < 0)
		value = get_slot(index)->descriptor.rid;
	else {
		if (atomic_read(&get_slot(index)->descriptor_old.is_valid) >= 0)
			return -EAGAIN;
		value = get_slot(index)->descriptor_old.rid;
	}

	char_cnt = snprintf(buf, 10 + 1, "%u", value);

	return char_cnt + 1;
}
