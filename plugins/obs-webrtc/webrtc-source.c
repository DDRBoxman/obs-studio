#include <obs-module.h>

static const char *webrtc_source_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("WebRTCSource");
}

static void *webrtc_source_create(obs_data_t *settings, obs_source_t *source) {}

static void webrtc_source_destroy(void *data) {}

static obs_properties_t *webrtc_source_get_properties(void *data) {}

struct obs_source_info webrtc_source_info = {
	.id = "webrtc_source",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_ASYNC_VIDEO | OBS_SOURCE_AUDIO |
			OBS_SOURCE_DO_NOT_DUPLICATE,
	.get_name = webrtc_source_getname,
	.create = webrtc_source_create,
	.destroy = webrtc_source_destroy,
	.get_properties = webrtc_source_get_properties,
	.icon_type = OBS_ICON_TYPE_MEDIA,
};