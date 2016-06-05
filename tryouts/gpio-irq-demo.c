#include <stdio.h>
#include <poll.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>


int main(int argc, char **argv)
{
	char fn[64];
	FILE *f;
	int fd;
	struct pollfd pfd;

	if(argc!=2) {
		printf("Usage: %s <GPIO>\n", argv[0]);
		return 1;
	}

	/* Export GPIO port */
	snprintf(fn, sizeof(fn), "/sys/class/gpio/export");
	f = fopen(fn, "w");
	if (f == NULL) {
		perror(fn);
		return 2;
	}
	fprintf(f, "%s\n", argv[1]);
	fclose(f);

	/* Setup GPIO input */
	snprintf(fn, sizeof(fn), "/sys/class/gpio/gpio%s/direction", argv[1]);
	f = fopen(fn, "w");
	if (f == NULL) {
		perror(fn);
		return 2;
	}
	fprintf(f, "in\n", argv[1]);
	fclose(f);

	/* Setup GPIO edge detection */
	snprintf(fn, sizeof(fn), "/sys/class/gpio/gpio%s/edge", argv[1]);
	f = fopen(fn, "w");
	if (f == NULL) {
		perror(fn);
		return 2;
	}
	fprintf(f, "both\n", argv[1]);
	fclose(f);

	/* Open GPIO port */
	snprintf(fn, sizeof(fn), "/sys/class/gpio/gpio%s/value", argv[1]);
	fd=open(fn, O_RDONLY);
	if(fd<0) {
		perror(fn);
		return 2;
	}

	pfd.fd=fd;
	pfd.events=POLLPRI;
	
	while (1) {
		char rdbuf[5];
		int ret;
		int i;

		ret=poll(&pfd, 1, -1);
		if (ret<0) {
			perror("poll()");
			close(fd);
			return 3;
		}
		if (ret==0) {
			printf("timeout\n");
			continue;
		}

		lseek(fd, 0, SEEK_SET);
		ret=read(fd, rdbuf, sizeof(rdbuf)-1);
		if(ret<0) {
			perror("read()");
			close(fd);
			return 4;
		}

		rdbuf[ret] = 0;
		for (i = 0; i < ret; i++) {
			if (rdbuf[i] < ' ') {
				rdbuf[i] = 0;
			}
		}
		printf("interrupt, value is: %s\n", rdbuf);
	}

	close(fd);
	return 0;
}
