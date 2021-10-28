#ifndef COMMUNICATION_H_
#define COMMUNICATION_H_

#include "sdbp.h"

struct ErrorStatistics {
	u32 transmission_errors;
	u32 notifications;
	u32 notifications_failed;
	u32 descriptor_failed;
};

int exchange_sdbp(struct Slot *slot, u8 * data, u8 * rx_buffer, u8 log_lvl);
int receive_notification(struct Slot *slot, u8 * data, u8 * rx_buffer, u8 log_lvl);
int init_slot(struct Slot *slot);
ssize_t spi_api_exchange(struct Slot *slot, u8 * tx_buffer, u8 * rx_buffer);
void print_struct(struct Slot *slot);
int wait_for_interrupt(struct Slot *slot, u16 timeout_ms);
int prepare_frame(struct Slot *slot, u8 * data);
void print_frame(struct Slot *slot, u8 * data);
int check_crc(struct Slot *slot, u8 * data, u8 log_lvl);
int get_notification(struct Slot *slot);
int sync_com(struct Slot *slot);

#define DEFAULT_FRAME_SIZE 64
#define DEFAULT_SCLK_SPEED 100000	//kHz
#define DEFAULT_CRC_SIZE 2
#define CRC32_SIZE 4
#define MAXIMUM_FRAME_SIZE 4096	// >4096 not implemented!

#define LOG_LVL_SILENT 0
#define LOG_LVL_NORMAL 1
#define LOG_LVL_VERBOSE 2

#define SDBP_MSG_TYPE_OPERATION 0x01
#define SDBP_MSG_TYPE_RESPONSE 0x02
#define SDBP_MSG_TYPE_NOTIFICATION 0x03
#define SDBP_MSG_TYPE_ACKNOWLEDGEMENT 0x04

#define SDBP_OPTION_BYTE 0x00
#define SDBP_OPTION_BYTE_NOTIFICATION_PENDING 0x01

#define SDBP_CLASSID_CORE 0x01
#define SDBP_CLASSID_STANDARD 0x02
#define SDBP_CLASSID_CUSTOM 0x03

#define SDBP_C_TRANSACTION_ERROR 0x01
#define SDBP_C_TRANSACTION_ERROR_MESSAGE_TYPE_INVALID 0x01
#define SDBP_C_TRANSACTION_ERROR_CLASS_IDENTIFIER_INVALID 0x02
#define SDBP_C_TRANSACTION_ERROR_FRAME_CRC 0x03
#define SDBP_C_TRANSACTION_ERROR_CLASS_INVALID 0x04
#define SDBP_C_TRANSACTION_ERROR_DATA_LENGTH_INVALID 0x05
#define SDBP_C_TRANSACTION_ERROR_DEVICE_WRONG_MODE 0x06

#define SDBP_C_WAIT 0x05
#define SDBP_C_WAIT_WAIT 0x02

#define DUMMY_PATTERN 0x7F

#endif
