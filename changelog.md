# V1.1.2
- Fixed driver unloading if previous slot initialization failed.
- Fixed max. notification size check.  
- Removed "dev_trigger_notification" debugging attribute.
- Improved logging.

# V1.1.1
- Fixed "rid" attribute change on UPDATE_DESCRIPTOR.

# V1.1.0
- Added "rid" attribute which contains a random id which changes after descriptor update.
- Added "dev_trigger_notification" attribute which triggers a notification if read.
- Disabled notification overwrite if a new notification is triggered.
- Implemented lock free attribute (descriptor) reading.

# V1.0.2
- Implemented UPDATE_DESCRIPTOR on driver_close(...).
- Implemented error logging for low level spi transfer fails.
- Improved error logging for attributes.

# V1.0.1
- Implemented transaction error message decoding.
- Fixed some uninitialized variables.
- Fixed missing protocol version check in get_descriptor(...).
- Implemented auto retransmit when the response is corrupted (notifi. bug).
- Implemented auto retransmit when an unexpected acknow. is received.
- Fixed device disconnect detection on driver_write/close(...).
- Fixed update_descriptor(...) failure when the driver executes the function before the CTS interrupt has ended.
- Fixed deadlock between driver_write(...) and sdbp_main(...) caused by wrong write counter and triggered by fast access.

# V1.0.0
- Initial release.
