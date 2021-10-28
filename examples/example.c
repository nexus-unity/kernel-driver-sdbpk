#define _GNU_SOURCE
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <stdint.h>
#include <sys/time.h>

int main()
{
	int fd, bytes_read, bytes_written;
	printf("Starting performance test...\n");
	fd = open("/dev/slot1", O_RDWR);

	if (fd < 0) {
		fprintf(stderr, "%s\n", strerror(errno));
		return -1;
	}

	long start, end, duration;
	struct timeval timecheck;

	gettimeofday(&timecheck, NULL);
	start = (long)timecheck.tv_sec * 1000 + (long)timecheck.tv_usec / 1000;

	int i = 0;
	int err = 0;

	do {
		char buf[64] = { 0x01, 0x02, 0x02 };

		bytes_written = write(fd, buf, 3);
		if (bytes_written < 0) {
			printf("Write failed\n");
			fprintf(stderr, "%s\n", strerror(errno));
			return -1;
		}
		if (bytes_written != 3) {
			printf("Could not write all data: %d\n", bytes_written);
			fprintf(stderr, "%s\n", strerror(errno));
			return -2;
		}

		bytes_read = read(fd, buf, sizeof(buf));
		if (bytes_read <= 0) {
			printf("Read system call failed.\n");
			fprintf(stderr, "%s\n", strerror(errno));
			return -2;
		}
		gettimeofday(&timecheck, NULL);
		end =
		    (long)timecheck.tv_sec * 1000 +
		    (long)timecheck.tv_usec / 1000;
		duration = end - start;
		//printf("%ld milliseconds elapsed\n", (end - start));
		i++;
	}
	while (duration < 10000);

	printf("%.3f transactions per second (100kHz default)\n", i / 10.0);

	close(fd);
}
