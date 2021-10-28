#ifndef SDBP_H_
#define SDBP_H_

#include <linux/irq.h>

int sdbp_main(void *data);
irqreturn_t gpio_rising_interrupt(int irq, void *dev_id);
struct Slot *get_slot(int index);
int find_slot(dev_t devt);
void free_slots(void);

#define BUS_0_CS_0_INT 0,0,34,8
#define BUS_0_CS_1_INT 0,1,35,7
#define BUS_1_CS_0_INT 1,0,36,18
#define BUS_1_CS_1_INT 1,1,37,17
#define BUS_1_CS_2_INT 1,2,38,16
#define BUS_2_CS_0_INT 2,0,39,43
#define BUS_2_CS_1_INT 2,1,30,44
#define BUS_2_CS_2_INT 2,2,31,45

#define MINOR_DEVICES 8

#define DRIVER_VERSION "1.1.2"

#endif
