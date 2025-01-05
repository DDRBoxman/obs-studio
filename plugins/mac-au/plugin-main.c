#include <obs-module.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("mac-au", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "macOS audio unit filter support";
}

extern struct obs_source_info audio_unit_info;

bool obs_module_load(void)
{
	obs_register_source(&audio_unit_info);
	return true;
}
