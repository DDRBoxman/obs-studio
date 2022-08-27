#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>

#include <graphics/graphics.h>

#include "./webrtc/bindings.h"

#include <obs.h>

OBSWebRTCCall *call;
struct gs_texture *texture;

static inline enum video_format convert_pixel_format(int f)
{
	switch (f) {
	case AV_PIX_FMT_NONE:
		return VIDEO_FORMAT_NONE;
	case AV_PIX_FMT_YUV420P:
		return VIDEO_FORMAT_I420;
	case AV_PIX_FMT_NV12:
		return VIDEO_FORMAT_NV12;
	case AV_PIX_FMT_YUYV422:
		return VIDEO_FORMAT_YUY2;
	case AV_PIX_FMT_YUV444P:
		return VIDEO_FORMAT_I444;
	case AV_PIX_FMT_UYVY422:
		return VIDEO_FORMAT_UYVY;
	case AV_PIX_FMT_RGBA:
		return VIDEO_FORMAT_RGBA;
	case AV_PIX_FMT_BGRA:
		return VIDEO_FORMAT_BGRA;
	case AV_PIX_FMT_BGR0:
		return VIDEO_FORMAT_BGRX;
	case AV_PIX_FMT_YUVA420P:
		return VIDEO_FORMAT_I40A;
	case AV_PIX_FMT_YUVA422P:
		return VIDEO_FORMAT_I42A;
	case AV_PIX_FMT_YUVA444P:
		return VIDEO_FORMAT_YUVA;
	default:;
	}

	return VIDEO_FORMAT_NONE;
}

void packet_callback(void *user_data, uint8_t *packet_data, int length)
{

	//blog(LOG_INFO, "%d", length);

	AVCodecContext *context = (AVCodecContext *)user_data;

	AVPacket *packet = av_packet_alloc();

	av_new_packet(packet, 1024 * 10);

	memcpy(packet->data, packet_data, length);

	avcodec_send_packet(context, packet);

	AVFrame *frame = av_frame_alloc();
	int res = avcodec_receive_frame(context, frame);
	if (res == 0) {
		blog(LOG_INFO, "Width: %d, Height: %d", frame->width,
		     frame->height);
		int format = convert_pixel_format(frame->format);

		obs_enter_graphics();

		struct gs_texture *texture =
			gs_texture_create(frame->width, frame->height, format,
					  1, (const uint8_t **)&frame->data, 0);

		obs_leave_graphics();
	}

	if (res == AVERROR(EAGAIN)) {
		// NO FRAME
	}

	av_free(frame);
}

void setup_call()
{
	AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_H264);

	AVDictionary *opts = NULL;
	//av_dict_set(&opts, "b", "2.5M", 0);
	if (!codec)
		exit(1);
	AVCodecContext *context = avcodec_alloc_context3(codec);
	if (avcodec_open2(context, codec, &opts) < 0)
		exit(1);

	AVCodecParameters *codecpar = avcodec_parameters_alloc();
	codecpar->codec_id = AV_CODEC_ID_H264;
	codecpar->height = 720;
	codecpar->width = 1280;

	//codecpar->

	//avcodec_parameters_to_context(context, codecpar);

	call = obs_webrtc_call_init(context, packet_callback);
}

void start_call()
{
	obs_webrtc_call_start(call);
}

void frame(AVCodecContext *context)
{
	AVFrame frame;
	avcodec_receive_frame(context, &frame);

	struct gs_texture *texture =
		gs_texture_create(frame.width, frame.height, frame.format, 1,
				  (const uint8_t **)&frame.data, 0);
}
