#include "all.h"
#include <libavformat/avformat.h>

static AVFormatContext *fc = NULL;

void mux_init(const char *outfile, int width, int height, int rate, int fps, const char *type)
{
	if(!outfile) return;

	av_register_all();
	fc = avformat_alloc_context();
	if(!fc) ERROREXIT("alloc context, failed.\n");

	fc->oformat = av_guess_format("matroska", NULL, NULL);
	if(!fc->oformat) ERROREXIT("guess format, failed.\n");

	strncpy(fc->filename, outfile, sizeof(fc->filename));
	if(avio_open(&fc->pb, fc->filename, AVIO_FLAG_WRITE) < 0)
		ERROREXIT("io open, failed.\n");

	AVStream *s = avformat_new_stream(fc, NULL);
	if(!s) ERROREXIT("new stream, failed.\n");

	AVCodecContext *c = s->codec;
	c->codec_type = AVMEDIA_TYPE_VIDEO;
	c->codec_id = CODEC_ID_H264;
	c->width = width;
	c->height = height;
	c->bit_rate = rate;
	c->time_base.num = 1000 / fps;
	c->time_base.den = 1000;
	c->pix_fmt = PIX_FMT_YUV420P;
	if(fc->oformat->flags & AVFMT_GLOBALHEADER)
		c->flags |= CODEC_FLAG_GLOBAL_HEADER;
	c->flags |= CODEC_FLAG_BITEXACT;

	if(strcmp(type, "mjpeg") == 0)
		c->codec_id = CODEC_ID_MJPEG;
	else if(strcmp(type, "h264") == 0)
		c->codec_id = CODEC_ID_H264;
	else
		ERROREXIT("unknown codec id %s.", type);
}

void mux_write_header(uint8_t *extradata, int extrasize)
{
	if(!fc) return;

	if(extradata) {
		AVCodecContext *c = fc->streams[0]->codec;
		c->extradata = av_malloc(extrasize);
		memcpy(c->extradata, extradata, extrasize);
		c->extradata_size = extrasize;
	}
	avformat_write_header(fc, NULL);
}

void mux_write(uint8_t *data, int size, int keyframe)
{
	if(!fc) return;

	AVPacket p;
	av_init_packet(&p);
	p.data = data;
	p.size = size;
	if(keyframe) p.flags |= AV_PKT_FLAG_KEY;
	if(av_write_frame(fc, &p) < 0)
		ERROREXIT("write frame, failed.\n");
	av_free_packet(&p);
}

void mux_close(void)
{
	if(!fc) return;

	av_write_trailer(fc);
	avio_close(fc->pb);
	avformat_free_context(fc);
}

