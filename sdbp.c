#include <linux/init.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/semaphore.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/spi/spi.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include "sdbp.h"
#include "descriptor.h"
#include "communication.h"
#include "attributes.h"
#include "debug.h"

static struct Slot *slot_list[MINOR_DEVICES];
static dev_t major_device_number;
static struct cdev *driver_object;
static struct class *sdbp_class;

static bool spi_bus[3];
static int bus_cnt = 0;
module_param_array(spi_bus, bool, &bus_cnt, S_IRUGO);
MODULE_PARM_DESC(spi_bus, " Selective SPI bus configuration, false means bus is not used. (bus=1,1,1)");

void print_struct(struct Slot *slot)
{
	PRINT_DBG("number : %d\n", slot->number);
	PRINT_DBG("valid : %d\n", slot->valid);
	PRINT_DBG("speed_sclk : %d\n", slot->speed_sclk);
	PRINT_DBG("frame_size : %d\n", slot->frame_size);
	PRINT_DBG("crc_size : %d\n", slot->crc_size);
	PRINT_DBG("spi_bus : %d\n", slot->spi_bus);
	PRINT_DBG("spi_chip_select : %d\n", slot->spi_chip_select);
	PRINT_DBG("interrupt_pin : %d\n", slot->interrupt_pin);
	PRINT_DBG("irq_number : %d\n", slot->irq_number);
	PRINT_DBG("interrupt_arrived : %d\n", atomic_read(&slot->interrupt_arrived));
}

static struct bus_type sdbp_bus = {
	.name = "sdbp",
};

static struct device_driver sdbp_driver = {
	.name = "sdbpk",
	.bus = &sdbp_bus,
	.owner = THIS_MODULE,
};

static int driver_open(struct inode *device_file, struct file *instance)
{
	u8 i;
	int minor_number = iminor(file_dentry(instance)->d_inode);

	PRINT_DBG("Driver open called!\n");
	for (i = 0; i < MINOR_DEVICES; i++) {
		if (slot_list[i] != NULL) {
			if (slot_list[i]->valid && slot_list[i]->number == minor_number) {
				if (atomic_inc_and_test(&slot_list[i]->access_count)) {
					PRINT_DBG("Driver open done ok!\n");
					return 0;
				}
				PRINT_DBG("Driver open done EBUSY!\n");
				atomic_dec(&slot_list[i]->access_count);
				return -EBUSY;
			}
		}
	}

	PRINT_DBG("Driver open EBADSLT called!\n");
	return -EBADSLT;
}

static int driver_close(struct inode *device_file, struct file *instance)
{
	u8 i, ret;
	int minor_number = iminor(file_dentry(instance)->d_inode);
	u8 *rx_buffer;
	u8 cnt = 0;
	PRINT_DBG("Driver close called!\n");

	for (i = 0; i < MINOR_DEVICES; i++) {
		if (slot_list[i] != NULL) {
			if (slot_list[i]->valid && slot_list[i]->number == minor_number) {
				PRINT_SLOT_DBG("Driver close slot found\n", slot_list[i]->number);
				rx_buffer = kcalloc(MAXIMUM_FRAME_SIZE, sizeof(u8), GFP_KERNEL);
				if (!atomic_inc_and_test(&slot_list[i]->write_count)) {
					PRINT_SLOT_DBG("Reset delayed (bus busy)!\n", slot_list[i]->number);
					ret =
					    wait_event_interruptible_timeout(slot_list[i]->wait_queue_for_write, atomic_read(&slot_list[i]->write_count) == 0,
									     msecs_to_jiffies(200));

					if (ret == 0) {
						PRINT_SLOT_DBG("Reset timeout (bus busy)!\n", slot_list[i]->number);
					} else if (ret == -ERESTARTSYS) {
						PRINT_SLOT_DBG("Reset timeout interrupted by system!\n", slot_list[i]->number);
					} else {
						PRINT_SLOT_DBG("Reset continued. (cnt: %d)\n", slot_list[i]->number, atomic_read(&slot_list[i]->write_count));
					}
				}
				slot_list[i]->speed_sclk = DEFAULT_SCLK_SPEED;

				if (slot_list[i]->frame_size != DEFAULT_FRAME_SIZE) {
					if (exchange_sdbp(slot_list[i], (u8 *)
							  CONTROL_SET_FRAME_SIZE_DEFAULT, rx_buffer, LOG_LVL_NORMAL))
						PRINT_SLOT_ERR("Failed resetting frame size after FD close!", slot_list[i]->number);
					slot_list[i]->frame_size = DEFAULT_FRAME_SIZE;
				}

				if (exchange_sdbp(slot_list[i], (u8 *) CONTROL_SET_MODE_SUSPEND, rx_buffer, LOG_LVL_NORMAL) != 0) {
					PRINT_SLOT_ERR("Failed setting device into SUSPEND mode after FD close!", slot_list[i]->number);

					for (cnt = 0; cnt < 5; cnt++) {
						if (gpio_get_value(slot_list[i]->interrupt_pin)) {
							PRINT_SLOT_DBG("Stopped disconnected detection after %d tries.\n", slot_list[i]->number, cnt);
							break;	// Stop debounce phase
						}
						usleep_range(500, 1000);
					}
				}

				if (exchange_sdbp(slot_list[i], (u8 *)
						  CONTROL_UPDATE_DESCRIPTOR, rx_buffer, LOG_LVL_NORMAL))
					PRINT_SLOT_ERR("Failed updateing after FD close!", slot_list[i]->number);

				atomic_dec(&slot_list[i]->write_count);
				wake_up_all(&slot_list[i]->wait_queue_for_write);
				if (cnt >= 4) {
					// If interrupt line stays low after failure we have a disconnect.
					atomic_set(&slot_list[i]->notification_arrived, 1);
					wake_up_all(&slot_list[i]->queue);
					PRINT_SLOT_DBG("Device disconnected on driver close! \n", slot_list[i]->number);
				}
				kfree(rx_buffer);
				atomic_dec(&slot_list[i]->access_count);
				PRINT_SLOT_DBG("Driver close ok!\n", slot_list[i]->number);
				return 0;
			}
		}
	}
	PRINT_DBG("Driver close with EBADSLT!\n");
	return -EBADSLT;
}

static ssize_t driver_read(struct file *instance, char __user * user, size_t max_bytes_to_read, loff_t * offset)
{
	unsigned long not_copied, to_copy;
	int minor_number = iminor(file_dentry(instance)->d_inode);
	u8 i;

	for (i = 0; i < MINOR_DEVICES; i++) {
		if (slot_list[i] != NULL) {
			if (slot_list[i]->valid && slot_list[i]->number == minor_number) {

				if (instance->f_flags & O_NONBLOCK || slot_list[i]->rx_len == 0)
					return -EWOULDBLOCK;

				slot_list[i]->rx_len = (slot_list[i]->rx_buffer[1] << 8) | slot_list[i]->rx_buffer[2];

				if (max_bytes_to_read < (slot_list[i]->rx_len))
					return -EMSGSIZE;

				to_copy = slot_list[i]->rx_len - 4;
				not_copied = copy_to_user(user, slot_list[i]->rx_buffer + 4, to_copy);

				slot_list[i]->rx_len = not_copied;
				return to_copy - not_copied;
			}
		}
	}

	PRINT_DBG("Minor is: %d", minor_number);
	return -EBADSLT;
}

ssize_t driver_write(struct file * instance, const char __user * buffer, size_t max_bytes_to_write, loff_t * offset)
{
	size_t to_copy, not_copied;
	int minor_number = iminor(file_dentry(instance)->d_inode);
	u8 i;
	int ret;

	for (i = 0; i < MINOR_DEVICES; i++) {
		if (slot_list[i] != NULL) {
			if (slot_list[i]->valid && slot_list[i]->number == minor_number) {

				if (atomic_read(&slot_list[i]->write_count) != -1) {
					wake_up_all(&slot_list[i]->wait_queue_for_write);
					PRINT_SLOT_DBG("Write delayed (bus busy %d)!\n", slot_list[i]->number, atomic_read(&slot_list[i]->write_count));
					wait_event_interruptible_timeout(slot_list[i]->wait_queue_for_write, atomic_read(&slot_list[i]->write_count) == -1,
									 msecs_to_jiffies(200));
					PRINT_SLOT_DBG("Write proceeds.\n", slot_list[i]->number);
				}

				if (!atomic_inc_and_test(&slot_list[i]->write_count)) {
					PRINT_SLOT_DBG("Write delayed again (bus busy %d)!\n", slot_list[i]->number, atomic_read(&slot_list[i]->write_count));
					ret =
					    wait_event_interruptible_timeout(slot_list[i]->wait_queue_for_write, atomic_read(&slot_list[i]->write_count) == 0,
									     msecs_to_jiffies(200));

					if (ret == 0) {
						PRINT_SLOT_ERR("Write timeout (bus busy)!\n", slot_list[i]->number);
						atomic_dec(&slot_list[i]->write_count);
						wake_up_all(&slot_list[i]->wait_queue_for_write);
						return -EWOULDBLOCK;
					} else if (ret == -ERESTARTSYS) {
						PRINT_SLOT_DBG("Write interrupted by system!\n", slot_list[i]->number);
						atomic_dec(&slot_list[i]->write_count);
						wake_up_all(&slot_list[i]->wait_queue_for_write);
						return -ERESTARTSYS;
					} else {
						PRINT_SLOT_DBG("Write continued. (cnt: %d)\n", slot_list[i]->number, atomic_read(&slot_list[i]->write_count));
					}
				}

				if (instance->f_flags & O_NONBLOCK) {
					PRINT_SLOT_DBG("Blocking call not possible!\n", slot_list[i]->number);
					atomic_dec(&slot_list[i]->write_count);
					wake_up_all(&slot_list[i]->wait_queue_for_write);
					return -EWOULDBLOCK;
				}

				if ((max_bytes_to_write > (slot_list[i]->frame_size - 6))
				    || (max_bytes_to_write > MAXIMUM_FRAME_SIZE)) {
					atomic_dec(&slot_list[i]->write_count);
					wake_up_all(&slot_list[i]->wait_queue_for_write);
					return -EMSGSIZE;
				}

				to_copy = min((size_t) slot_list[i]->frame_size, max_bytes_to_write);
				max_bytes_to_write += 4;
				slot_list[i]->tx_buffer[0] = SDBP_MSG_TYPE_OPERATION;
				slot_list[i]->tx_buffer[1] = (max_bytes_to_write >> 8) & 0x00ff;
				slot_list[i]->tx_buffer[2] = max_bytes_to_write & 0x00ff;
				slot_list[i]->tx_buffer[3] = SDBP_OPTION_BYTE;

				not_copied = copy_from_user(slot_list[i]->tx_buffer + 4, buffer, to_copy);

				ret = 0;
				if (exchange_sdbp(slot_list[i], slot_list[i]->tx_buffer, slot_list[i]->rx_buffer, LOG_LVL_NORMAL) != 0) {
					atomic_dec(&slot_list[i]->write_count);
					wake_up_all(&slot_list[i]->wait_queue_for_write);
					PRINT_SLOT_DBG("Data exchange failed!", slot_list[i]->number);
					ret = -ECOMM;
				}

				if (!gpio_get_value(slot_list[i]->interrupt_pin)) {
					usleep_range(500, 1000);
					if (!gpio_get_value(slot_list[i]->interrupt_pin)) {
						atomic_set(&slot_list[i]->notification_arrived, 1);
						wake_up_all(&slot_list[i]->queue);
						PRINT_SLOT_DBG("Device disconnected after write!\n", slot_list[i]->number);
					}
				}

				if (ret != 0) {
					return ret;	// Return after disconnect check
				}

				slot_list[i]->rx_len = slot_list[i]->frame_size;

				atomic_dec(&slot_list[i]->write_count);
				wake_up_all(&slot_list[i]->wait_queue_for_write);

				if (slot_list[i]->rx_buffer[3] == SDBP_OPTION_BYTE_NOTIFICATION_PENDING) {
					PRINT_SLOT_DBG("Notification pending.", slot_list[i]->number);
					atomic_set(&slot_list[i]->notification_arrived, 1);
					wake_up_all(&slot_list[i]->queue);
				}
				return to_copy;
			}
		}
	}

	PRINT_DBG("Minor is: %d", minor_number);
	return -EBADSLT;
}

struct Slot *init_slot_struct(u8 slot_number, u8 spi_bus, u8 spi_cs, u8 int_pin, u8 cs_pin_alt)
{
	struct Slot *slot = kzalloc(sizeof(struct Slot),
				    GFP_KERNEL);

	slot->number = slot_number;
	slot->spi_bus = spi_bus;
	slot->spi_chip_select = spi_cs;
	slot->interrupt_pin = int_pin;
	slot->cs_pin_alt = cs_pin_alt;
	atomic_set(&slot->interrupt_arrived, 0);
	atomic_set(&slot->notification_arrived, 0);
	atomic_set(&slot->access_count, -1);
	atomic_set(&slot->write_count, -1);
	atomic_set(&slot->descriptor.is_valid, -1);
	atomic_set(&slot->descriptor_old.is_valid, 0);
	atomic_set(&slot->stop, 0);
	slot->speed_sclk = DEFAULT_SCLK_SPEED;
	slot->crc_size = DEFAULT_CRC_SIZE;
	slot->frame_size = DEFAULT_FRAME_SIZE;
	slot->spi_device = NULL;
	slot->tx_buffer = kcalloc(MAXIMUM_FRAME_SIZE, sizeof(u8), GFP_KERNEL);
	slot->rx_buffer = kcalloc(MAXIMUM_FRAME_SIZE, sizeof(u8), GFP_KERNEL);
	init_waitqueue_head(&slot->wait_queue_for_read);
	init_waitqueue_head(&slot->wait_queue_for_write);
	init_waitqueue_head(&slot->notification.wait_for_notification);
	init_completion(&slot->dev_obj_is_free);
	slot->session_stats.transmission_errors = 0;
	slot->session_stats.notifications = 0;
	slot->session_stats.notifications_failed = 0;
	slot->session_stats.descriptor_failed = 0;
	return slot;
}

struct Slot *get_slot(int index)
{
	return slot_list[index];
}

int find_slot(dev_t devt)
{
	u8 i;

	for (i = 0; i < MINOR_DEVICES; i++) {
		if (slot_list[i] != NULL) {
			if (slot_list[i]->valid) {
				if (slot_list[i]->sdbp_device != NULL) {
					if (slot_list[i]->sdbp_device->devt == devt) {
						return i;
					}
				}
			}
		}
	}
	return -1;
}

static DEVICE_ATTR(vendor_product_id, S_IRUGO, get_vendor_product_id, NULL);
static DEVICE_ATTR(vendor_name, S_IRUGO, get_vendor_name, NULL);
static DEVICE_ATTR(product_name, S_IRUGO, get_product_name, NULL);
static DEVICE_ATTR(max_power_3v3, S_IRUGO, get_max_power_3v3, NULL);
static DEVICE_ATTR(max_power_5v0, S_IRUGO, get_max_power_5v0, NULL);
static DEVICE_ATTR(max_power_12v, S_IRUGO, get_max_power_12v, NULL);
static DEVICE_ATTR(max_sclk_speed, S_IRUGO, get_max_sclk_speed, NULL);
static DEVICE_ATTR(max_frame_size, S_IRUGO, get_max_frame_size, NULL);
static DEVICE_ATTR(bootloader_state, S_IRUGO, get_bootloader_state, NULL);
static DEVICE_ATTR(fw_version, S_IRUGO, get_fw_version, NULL);
static DEVICE_ATTR(hw_version, S_IRUGO, get_hw_version, NULL);
static DEVICE_ATTR(protocol_version, S_IRUGO, get_protocol_version, NULL);
static DEVICE_ATTR(serial_code, S_IRUGO, get_serial_code, NULL);
static DEVICE_ATTR(notification, S_IRUGO, get_notification_data, NULL);
static DEVICE_ATTR(stats_failed_transmissions, S_IRUGO, get_stats_failed_transmissions, NULL);
static DEVICE_ATTR(stats_notifications, S_IRUGO, get_stats_notifications, NULL);
static DEVICE_ATTR(stats_failed_notifications, S_IRUGO, get_stats_failed_notifications, NULL);
static DEVICE_ATTR(stats_failed_descriptors, S_IRUGO, get_stats_failed_descriptors, NULL);
static DEVICE_ATTR(rid, S_IRUGO, get_rid, NULL);

static struct attribute *dev_attrs[] = {
	&dev_attr_vendor_name.attr,
	&dev_attr_vendor_product_id.attr,
	&dev_attr_product_name.attr,
	&dev_attr_max_power_3v3.attr,
	&dev_attr_max_power_5v0.attr,
	&dev_attr_max_power_12v.attr,
	&dev_attr_max_frame_size.attr,
	&dev_attr_max_sclk_speed.attr,
	&dev_attr_bootloader_state.attr,
	&dev_attr_fw_version.attr,
	&dev_attr_hw_version.attr,
	&dev_attr_protocol_version.attr,
	&dev_attr_serial_code.attr,
	&dev_attr_notification.attr,
	&dev_attr_stats_failed_transmissions.attr,
	&dev_attr_stats_notifications.attr,
	&dev_attr_stats_failed_notifications.attr,
	&dev_attr_stats_failed_descriptors.attr,
	&dev_attr_rid.attr,
	NULL,
};

static struct attribute_group dev_attr_group = {
	.attrs = dev_attrs,
};

static const struct attribute_group *dev_attr_groups[] = {
	&dev_attr_group,
	NULL,
};

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.release = driver_close,
	.open = driver_open,
	.read = driver_read,
	.write = driver_write,
};

static void driver_release(struct device *dev)
{
	int index;
	index = find_slot(dev->devt);
	if (index < 0) {
		return;
	}
	complete(&get_slot(index)->dev_obj_is_free);
}

irqreturn_t gpio_rising_interrupt(int irq, void *dev_id)
{
	u8 i;

	for (i = 0; i < MINOR_DEVICES; i++) {
		if (slot_list[i] != NULL) {
			if (slot_list[i]->valid && slot_list[i]->irq_number == irq) {
				if (atomic_read(&slot_list[i]->write_count) == -1) {
					atomic_set(&slot_list[i]->notification_arrived, 1);
				}
				atomic_set(&slot_list[i]->interrupt_arrived, 1);
				wake_up_all(&slot_list[i]->queue);
			}
		}
	}

	return (IRQ_HANDLED);
}

void free_slots(void)
{
	int i, ret;
	for (i = 0; i < MINOR_DEVICES; i++) {
		if (slot_list[i] != NULL) {
			ret = kthread_stop(slot_list[i]->thread);
			if (!ret || -EINTR)
				PRINT_DBG("Thread stopped.");
			if (slot_list[i]->valid) {
				atomic_set(&slot_list[i]->stop, 1);
				wake_up_all(&slot_list[i]->queue);
				if (slot_list[i]->spi_device != NULL) {
					//PRINT_DBG("Unloaded spi.");
					spi_unregister_device(slot_list[i]->spi_device);
				}
				free_irq(slot_list[i]->irq_number, NULL);
				//PRINT_DBG("Unloaded irq.");
				gpio_unexport(slot_list[i]->interrupt_pin);
				//PRINT_DBG("Unloaded gpio.");
				gpio_free(slot_list[i]->interrupt_pin);
				//PRINT_DBG("Free gpio.");
				wait_for_completion(&slot_list[i]->dev_obj_is_free);
				PRINT_SLOT_DBG("Released slot %d.", slot_list[i]->number, slot_list[i]->number);
			}
		}
	}
}

static int __init sdbp_init(void)
{
	u8 i, j;
	PRINT_NORM("Registering sdbp driver v%s...\n", DRIVER_VERSION);

	slot_list[0] = init_slot_struct(0, BUS_0_CS_0_INT);
	slot_list[1] = init_slot_struct(1, BUS_0_CS_1_INT);
	slot_list[2] = init_slot_struct(2, BUS_1_CS_0_INT);
	slot_list[3] = init_slot_struct(3, BUS_1_CS_1_INT);
	slot_list[4] = init_slot_struct(4, BUS_1_CS_2_INT);
	slot_list[5] = init_slot_struct(5, BUS_2_CS_0_INT);
	slot_list[6] = init_slot_struct(6, BUS_2_CS_1_INT);
	slot_list[7] = init_slot_struct(7, BUS_2_CS_2_INT);

	PRINT_NORM("Debug level NORMAL enabled.\n");
	PRINT_DBG("Debug level DEBUG enabled.\n");
	PRINT_ERR("Debug level ERROR enabled.\n");

	if (bus_cnt == 0) {
		PRINT_NORM("All SPI buses used.\n");
	} else if (bus_cnt == 3) {
		for (i = 0; i < bus_cnt; i++) {
			if (!spi_bus[i]) {
				for (j = 0; j < MINOR_DEVICES; j++) {
					if (slot_list[j] != NULL) {
						if (slot_list[j]->spi_bus == i) {
							PRINT_NORM("Slot %d disabled.\n", slot_list[j]->number);
							kfree(slot_list[j]);
							slot_list[j] = NULL;
						}
					}
				}
				PRINT_NORM("Bus %d unused.", i);
			}
		}
	} else {
		PRINT_ERR("Parameter must have three fields! (e.g: spi_bus=1,1,1)\n");
		return -EINVAL;
	}

	if (bus_register(&sdbp_bus) != 0) {
		PRINT_ERR("Failed to register sdbp bus...\n");
		return -EAGAIN;
	}

	if (driver_register(&sdbp_driver) != 0) {
		PRINT_ERR("Failed to register sdbp driver...\n");
		bus_unregister(&sdbp_bus);
		return -EAGAIN;
	}

	if (alloc_chrdev_region(&major_device_number, 0, MINOR_DEVICES, "sdbp_class")
	    < 0)
		goto free_bus_and_slots;
	driver_object = cdev_alloc();
	if (driver_object == NULL)
		goto free_device_number;

	driver_object->owner = THIS_MODULE;
	driver_object->ops = &fops;

	if (cdev_add(driver_object, major_device_number, MINOR_DEVICES))
		goto free_cdev;

	sdbp_class = class_create(THIS_MODULE, "sdbp");
	if (IS_ERR(sdbp_class)) {
		PRINT_ERR("No udev support!\n");
		goto free_cdev;
	}

	for (i = 0; i < MINOR_DEVICES; i++) {
		if (slot_list[i] != NULL) {
			slot_list[i]->thread = kthread_create(sdbp_main, slot_list[i], "sdbp-thread");
			if (slot_list[i]->thread) {
				wake_up_process(slot_list[i]->thread);
			} else {
				PRINT_ERR("Thread creation failed!\n");
				goto free_cdev;
			}
			//usleep_range(1000000, 1500000);       // Wait to get structured descriptor logging
		} else {
			PRINT_DBG("Index %d disabled.", i);
		}
	}
	PRINT_NORM("Registered sdbp driver.\n");
	return 0;

 free_cdev:
	kobject_put(&driver_object->kobj);
	goto free_device_number;

 free_device_number:
	unregister_chrdev_region(major_device_number, MINOR_DEVICES);
	goto free_bus_and_slots;

 free_bus_and_slots:
	free_slots();
	driver_unregister(&sdbp_driver);
	bus_unregister(&sdbp_bus);
	return -EAGAIN;
}

int sdbp_main(void *data)
{
	struct Slot *slot = (struct Slot *)data;
	u8 exit;
	u8 was_connected;
	int ret;
	u8 tx_err_cnt;
	u8 input;
	int not_check;

	enum {
		disconnected, initiating, connected, failed
	} state;

	was_connected = 0;
	exit = 0;
	tx_err_cnt = 0;
	input = 0;

	state = disconnected;

	if (init_slot(slot) != 0) {
		PRINT_SLOT_ERR("Slot init failed!\n", slot->number);
		state = failed;
	}

	while (!kthread_should_stop()) {
		switch (state) {
		case disconnected:
			{
				slot->session_stats.transmission_errors = 0;
				slot->session_stats.notifications = 0;
				slot->session_stats.notifications_failed = 0;
				slot->session_stats.descriptor_failed = 0;
				input = gpio_get_value(slot->interrupt_pin);
				if (input) {
					u8 cnt = 0;
					for (cnt = 0; cnt < 5; cnt++) {
						if (!gpio_get_value(slot->interrupt_pin)) {
							PRINT_SLOT_DBG("Stopped debounce phase after %d tries.\n", slot->number, cnt);
							break;	// Stop debounce phase
						}

						msleep_interruptible(100);
					}
					if (cnt < 4)
						break;
					PRINT_SLOT_DBG("Debounce successful after %d tries.\n", slot->number, cnt);
					state = initiating;
				} else
					msleep_interruptible(100);
			}
			break;
		case initiating:
			{
				PRINT_SLOT_DBG("Reached state initiating.\n", slot->number);
				slot->frame_size = DEFAULT_FRAME_SIZE;
				slot->speed_sclk = DEFAULT_SCLK_SPEED;
				atomic_set(&slot->notification.length, 0);
				atomic_set(&slot->notification.lock, -1);
				atomic_set(&slot->notification_arrived, 0);
				atomic_set(&slot->interrupt_arrived, 0);
				slot->session_stats.transmission_errors = 0;
				slot->session_stats.notifications = 0;
				slot->session_stats.notifications_failed = 0;
				if (get_descriptor(slot, &slot->descriptor, 0, 0)
				    != 0) {
					sync_com(slot);

					if (tx_err_cnt < 60) {
						tx_err_cnt++;
						PRINT_SLOT_DBG("Increasing sleep time. %d\n", slot->number, tx_err_cnt);
					}

					msleep_interruptible(1000 * tx_err_cnt);
					state = disconnected;
				} else {
					tx_err_cnt = 0;
					was_connected = 1;

					{	// Register device
						slot->sdbp_device = kzalloc(sizeof(struct device), GFP_KERNEL);
						if (!slot->sdbp_device) {
							PRINT_SLOT_ERR("Could not allocate memory!\n", slot->number);
							state = failed;
							break;
						}

						slot->sdbp_device->class = sdbp_class;
						slot->sdbp_device->parent = NULL;
						slot->sdbp_device->devt = major_device_number + slot->number;
						dev_set_drvdata(slot->sdbp_device, NULL);
						dev_set_name(slot->sdbp_device, "slot%d", slot->number);
						slot->sdbp_device->release = driver_release;
						slot->sdbp_device->groups = dev_attr_groups;
						slot->sdbp_device->driver = &sdbp_driver;
						// slot->sdbp_device->bus =
						//     &sdbp_bus;

						ret = device_register(slot->sdbp_device);
						if (ret) {
							PRINT_SLOT_ERR("Device registration failed!\n", slot->number);
							put_device(slot->sdbp_device);
							kfree(slot->sdbp_device);
							state = failed;
							break;
						}
					}

					{	// Bind device to driver
						mutex_lock(&slot->sdbp_device->mutex);
						if (device_bind_driver(slot->sdbp_device)) {
							mutex_unlock(&slot->sdbp_device->mutex);
							PRINT_SLOT_ERR("Binding device to driver failed!\n", slot->number);
							device_destroy(sdbp_class, major_device_number + slot->number);
							kobject_put(&driver_object->kobj);
							state = failed;
							break;
						}
						mutex_unlock(&slot->sdbp_device->mutex);
					}

					PRINT_SLOT_DBG("Reached state connected.\n", slot->number);
					state = connected;
				}
			}
			break;
		case connected:
			{

				ret = wait_event_interruptible(slot->queue, atomic_read(&slot->notification_arrived)
							       > 0 || atomic_read(&slot->stop));

				if (ret == -ERESTARTSYS) {
					PRINT_SLOT_DBG("Interrupted by system!\n", slot->number);
				}

				if (atomic_read(&slot->stop)) {
					//exit = 1;
					break;
				}

				if (!atomic_inc_and_test(&slot->write_count)) {
					PRINT_SLOT_DBG("Device is used %d.\n", slot->number, atomic_read(&slot->write_count));
					ret = wait_event_interruptible(slot->wait_queue_for_write, atomic_read(&slot->write_count) == 0
								       || atomic_read(&slot->stop));

					if (ret == ERESTARTSYS) {
						PRINT_SLOT_DBG("Notification interrupted by system!\n", slot->number);
						atomic_dec(&slot->write_count);
						wake_up_all(&slot->wait_queue_for_write);
						break;
					}
					PRINT_SLOT_DBG("Device freed.\n", slot->number);
				}
				not_check = get_notification(slot);
				if (not_check != 0) {
					input = gpio_get_value(slot->interrupt_pin);
					if (input == 0) {

						// Trigger blocking attribute
						atomic_set(&slot->notification.length, -1);
						wake_up_all(&slot->notification.wait_for_notification);

						PRINT_SLOT_NORM("Device disconnected.\n", slot->number);
						device_release_driver(slot->sdbp_device);
						device_destroy(sdbp_class, major_device_number + slot->number);
						was_connected = 0;
						state = disconnected;
						msleep_interruptible(100);
					} else {
						if (not_check == -2)
							PRINT_SLOT_DBG("Notification queue is full.\n", slot->number);
						else
							PRINT_SLOT_ERR("Notification exchange failed.\n", slot->number);
					}
				} else {
					PRINT_SLOT_DBG("Notification exchange successful.\n", slot->number);
				}
				atomic_dec(&slot->write_count);
				wake_up_all(&slot->wait_queue_for_write);
				atomic_set(&slot->notification_arrived, 0);
			}
			break;
		case failed:
			{
				PRINT_SLOT_ERR("Slot not used because of an error!\n", slot->number);
				while (!kthread_should_stop()) {
					msleep_interruptible(100);
				}
			}
			break;
		default:
			PRINT_SLOT_ERR("Wrong enum state!\n", slot->number);
		}
		if (exit) {
			while (!kthread_should_stop()) {
				msleep_interruptible(100);
			}
		}

	}

	if (was_connected) {
		device_release_driver(slot->sdbp_device);
		device_destroy(sdbp_class, major_device_number + slot->number);
	} else {
		complete(&slot->dev_obj_is_free);
	}
	kfree(slot->tx_buffer);
	kfree(slot->rx_buffer);
	return 0;
}

static void __exit sdbp_exit(void)
{

	PRINT_NORM("Driver unloading.\n");

	free_slots();

	class_destroy(sdbp_class);
	cdev_del(driver_object);
	unregister_chrdev_region(major_device_number, MINOR_DEVICES);
	driver_unregister(&sdbp_driver);
	bus_unregister(&sdbp_bus);
	PRINT_NORM("Driver unloaded.\n");
}

late_initcall(sdbp_init);
module_exit(sdbp_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Richard Schleich <rs@noreya.tech>");
MODULE_DESCRIPTION("Serial Device Bus Protocol (SDBP) driver");
MODULE_SOFTDEP("post: spi_bcm2835aux");
MODULE_SOFTDEP("post: spi_bcm2835");
MODULE_VERSION(DRIVER_VERSION);
