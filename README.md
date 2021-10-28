# Serial Device Bus Protocol (SDBP) Kernel Driver
[![Build](https://github.com/nexus-unity/kernel-driver-sdbpk/actions/workflows/build.yml/badge.svg)](https://github.com/nexus-unity/kernel-driver-sdbpk/actions/workflows/build.yml)

This kernel driver implements low-level support for the SDBP protocol.  
It basically uses the SPI and GPIO interfaces to implement a new bus.  

Basic features are:
- Auto insert/removal detection
- Descriptor is mapped to /sys/class/sdbp/slotX/ (like USB)
- Character device file is /dev/slotX
- Header and crc are automatically added to the payload
- Support for notifications (device initiated communication)

It is part of the [nexus-unity.com](https://nexus-unity.com) project.  

## User space API

### Driver presence check
To check if the driver is loaded and active the following checks can be done:

driver/module version:  
```
/sys/module/sdbpk/version
```
The sysfs directory is available when driver is ready and waiting for devices.  
```
/sys/class/sdbp/
```

### Device detection
If a device is connected the driver creates a "slot" directory under /sys/class/sdbp/.  
e.g: */sys/class/sdbp/slot0*  

The slot[1-9] directory contains the device descriptor information,
which can be used to select the device.  
e.g: */sys/class/sdbp/slot0/vendor_product_id*  

The available attributes are:  
```
bootloader_state  
fw_version  
hw_version  
max_frame_size  
max_power_12v  
max_power_3v3  
max_power_5v0  
max_sclk_speed  
notification  
product_name  
protocol_version  
serial_code  
vendor_name  
vendor_product_id
```
connection attributes:
```
stats_failed_descriptors (number of failed descriptor checks)  
stats_failed_notifications (number of failed notifications)  
stats_failed_transmissions (number of failed transmissions)  
stats_notifications (number of notifications handled)  
rid (random descriptor id)  
```

Except the "notification" sysfs attribute, all of them share the following attributes:  
- Read-only  
- Non-blocking  
- ASCII encoded  
- Data ends with trailing null termination.  
- Can be used with standard system calls (open/read/close).  
- Number of bytes read are returned.  

For data interpretation see the SDBP specification.

### Data exchange
The driver creates a character device file under /dev e.g: /dev/slot0.  
The user space application can open this file and read/write to it.  
Very basic examples can be found under [examples/](examples/).  

The following rules apply:  

#### General
- Exclusive access, only one open file handle per time (-EBUSY).  
- Access must be done by system class open/read/write/close without buffering.
- When the file handle is closed the driver:
	- Resets the frame size by command to default.
	- Resets the SCLK speed to default.
	- Sets the device into SUSPEND mode by command.

#### Write:  
- The data should be a valid SDBP frame starting with the class identifier.  
- Header and CRC are added by the driver.  
- The maximum amount of data to write is limited by the frame size set minus the header (-EMSGSIZE).  
  - For the default frame size 64 bytes: 64-6 = 58 bytes maxium data to write.  
- Blocking access only (-EWOULDBLOCK).  
- In case of an exchange error -ECOMM is returned.  
- The SDBP Control class commands SET_FRAME_SIZE, SET_SCLK_SPEED and UPDATE_DESCRIPTOR are transparently handled.  
- A write to the file returns the number of written bytes.  
- The whole SDBP exchange is done when the write returns.  
- The Maximum frame size is 4096 bytes.   

#### Read:  
- Returns -EWOULDBLOCK if no data is available (no previous write).  
- The minimum read buffer size must be the number of bytes (payload) received (-EMSGSIZE).
  - It is recommended to use the current frame size setting as buffer size.  

#### Further recommendations:  
- The open/close cycles should be minimized to improve performance.  
- **Most programming languages use read/write buffers by default -> they must be disabled!**

### Notification handling
The user space application **must listen** to the "notification" attribute.  
e.g: */sys/class/sdbp/slot0/notification*

The following rules apply:  
- Reading from the file will block until a SDBP notification is available.  
- Only the last notification will be returned (older are discarded).  
- The read buffer must be at least 4096 bytes.  
- The payload returned is an ASCII encoded hex string (0x12AB..) with null termination.  
- File is read-only.  
- Lock protected, only one handle can be opened at the same time.  
- Returns -ENODEV if slot is disconnected.

## Debugging
To use this feature the kernel must have dynamic debug support.  
To enable debugging output:  
```
echo -n 'module sdbpk +p' > /sys/kernel/debug/dynamic_debug/control
```
To disable debugging output:  
```
echo -n 'module sdbpk -p' > /sys/kernel/debug/dynamic_debug/control
```

## Important notes
- The *spidev* kernel driver conflicts with this driver.  
  It can be disabled using the device tree system (dtoverlay=spi1-3cs,cs0_spidev=disabled,cs1_spidev=disabled,cs2_spidev=disabled).
	There is an example in the [overlay](overlay/) directory. Alternatively, the kernel must be build without *spidev* support.
- To enable the maximum SCLK speed on the Raspberry Pi Compute Module 3 the gpio current setting must be set to the highest setting (16mA).  
  - The kernel does not support this function so it must be done by patching the dt-blob.bin/dts file.  
  - The current setting can be changed from the userspace using a Raspberry Pi gpio tool but this must be done before the driver services are started!

## Things that need to be done before going upstream
Overall, the driver is in good condition but there are a few things that should be changed before merging it to the mainline kernel (from the authors point of view).

- Currently, the SPI/GPIO allocation is hardcoded.  
  It should be replaced by a device tree based implementation.  
- Driver contains a crc16ccitt implementation (from CANopenNode, GPLv2).
  The kernel has a lib for this which should be used.  
	However, using the kernel implementation prevents compilation outside the kernel source tree.
