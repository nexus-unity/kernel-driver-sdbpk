#ifndef DEBUG_H_
#define DEBUG_H_

#define DRIVER_PREFIX "sdbpk: "

#define PRINT_NORM(fmt, args...) printk(DRIVER_PREFIX fmt, ## args)

#define PRINT_DBG(fmt, args...) pr_debug(DRIVER_PREFIX fmt, ## args)

#define PRINT_ERR(fmt, args...) printk(KERN_ERR DRIVER_PREFIX fmt, ## args)

#define PRINT_SLOT_NORM(fmt, args...) printk(DRIVER_PREFIX "slot %d: " fmt, ## args)

#define PRINT_SLOT_DBG(fmt, args...) pr_debug(DRIVER_PREFIX "slot %d: " fmt, ##args)

#define PRINT_SLOT_ERR(fmt, args...) printk(KERN_ERR DRIVER_PREFIX "slot %d: " fmt, ## args)

#endif
