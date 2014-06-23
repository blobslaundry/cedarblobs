#include "all.h"

rawsource_t *source_open(const char *filename)
{
	rawsource_t *raw = NULL;
	int fd;
	int width = 0;
	int height = 0;
	char extension[5] = "none";
	char *underscore = strrchr(filename, '_');
	if(underscore) {
		if(3 == sscanf(underscore, "_%dx%d.%4s", &width, &height, extension)) {
			printf("source: %dx%d %s %s\n", width, height, extension, filename);
			fd = open(filename, O_RDONLY);
			if(fd >= 0) {
				raw = malloc(sizeof(rawsource_t));
				raw->fd = fd;
				raw->width = width;
				raw->height = height;
				strncpy(raw->format, extension, 5);
				raw->size = 0;
				raw->sizeY = 0;
				raw->sizeC = 0;
				if(strcmp(extension, "nv12") == 0) {
					raw->sizeY = raw->width * raw->height;
					raw->sizeC = raw->sizeY / 2;
					raw->size = raw->sizeY + raw->sizeC;
				}
			}
		}
	}
	return raw;
}

int source_fill(rawsource_t *raw, unsigned char *buf, unsigned int size)
{
	size_t r = 0;
	r = read(raw->fd, buf, size);
	if(r != size)
		return -1;
	return r;
}

void source_close(rawsource_t *raw)
{
	close(raw->fd);
}

