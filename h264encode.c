#include "all.h"
#include "cedarx_hardware.h"
#include "H264encLibApi.h"

static VENC_DEVICE *device = NULL;
static rawsource_t *raw = NULL;
static options_t *option = NULL;
static struct VIDEO_ENCODE_FORMAT vef = {
	.color_format	= PIXEL_YUV420,
	.color_space	= COLOR_SPACE_BT601,
	.qp_max		= 40,
	.qp_min		= 40,
	.profileIdc	= 66,
	.levelIdc	= 41,
	.maxKeyInterval = 2,
};

void h264encode_init(const char *rawfile)
{
	int result = 0;
	cedarx_hardware_init(0);
	device = H264EncInit(&result);
	if(!device || result != 0)
		ERROREXIT("init, failed.\n");

	raw = source_open(rawfile);
	if(!raw)
		ERROREXIT("source open, failed.\n");

	vef.width = raw->width;
	vef.height = raw->height;
	vef.src_width = raw->width;
	vef.src_height = raw->height;
	result = device->IoCtrl(device, VENC_SET_ENC_INFO_CMD, (unsigned int)&vef);
	if(result != 0)
		ERROREXIT("set info, failed.\n");

	if(option->outfile)
		mux_init(option->outfile, vef.width, vef.height, vef.avg_bit_rate, 25, "h264");

	result = device->open(device);
	if(result != 0)
		ERROREXIT("open, failed.\n");
}

void h264encode_exit(void)
{
	device->close(device);
	H264EncExit(device);
	cedarx_hardware_exit(0);
	if(option->outfile)
		mux_close();
	if(raw)
		source_close(raw);
}

int h264encode_frame(int n)
{
	unsigned char *virbufY = NULL;
	unsigned char *virbufC = NULL;
	unsigned char *phybufY = NULL;
	unsigned char *phybufC = NULL;

	if(device->RequestBuffer(device, &virbufY, &virbufC, (unsigned int *)&phybufY, (unsigned int *)&phybufC) != 0)
		ERROREXIT("request buffer, failed.\n");

	if(source_fill(raw, virbufY, raw->sizeY) != raw->sizeY)	return 0;
	if(source_fill(raw, virbufC, raw->sizeC) != raw->sizeC)	return 0;

	if(device->UpdataBuffer(device) != 0)
		ERROREXIT("update buffer, failed.\n");

	VEnc_FrmBuf_Info info;
	MEMSETZERO(info);
	info.addrY = phybufY;
	info.addrCb = phybufC;
	info.color_fmt = vef.color_format;
	info.color_space = vef.color_space;
	if(device->encode(device, &info) != 0)
		ERROREXIT("encode, failed.\n");

	__vbv_data_ctrl_info_t out;
	MEMSETZERO(out);
	if(device->GetBitStreamInfo(device, &out) != 0)
		ERROREXIT("get bitstream, failed.\n");

	printf("frame %d\n", n);
	if(option->outfile) {
		if(out.privateDataLen > 0 && out.privateData)
			mux_write_header(out.privateData, out.privateDataLen);
		if(out.uSize0 > 0 && out.pData0) {
			int iskeyframe = (out.keyFrameFlag == 1);
			unsigned int size = out.uSize0;
			unsigned char *buffer = out.pData0;
			if(out.uSize1 > 0 && out.pData1) {
				size = out.uSize0 + out.uSize1;
				buffer = malloc(size);
				memcpy(buffer             , out.pData0, out.uSize0);
				memcpy(buffer + out.uSize0, out.pData1, out.uSize1);
			}
			mux_write(buffer, size, iskeyframe);
			if(buffer != out.pData0)
				free(buffer);
		}
	}
	if(device->ReleaseBitStreamInfo(device, out.idx) != 0)
		ERROREXIT("release bitstream, failed.\n");
	return 1;
}

int main(int argc, char *argv[])
{
	int n = 1;
	option = options_get(argc, argv);
	if(option && option->infile) {
		h264encode_init(option->infile);
		while((option->number >= n || option->number < 0) && h264encode_frame(n)) {
			n++;
		}
		h264encode_exit();
	}
	return 0;
}

