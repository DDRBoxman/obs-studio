#include <obs-module.h>

#include "public.sdk/source/vst/hosting/module.h"

class VST3Manager {
public:

void findModules();

private:
	VST3::Hosting::Module::PathList modulePaths;
};
