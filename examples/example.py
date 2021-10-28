#!/usr/bin/python3
import time
import os


class Descriptor:

	def __init__(self,devicepath):
		with open(path+device+"/vendor_product_id", "rb",0) as f:
			self.vendor_product_id = f.read(256)
			self.__check_data(self.vendor_product_id,length_max=256)
		with open(path+device+"/vendor_name", "rb",0) as f:
			self.vendor_name = f.read(256)
			self.__check_data(self.vendor_name,length_max=256)
		with open(path+device+"/product_name", "rb",0) as f:
			self.product_name = f.read(256)
			self.__check_data(self.product_name,length_max=256)
		with open(path+device+"/serial_code", "rb",0) as f:
			self.serial_code = f.read(39+1)
			self.__check_data(self.serial_code,39+1)
		with open(path+device+"/max_power_3v3", "rb",0) as f:
			self.max_power_3v3 = f.read(5+1)
			self.__check_data(self.max_power_3v3,length_max=5+1)
		with open(path+device+"/max_power_5v0", "rb",0) as f:
			self.max_power_5v0 = f.read(5+1)
			self.__check_data(self.max_power_5v0,length_max=5+1)
		with open(path+device+"/max_power_12v", "rb",0) as f:
			self.max_power_12v = f.read(5+1)
			self.__check_data(self.max_power_12v,length_max=5+1)
		with open(path+device+"/max_sclk_speed", "rb",0) as f:
			self.max_sclk_speed = f.read(10+1)
			self.__check_data(self.max_sclk_speed,length_max=10+1)
		with open(path+device+"/max_frame_size", "rb",0) as f:
			self.max_frame_size = f.read(5+1)
			self.__check_data(self.max_frame_size,length_max=5+1)
		with open(path+device+"/fw_version", "rb",0) as f:
			self.fw_version = f.read(19+1)
			self.__check_data(self.fw_version,length_max=19+1)
		with open(path+device+"/hw_version", "rb",0) as f:
			self.hw_version = f.read(17+1)
			self.__check_data(self.hw_version,length_max=17+1)
		with open(path+device+"/protocol_version", "rb",0) as f:
			self.protocol_version = f.read(17+1)
			self.__check_data(self.protocol_version,length_max=17+1)
		with open(path+device+"/bootloader_state", "rb",0) as f:
			self.bootloader_state = f.read(1+1)
			self.__check_data(self.bootloader_state,1+1)

	def __check_data(self,data,static_length = 0,length_max = 0):
		if data[-1:] != b'\x00':
			raise ValueError('Invalid string ending!')
		if (static_length != 0 and static_length != len(data)) or (length_max != 0 and length_max < len(data)):
				raise ValueError('Invalid string length!')


	def print(self):
		print('Vendor product id:\t\t' + str(self.decode(self.vendor_product_id)))
		print('Vendor name:\t\t\t' + str(self.decode(self.vendor_name)))
		print('Product name:\t\t\t' + str(self.decode(self.product_name)))
		print('Serial code:\t\t\t' + str(self.decode(self.serial_code)))
		print('Maximum power 3V3:\t\t' + str(self.decode(self.max_power_3v3)))
		print('Maximum power 5V0:\t\t' + str(self.decode(self.max_power_5v0)))
		print('Maximum power 12V:\t\t' + str(self.decode(self.max_power_12v)))
		print('Maximum SCLK speed:\t\t' + str(self.decode(self.max_sclk_speed)))
		print('Maximum frame size:\t\t' + str(self.decode(self.max_frame_size)))
		print('Firmware version:\t\t' + str(self.decode(self.fw_version)))
		print('Hardware version:\t\t' + str(self.decode(self.hw_version)))
		print('Protocol version:\t\t' + str(self.decode(self.protocol_version)))
		print('Bootloader state:\t\t' + str(self.decode(self.bootloader_state)))

	def decode(self,data):
		return bytes(data).decode('ascii').rstrip()


path = "/sys/class/sdbp/"
dev_path = "/dev/"
slots = sorted(os.listdir( path ))
protocol_version_cmd = b'\x01\x02\x08'

print('Connected slots: ' + str(slots))

for device in slots:
	Descriptor(path+device).print()
	print("------------------------------------------------------------")

print("")
for device in slots:
	print("Testing exchange for " + str(device))
	with open(dev_path+device, "rb+",0) as f:
		written_bytes = f.write(protocol_version_cmd)
		if written_bytes != len(protocol_version_cmd):
			raise ValueError('Number of written bytes is wrong!')
		data = f.read(64)
		if len(data) != 9:
			raise ValueError('Number of read bytes is wrong!')
		print("Protocol version response: " + str(data))
		print("")


for device in slots:
	print("Testing performance for " + str(device))
	start_time = time.perf_counter()
	test_time = 10  # seconds
	cnt = 0
	with open(dev_path+device, "rb+",0) as f:
		while time.perf_counter()-start_time < test_time:
			f.write(protocol_version_cmd)
			data = f.read(64)
			cnt += 1
	print(str(cnt/test_time) + " transactions per second (100kHz default)")
	print("")
