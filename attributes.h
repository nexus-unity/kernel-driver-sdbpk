#ifndef ATTRIBUTES_H_
#define ATTRIBUTES_H_

#include <linux/module.h>
#include <linux/device.h>

ssize_t get_vendor_product_id(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t get_vendor_name(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t get_product_name(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t get_max_power_3v3(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t get_max_power_5v0(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t get_max_power_12v(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t get_max_sclk_speed(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t get_max_frame_size(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t get_bootloader_state(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t get_fw_version(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t get_hw_version(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t get_protocol_version(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t get_serial_code(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t get_notification_data(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t get_stats_failed_transmissions(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t get_stats_notifications(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t get_stats_failed_notifications(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t get_stats_failed_descriptors(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t get_rid(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t trigger_notification(struct device *dev, struct device_attribute *attr, char *buf);

#endif
