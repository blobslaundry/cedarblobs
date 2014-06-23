#include "all.h"

static void tile_to_yuv_luma(int w, int h, unsigned char *src, int size, unsigned char *dst)
{
	int x;
	int y;
	int t;
	int o;
	for(y = 0; y < h; y++)
		for(x = 0; x < w; x++) {
			t = (x / 32) + (y / 32) * (w / 32);
			o = (32 * 32) * t + (y % 32) * 32 + (x % 32);
			dst[y * w + x] = src[o];
		}
}

static void tile_to_yuv_chroma(int w, int h, unsigned char *src, int size, unsigned char *dst)
{
	int x;
	int y;
	int t;
	int o;
	for(y = 0; y < h; y++)
		for(x = 0; x < w; x++) {
			t = (x / 16) + (y / 32) * (w / 16);
			o = (32 * 32) * t + ((y % 32) * 16 + (x % 16)) * 2;
			dst[y * w + x] = src[o];
			dst[(size / 2) + y * w + x] = src[o + 1];
		}
}

static FILE *fp = NULL;
void sink_to_file(const char *outfile, int width, int height,
		  unsigned char *dataY, int datasizeY, unsigned char *dataC, int datasizeC)
{
	unsigned char *outbuf;
	int aligned_width  = ((width + 31) / 32) * 32;
	int aligned_height = ((height + 31) / 32) * 32;
	int sizeY = aligned_width * aligned_height;
	int sizeC = sizeY / 2;

	if(!fp) {
		fp = fopen(outfile, "w");
		if(!fp)
			ERROREXIT("fopen %s, failed.\n", outfile);
		printf("sink: %dx%d %s\n", aligned_width, aligned_height, outfile);
		fprintf(fp, "YUV4MPEG2 Ip F10:1 A1:1 C420jpeg W%d H%d\n", aligned_width, aligned_height);
	}
	fprintf(fp, "FRAME\n");

	outbuf = malloc(sizeY);
	memset(outbuf, 0x80, sizeY);
	tile_to_yuv_luma(aligned_width, aligned_height, dataY, sizeY, outbuf);
	fwrite(outbuf, sizeY, 1, fp);

	memset(outbuf, 0x80, sizeY);
	tile_to_yuv_chroma(aligned_width / 2, aligned_height / 2, dataC, sizeC, outbuf);
	fwrite(outbuf, sizeC, 1, fp);

	printf("sink: written %x %x\n", sizeY, sizeC);
	free(outbuf);
}

