#include "all.h"
#include "cedarx_hardware.h"
#include "libcedarv.h"

static options_t *option = NULL;
static cedarv_decoder_t *decoder = NULL;
static int nwritten = 0;
static int ndecoded = 0;
static int ndisplay = 0;

#define IFFORMAT(type, main, sub) \
	if(strcmp(demux->name, type) == 0) { \
		info->format = CEDARV_STREAM_FORMAT_ ## main; \
		info->sub_format = CEDARV_ ## sub; \
	}
static void fill_info(cedarv_stream_info_t *info, demux_t *demux)
{
	info->container_format = CEDARV_CONTAINER_FORMAT_UNKNOW;
	info->sub_format = CEDARV_SUB_FORMAT_UNKNOW;
	info->format = CEDARV_STREAM_FORMAT_UNKNOW;

	     IFFORMAT("mjpeg",		MJPEG,       SUB_FORMAT_UNKNOW)
	else IFFORMAT("mpeg1video",	MPEG2, MPEG2_SUB_FORMAT_MPEG1)
	else IFFORMAT("mpeg2video",	MPEG2, MPEG2_SUB_FORMAT_MPEG2)
	else IFFORMAT("mpeg4",		MPEG4, MPEG4_SUB_FORMAT_DIVX5)
	else IFFORMAT("msmpeg4",	MPEG4, MPEG4_SUB_FORMAT_DIVX3)
	else IFFORMAT("wmv1",		MPEG4, MPEG4_SUB_FORMAT_WMV1)
	else IFFORMAT("wmv2",		MPEG4, MPEG4_SUB_FORMAT_WMV2)
	else IFFORMAT("h263",		MPEG4, MPEG4_SUB_FORMAT_H263)
	else IFFORMAT("vp6",		MPEG4, MPEG4_SUB_FORMAT_VP6)
	else IFFORMAT("vp6f",		MPEG4, MPEG4_SUB_FORMAT_VP6)
	else IFFORMAT("vc1",		VC1,         SUB_FORMAT_UNKNOW)
	else IFFORMAT("h264",		H264,        SUB_FORMAT_UNKNOW)
	else IFFORMAT("vp8",		VP8,         SUB_FORMAT_UNKNOW)
	else ERROREXIT("format match %s, failed.\n", demux->name);

	info->video_width  = demux->width;
	info->video_height = demux->height;
	info->frame_rate = 10000;
	info->frame_duration = 1000 * 1000;
	info->aspect_ratio = 1000;
	info->init_data_len = demux->extrasize;
	info->init_data     = demux->extradata;
	info->is_pts_correct = 0;
	info->_3d_mode = CEDARV_3D_MODE_NONE;
}

void decode_init(const char *filename)
{
	int result;
	cedarx_hardware_init(0);

	decoder = cedarvDecInit(&result);

	demux_t demux;
	demux_open(&demux, filename);
	if(option->start > 0) demux_discard(option->start);

	cedarv_stream_info_t info;
	MEMSETZERO(info);
	fill_info(&info, &demux);

	if(decoder->set_vstream_info(decoder, &info) != 0)
		ERROREXIT("set stream info, failed.\n");

	if(decoder->open(decoder) != 0)
		ERROREXIT("open, failed.\n");

	if(decoder->ioctrl(decoder, CEDARV_COMMAND_RESET, 0) != 0)
		ERROREXIT("reset, failed.\n");

	if(decoder->ioctrl(decoder, CEDARV_COMMAND_PLAY, 0) != 0)
		ERROREXIT("play, failed.\n");
}

void decode_exit(void)
{
	decoder->ioctrl(decoder, CEDARV_COMMAND_STOP, 0);
	decoder->close(decoder);
	cedarvDecExit(decoder);
	cedarx_hardware_exit(0);
}

int decode_display(void)
{
	cedarv_picture_t picture;
	if(decoder->display_request(decoder, &picture) == 0) {
		ndisplay++;
		if(option->outfile) {
			unsigned char *addrY = sunxi_alloc_phy2vir(picture.y);
			unsigned char *addrC = sunxi_alloc_phy2vir(picture.u);
			sink_to_file(option->outfile, picture.width, picture.height,
						      addrY, picture.size_y, addrC, picture.size_u);
		}
		if(decoder->display_release(decoder, picture.id) != 0) {
			ERROREXIT("display release, failed.\n");
		}
		return 1;
	}
	return 0;
}

static int fill_circular_buffer(unsigned char *source, unsigned int size,
				unsigned char *right, unsigned int rightsize,
				unsigned char *left, unsigned int leftsize)
{
	int rwrite = 0;
	int lwrite = 0;
	int total = rightsize + leftsize;
	if(source && size > 0 && total >= size) {
		if(right && rightsize > 0) {
			rwrite = (rightsize > size) ? size : rightsize;
			memcpy(right, source, rwrite);
			if(left && leftsize > 0 && size > rwrite) {
				lwrite = size - rwrite;
				lwrite = (leftsize > lwrite) ? lwrite : leftsize;
				memcpy(left, source + rwrite, lwrite);
			}
			return 0;
		}
	}
	return -1;
}

int decode_write(void)
{
	unsigned char *buffer0 = NULL;
	unsigned char *buffer1 = NULL;
	unsigned int bufsize0 = 0;
	unsigned int bufsize1 = 0;
	unsigned char *data = NULL;
	unsigned int datasize = 0;

	if(demux_get_packet(&data, &datasize) > 0) {
		if(decoder->request_write(decoder, datasize, &buffer0, &bufsize0, &buffer1, &bufsize1) == 0) {

			if(fill_circular_buffer(data, datasize, buffer0, bufsize0, buffer1, bufsize1) != 0)
				return -1;

			if(datasize > 0) {
				cedarv_stream_data_info_t datainfo;
				datainfo.flags = CEDARV_FLAG_FIRST_PART | CEDARV_FLAG_LAST_PART;
				datainfo.length = datasize;
				datainfo.pts = 0;
				datainfo.type = CDX_VIDEO_STREAM_MAJOR;
				if(decoder->update_data(decoder, &datainfo) == 0) {
					nwritten++;
					return 0;
				}
			}
		}
	}
	return -1;
}

static const char* decode_to_string(cedarv_result_e result)
{
	switch(result) {
		case CEDARV_RESULT_ERR_FAIL:		return "ERROR FAIL";
		case CEDARV_RESULT_ERR_INVALID_PARAM:	return "ERROR INVALID PARAMETER";
		case CEDARV_RESULT_ERR_INVALID_STREAM:	return "ERROR INVALID BITSTREAM";
		case CEDARV_RESULT_ERR_NO_MEMORY:	return "ERROR NO MEMORY";
		case CEDARV_RESULT_ERR_UNSUPPORTED:	return "ERROR UNSUPPORTED";
		case CEDARV_RESULT_OK:			return "OK";
		case CEDARV_RESULT_FRAME_DECODED:	return "FRAME DECODED";
		case CEDARV_RESULT_KEYFRAME_DECODED:	return "KEYFRAME DECODED";
		case CEDARV_RESULT_NO_FRAME_BUFFER:	return "NO FRAME BUFFER";
		case CEDARV_RESULT_NO_BITSTREAM:	return "NO BITSTREAM";
		case CEDARV_RESULT_MULTI_PIXEL:		return "MULTI PIXEL";
	}
	return "unknown";
}

int decode_frame(void)
{
	for(;;) {
		int result = decoder->decode(decoder);
		switch(result) {
			case CEDARV_RESULT_FRAME_DECODED:
			case CEDARV_RESULT_KEYFRAME_DECODED:
				ndecoded++;
				decode_display();
				printf("decode: %d/%d\n", ndecoded, ndisplay);
				return 1;
				break;
			case CEDARV_RESULT_OK:
				break;
			case CEDARV_RESULT_NO_BITSTREAM:
				if(decode_write() < 0) {
					printf("decode: end of bitstream.\n");
					return 0;
				}
				break;
			default:
				printf("decode: (%s)\n", decode_to_string(result));
				return -1;
				break;
		}
	}
	return -10;
}

int main(int argc, char *argv[])
{
	int n = 1;
	option = options_get(argc, argv);
	if(option && option->infile) {
		decode_init(option->infile);
		while((option->number >= n || option->number < 0) && (decode_frame() > 0)) {
			n++;
		}
		decode_exit();
		printf("decoded: %d/%d/%d\n", nwritten, ndecoded, ndisplay);
	}
	return 0;
}

