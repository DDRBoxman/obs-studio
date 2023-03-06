#include "webrtc-output.h"
#include "whip.h"

static const char *webrtc_output_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("WebRTCOutput");
}

static void *webrtc_output_create(obs_data_t *settings, obs_output_t *output)
{
    UNUSED_PARAMETER(settings);
    auto webrtc_output = new WebRTCOutput();

    webrtc_output->output = output;

    webrtc_output->obsrtc = new OBSWebRTCStream();

	return webrtc_output;
}

static void webrtc_output_destroy(void *data) {
    UNUSED_PARAMETER(data);
    //static_cast<WebRTCOutput *>(data);
}

static bool webrtc_output_start(void *data)
{
    auto webrtc_output = static_cast<WebRTCOutput *>(data);

	if (!obs_output_can_begin_data_capture(webrtc_output->output, 0)) {
		return false;
	}
	if (!obs_output_initialize_encoders(webrtc_output->output, 0)) {
		return false;
	}

    auto offer_sdp = webrtc_output->obsrtc->Setup();

	// todo: find a good way to let plugins use the SDP here to do something other than whip
	auto answer_sdp = whip_it("http://127.0.0.1:8000/whip", offer_sdp);

    if (answer_sdp == NULL) {
        return false;
    }
    
	webrtc_output->obsrtc->Connect(answer_sdp);

	free(answer_sdp);
    
	obs_output_begin_data_capture(webrtc_output->output, 0);

	return true;
}

static void webrtc_output_stop(void *data, uint64_t ts)
{
    UNUSED_PARAMETER(ts);
    auto webrtc_output = static_cast<WebRTCOutput *>(data);

	obs_output_end_data_capture(webrtc_output->output);
}

static void webrtc_output_data(void *data, struct encoder_packet *packet)
{
    auto webrtc_output = static_cast<WebRTCOutput *>(data);

	if (packet->type == OBS_ENCODER_VIDEO) {
		int64_t duration = packet->dts_usec - webrtc_output->video_timestamp;
        webrtc_output->obsrtc->SendVideo(packet->data,
                                         packet->size, duration);
        webrtc_output->video_timestamp = packet->dts_usec;
	}

	if (packet->type == OBS_ENCODER_AUDIO) {
		int64_t duration = packet->dts_usec - webrtc_output->audio_timestamp;
        webrtc_output->obsrtc->SendAudio(packet->data,
					packet->size, duration);
        webrtc_output->audio_timestamp = packet->dts_usec;
	}
}

static void webrtc_output_defaults(obs_data_t *defaults)
{
    UNUSED_PARAMETER(defaults);
	/*obs_data_set_default_int(defaults, OPT_DROP_THRESHOLD, 700);
 	obs_data_set_default_int(defaults, OPT_PFRAME_DROP_THRESHOLD, 900);
 	obs_data_set_default_int(defaults, OPT_MAX_SHUTDOWN_TIME_SEC, 30);
 	obs_data_set_default_string(defaults, OPT_BIND_IP, "default");
 	obs_data_set_default_bool(defaults, OPT_NEWSOCKETLOOP_ENABLED, false);
 	obs_data_set_default_bool(defaults, OPT_LOWLATENCY_ENABLED, false);*/
}

static obs_properties_t *webrtc_output_properties(void *unused)
{
	UNUSED_PARAMETER(unused);

	obs_properties_t *props = obs_properties_create();

	return props;
}

static uint64_t webrtc_output_total_bytes_sent(void *data)
{
    UNUSED_PARAMETER(data);
	//struct rtmp_stream *stream = data;
	return 100;
}

static int webrtc_output_dropped_frames(void *data)
{
    UNUSED_PARAMETER(data);
	//struct rtmp_stream *stream = data;
	return 0;
}

static int webrtc_output_connect_time(void *data)
{
    UNUSED_PARAMETER(data);
	//struct rtmp_stream *stream = data;
	return 100;
}

struct obs_output_info webrtc_output_info = {
	.id = "webrtc_output",
	.flags = OBS_OUTPUT_AV | OBS_OUTPUT_ENCODED | OBS_OUTPUT_SERVICE |
		 OBS_OUTPUT_MULTI_TRACK,
	.encoded_video_codecs = "h264",
	.encoded_audio_codecs = "opus",
	.get_name = webrtc_output_getname,
	.create = webrtc_output_create,
	.destroy = webrtc_output_destroy,
	.start = webrtc_output_start,
	.stop = webrtc_output_stop,
	.encoded_packet = webrtc_output_data,
	.get_defaults = webrtc_output_defaults,
	.get_properties = webrtc_output_properties,
	.get_total_bytes = webrtc_output_total_bytes_sent,
	.get_connect_time_ms = webrtc_output_connect_time,
	.get_dropped_frames = webrtc_output_dropped_frames,
};
