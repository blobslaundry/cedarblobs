#include "all.h"
#include "cedarx_hardware.h"
#include "venc.h"

static options_t *option = NULL;
static rawsource_t *raw = NULL;
static cedarv_encoder_t *device = NULL;
static VencBaseConfig baseconfig = {
	.input_width	= 32,
	.input_height	= 32,
	.dst_width	= 32,
	.dst_height	= 32,
	.framerate	= 25,
	.targetbitrate	= (1024 * 1024) * 1,
	.inputformat	= VENC_PIXEL_YUV420,
	.codectype	= VENC_CODEC_H264, /* VENC_CODEC_JPEG */
	.maxKeyInterval	= 2,
};
static VencProfileLevel profilelevel = {
	.profile	= 66,
	.level		= 41,
};
static VencQPrange qprange = {
	.minqp	= 40,
	.maxqp	= 40,
};

void encode_init(void)
{
	raw = source_open(option->infile);
	if(!raw)
		ERROREXIT("source open.\n");

	baseconfig.input_width  = raw->width;
	baseconfig.input_height = raw->height;
	baseconfig.dst_width  = baseconfig.input_width;
	baseconfig.dst_height = baseconfig.input_height;

	cedarx_hardware_init(0);
	device = cedarvEncInit();
	if(!device)
		ERROREXIT("init, failed.\n");
	if(device->ioctrl(device, VENC_CMD_BASE_CONFIG, &baseconfig) != 0)
		ERROREXIT("base config, failed.\n");

	/* blobs don't have this implemented
	if(device->ioctrl(device, VENC_CMD_SET_FROFILE_LEVEL, &profilelevel) != 0)
		ERROREXIT("set profile level, failed.\n");
	if(device->ioctrl(device, VENC_CMD_SET_QP_RANGE, &qprange) != 0)
		ERROREXIT("set qp range, failed.\n");
	*/

	VencAllocateBufferParam allocate;
	allocate.buffernum = 2;
	if(device->ioctrl(device, VENC_CMD_ALLOCATE_INPUT_BUFFER, &allocate) != 0)
		ERROREXIT("allocate input buffer, failed.\n");

	if(device->ioctrl(device, VENC_CMD_OPEN, 0) != 0)
		ERROREXIT("open, failed.\n");

	if(option->outfile) {
		char *codectype = "";
		switch(baseconfig.codectype) {
			case VENC_CODEC_H264: codectype = "h264";  break;
			case VENC_CODEC_JPEG: codectype = "mjpeg"; break;
			default: codectype = "unknown"; break;
		}
		mux_init(option->outfile, baseconfig.dst_width, baseconfig.dst_height,
					  baseconfig.targetbitrate,
					  baseconfig.framerate, codectype);
	}
	
	VencSeqHeader header;
	if(device->ioctrl(device, VENC_CMD_HEADER_DATA, &header) != 0)
		ERROREXIT("header data, failed.\n");

	if(option->outfile)
		mux_write_header(header.bufptr, header.length);
}

void encode_exit(void)
{
	device->ioctrl(device, VENC_CMD_CLOSE, 0);
	cedarvEncExit(device);
	cedarx_hardware_exit(0);
	if(option->outfile)
		mux_close();
	if(raw)
		source_close(raw);
}

int encode_frame(int n)
{
	int filled = 1;
	VencInputBuffer inbuf;
	if(device->ioctrl(device, VENC_CMD_GET_ALLOCATE_INPUT_BUFFER, &inbuf) != 0)
		ERROREXIT("allocate input buffer, failed.\n");

	if(source_fill(raw, inbuf.addrvirY, raw->sizeY) != raw->sizeY) filled = 0;
	if(source_fill(raw, inbuf.addrvirC, raw->sizeC) != raw->sizeC) filled = 0;

	if(filled) {
		if(device->ioctrl(device, VENC_CMD_ENQUENE_INPUT_BUFFER, &inbuf) != 0)
			ERROREXIT("enqueue input buffer, failed.\n");
		if(device->ioctrl(device, VENC_CMD_DEQUENE_INPUT_BUFFER, &inbuf) != 0)
			ERROREXIT("dequeue input buffer, failed.\n");

		if(device->ioctrl(device, VENC_CMD_ENCODE, &inbuf) != 0)
			ERROREXIT("encode, failed.\n");

		VencOutputBuffer outbuf;
		MEMSETZERO(outbuf);
		if(device->ioctrl(device, VENC_CMD_GET_BITSTREAM, &outbuf) != 0)
			ERROREXIT("get bitstream, failed.\n");

		printf("frame %d\n", n);
		if(option->outfile) {
			mux_write(outbuf.ptr0, outbuf.size0, 0);
			if(outbuf.size1 > 0)
				ERROREXIT("unexpected output bitstream size1\n.");
		}

		if(device->ioctrl(device, VENC_CMD_RETURN_BITSTREAM, &outbuf) != 0)
			ERROREXIT("return bitstream, failed.\n");
	}
	if(device->ioctrl(device, VENC_CMD_RETURN_ALLOCATE_INPUT_BUFFER, &inbuf) != 0)
		ERROREXIT("return allocate input buffer, failed.\n");
	return filled;
}

int main(int argc, char *argv[])
{
	int n = 1;
	option = options_get(argc, argv);
	if(option && option->infile) {
		encode_init();
		while((option->number >= n || option->number < 0) && encode_frame(n)) {
			n++;
		}
		encode_exit();
	}
	return 0;
}

