#include "all.h"
#include <libavformat/avformat.h>

static AVFormatContext *fc = NULL;
static int stream_id = -1;

void demux_open(demux_t *demux, const char *filename)
{
	av_register_all();
	if(avformat_open_input(&fc, filename, NULL, NULL) < 0)
		ERROREXIT("open input, failed.\n");

	if(avformat_find_stream_info(fc, NULL) < 0)
		ERROREXIT("find stream info, failed.\n");

	AVCodec *codec = NULL;
	stream_id = av_find_best_stream(fc, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);
	if(stream_id < 0)
		ERROREXIT("find best stream, failed.\n");

	AVCodecContext *c = fc->streams[stream_id]->codec;
	if(c && demux) {
		demux->name = codec->name;
		demux->width = c->coded_width;
		demux->height = c->coded_height;
		demux->extradata = c->extradata;
		demux->extrasize = c->extradata_size;
	}
}

static AVPacket pk;
int demux_get_packet(unsigned char **data, unsigned int *size)
{
	av_free_packet(&pk);
	av_init_packet(&pk);
	while(av_read_frame(fc, &pk) >= 0) {
		if(pk.stream_index != stream_id)
			continue;
		if(data)
			*data = pk.data;
		if(size)
			*size = pk.size;
		return pk.size;
	}
	return -1;
}

void demux_discard(int start)
{
	int discarded = 1;
	while(discarded < start && demux_get_packet(NULL, NULL) > 0)
		discarded++;
}

void demux_close(void)
{
	avformat_close_input(&fc);
}

