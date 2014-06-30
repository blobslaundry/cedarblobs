#ifndef _ALL_H_
#define _ALL_H_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include "wrapalloc.h"

#define MEMSETZERO(object) memset(&object, 0, sizeof(object))
#define ERROREXIT(...) do { fprintf(stderr, "ERROR["__FILE__"]: "__VA_ARGS__); exit(EXIT_FAILURE); } while(0)

typedef struct {
	int fd;
	unsigned int width;
	unsigned int height;
	char format[5];
	unsigned int size;
	unsigned int sizeY;
	unsigned int sizeC;
} rawsource_t;
rawsource_t *source_open(const char*rawfilename);
int  source_fill(rawsource_t *raw, unsigned char *buffer, unsigned int size);
void source_close(rawsource_t *raw);

typedef struct {
	char *infile;
	char *outfile;
	int start;
	int number;
	int verbose;
} options_t;
options_t *options_get(int argc, char *argv[]);

void mux_init(const char *outfile, int width, int height, int rate, int fps, const char *type);
void mux_write_header(unsigned char *extradata, int extrasize);
void mux_write(unsigned char *data, int size, int keyframe);
void mux_close(void);

typedef struct {
	const char *name;
	int width;
	int height;
	unsigned char *extradata;
	unsigned int   extrasize;
} demux_t;
void demux_open(demux_t *demux, const char *filename);
int  demux_get_packet(unsigned char **data, unsigned int *size);
void demux_discard(int start);
void demux_close(void);

#endif

