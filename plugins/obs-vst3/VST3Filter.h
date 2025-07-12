#include <obs-module.h>

class VST3Filter {
public:
	VST3Filter(obs_source_t *source) : source{ source } {};
	void filterAudio(struct obs_audio_data *audio_data);
private:
	obs_source_t *source;
};
