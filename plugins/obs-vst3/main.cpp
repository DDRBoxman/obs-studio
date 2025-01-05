#include <obs-module.h>
#include "VST3Filter.h"

#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/hosting/plugprovider.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-vst3", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "OBS VST3 module";
}

static const char *get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("VSTModule.Name");
}

void *create(obs_data_t *, obs_source_t *source)
{
	return new VST3Filter(source);
}

void destroy(void *data)
{
	delete static_cast<VST3Filter *>(data);
}

void update(void *, obs_data_t *) {}

struct obs_audio_data *filter_audio(void *, struct obs_audio_data *audio_data)
{
	return audio_data;
}

obs_properties_t *get_properties(void *)
{
	return NULL;
}

void save(void *, obs_data_t *) {}

void testLoad()
{
	auto paths = VST3::Hosting::Module::getModulePaths();
	if (paths.empty()) {
		blog(LOG_DEBUG, "No Plug-ins found.");
		return;
	}
	for (const auto &path : paths) {
		blog(LOG_DEBUG, "VST3 PATH: %s", path.c_str());
	}
}

bool obs_module_load()
{
	testLoad();

	struct obs_source_info vst3_filter = {};
	vst3_filter.id = "vst3_filter";
	vst3_filter.type = OBS_SOURCE_TYPE_FILTER;
	vst3_filter.output_flags = OBS_SOURCE_AUDIO;
	vst3_filter.get_name = get_name;
	vst3_filter.create = create;
	vst3_filter.destroy = destroy;
	vst3_filter.update = update;
	vst3_filter.filter_audio = filter_audio;
	vst3_filter.get_properties = get_properties;
	vst3_filter.save = save;

	obs_register_source(&vst3_filter);
	return true;
}
