#include "all.h"
#include "cedarx_hardware.h"
#include "JpgencLibApi.h"

static options_t *option = NULL;
static VENC_DEVICE *device = NULL;
static struct VIDEO_ENCODE_FORMAT vef = {
	.color_format	= PIXEL_YUV420,
	.color_space	= COLOR_SPACE_BT601,
	.quality	= 50,
};

void jpegencode_init(void)
{
	int result = 0;
	cedarx_hardware_init(0);
	device = JPGVENC_init(&result);
	if(result != 0 || !device)
		ERROREXIT("init, failed.\n");

	result = device->open(device);
	if(result != 0)
		ERROREXIT("open, failed.\n");
}

void jpegencode_exit()
{
	device->close(device);
	cedarx_hardware_exit(0);
}

void jpegencode_frame(const char *filename)
{
	rawsource_t *raw = source_open(filename);
	if(!raw)
		ERROREXIT("source open, failed.\n");

	unsigned char *buffer = (unsigned char *)sunxi_alloc_alloc(raw->size);
	if(!buffer)
		ERROREXIT("buffer alloc, failed.\n");

	if(source_fill(raw, buffer, raw->size) != raw->size)
		ERROREXIT("source fill buffer, failed.\n");
	source_close(raw);
	sunxi_flush_cache(buffer, raw->size);


	__video_crop_crop_para_t crop;
	crop.crop_img_enable = 0;
	crop.start_x = 0;
	crop.start_y = 0;
	crop.crop_pic_width = 160;
	crop.crop_pic_height = 160;
	device->IoCtrl(device, VENC_CROP_IMAGE_CMD, (__u32)&crop);


	vef.width = raw->width;
	vef.height = raw->height;
	vef.src_width = raw->width;
	vef.src_height = raw->height;
	if(device->IoCtrl(device, VENC_SET_ENC_INFO_CMD, (__u32)&vef) != 0)
		ERROREXIT("set info, failed.\n");

	VEnc_FrmBuf_Info info;
	MEMSETZERO(info);
	info.addrY = (unsigned char*)sunxi_alloc_vir2phy(buffer);
	info.addrCb = info.addrY + raw->sizeY;
	info.color_fmt = vef.color_format;
	info.color_space = vef.color_space;
	if(device->encode(device, &info) != 0)
		ERROREXIT("encode, failed.\n");

	__vbv_data_ctrl_info_t out;
	MEMSETZERO(out);
	if(device->GetBitStreamInfo(device, &out) != 0)
		ERROREXIT("get bitstream, failed.\n");

	if(out.uSize0 > 0 && option->outfile) {
		int fd = open(option->outfile, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
		if(fd >= 0) {
			write(fd, out.pData0, out.uSize0);
			close(fd);
			printf("written: %d %s\n", out.uSize0, option->outfile);
		}
	}
	if(out.uSize1 > 0)
		ERROREXIT("leftover bits in buffer1.\n");
}

int main(int argc, char *argv[])
{
	option = options_get(argc, argv);
	if(option && option->infile) {
		jpegencode_init();
		jpegencode_frame(option->infile);
		jpegencode_exit();
	}
	return 0;
}

