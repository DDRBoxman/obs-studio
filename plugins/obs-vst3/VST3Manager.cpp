#include "VST3Manager.h"

//#include "public.sdk/source/vst/hosting/hostclasses.h"
//#include "public.sdk/source/vst/hosting/plugprovider.h"
//#include "public.sdk/source/vst/hosting/plugprovider.h"

#include "public.sdk/source/vst/hosting/plugprovider.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"


void VST3Manager::findModules() {
	this->modulePaths = VST3::Hosting::Module::getModulePaths();
 	if (this->modulePaths.empty()) {
		blog(LOG_DEBUG, "No Plug-ins found.");
		return;
	}
	for (const auto &path : this->modulePaths) {
		blog(LOG_DEBUG, "VST3 PATH: %s", path.c_str());
		}


  Steinberg::FUnknown* gStandardPluginContext = new Steinberg::Vst::HostApplication ();

  Steinberg::Vst::PluginContextFactory::instance ().setPluginContext (gStandardPluginContext);


	std::string error;
	//auto module = VST3::Hosting::Module::create ("/Library/Audio/Plug-Ins/VST3/WaveShell1-VST3 14.12.vst3", error);
	//auto vstModule = VST3::Hosting::Module::create ("/Library/Audio/Plug-Ins/VST3/Kilohearts/kHs Bitcrush.vst3", error);
	//auto module = VST3::Hosting::Module::create ("/Library/Audio/Plug-Ins/VST3/Kilohearts/kHs 3-Band EQ.vst3", error);
	//auto module = VST3::Hosting::Module::create ("/Library/Audio/Plug-Ins/VST3/Serum.vst3", error);
	auto vstModule = VST3::Hosting::Module::create ("/Library/Audio/Plug-Ins/VST3/Kilohearts/kHs Bitcrush.vst3", error);

	if (!vstModule)
	{
		std::string reason = "Could not create Module for file:";
		reason += "\nError: ";
		reason += error;

		blog(LOG_DEBUG, "VST3: %s", reason.c_str());
		return;
	}

	Steinberg::IPtr<Steinberg::Vst::PlugProvider> plugProvider = nullptr;

	auto factory = vstModule->getFactory();
	for (auto& classInfo : factory.classInfos())
	{
	    if (classInfo.category() == kVstAudioEffectClass)
	    {

	      blog(LOG_DEBUG, "VST3: %s", classInfo.get().name.c_str());
		plugProvider = owned (new Steinberg::Vst::PlugProvider (factory, classInfo, true));
		if (plugProvider->initialize () == false) {
			plugProvider = nullptr;
			}
		break;
	    }
	}

	if (!plugProvider)
	{
		error = "No VST3 Audio Module Class found in file ";
		blog(LOG_DEBUG, "VST3: %s", error.c_str());

		return;
	}

	Steinberg::IPtr<Steinberg::Vst::IEditController> editController = plugProvider->getController ();
	if (!editController)
	{
		error = "No EditController found (needed for allowing editor) in file ";
		blog(LOG_DEBUG, "VST3: %s", error.c_str());

		return;
	}
	editController->release (); // plugProvider does an addRef


 /* 	auto factory = module->getFactory();
	Steinberg::Vst::PlugProvider *plugProvider;

	for (auto& classInfo : factory.classInfos ())
	{
		if (classInfo.category () == kVstAudioEffectClass)
		{
			plugProvider = owned (new Steinberg::Vst::PlugProvider (factory, classInfo, true));
			if (plugProvider->initialize () == false)
				plugProvider = nullptr;
			break;
		}
	}

	if (!plugProvider)
	{
		error = "No VST3 Audio Module Class found in file ";
		blog(LOG_DEBUG, "VST3: %s", error.c_str());

		return;
	}

	auto editController = plugProvider->getController ();
	if (!editController)
	{
		error = "No EditController found (needed for allowing editor) in file ";
		blog(LOG_DEBUG, "VST3: %s", error.c_str());

		return;
	}
	editController->release (); // plugProvider does an addRef


	auto view = owned (editController->createView (Steinberg::Vst::ViewType::kEditor));
	if (!view)
	{
		blog(LOG_DEBUG, "EditController does not provide its own editor");
		return;
		}*/


}
