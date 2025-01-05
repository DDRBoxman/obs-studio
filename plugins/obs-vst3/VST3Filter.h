
class VST3Filter {
public:
	VST3Filter(obs_source_t *source) : source{source} {};

private:
	obs_source_t *source;
};
