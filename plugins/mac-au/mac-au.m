#include <obs-module.h>
#include <util/deque.h>

#include <AVFAudio/AVAudioUnitComponent.h>
#include <AVFAudio/AVAudioFormat.h>

#include <CoreAudioKit/AUViewController.h>

#include "util.h"

struct audio_unit_filter {
    obs_source_t *source;

    AudioTimeStamp timestamp;

    AUAudioUnit *audioUnit;

    AURenderBlock renderBlock;

    struct deque input_buffer;

    struct deque output_buffer;

    NSWindow *window;

    AudioBufferList *outBufferList;

    AUAudioUnitStatus (^inputBlock)(AudioUnitRenderActionFlags *_Nonnull actionFlags,
                                    const AudioTimeStamp *_Nonnull timestamp, AUAudioFrameCount frameCount,
                                    NSInteger inputBusNumber, AudioBufferList *_Nonnull inputData);
};

static void audio_unit_update(void *data, obs_data_t *settings);

static bool open_editor_button_clicked(obs_properties_t *props, obs_property_t *property, void *data)
{
    UNUSED_PARAMETER(props);
    UNUSED_PARAMETER(property);
    struct audio_unit_filter *auf = data;

    if (auf == NULL || auf->audioUnit == NULL) {
        return true;
    }

    [auf->audioUnit requestViewControllerWithCompletionHandler:(^(AUViewControllerBase *viewController) {
                        if (viewController) {
                            auf->window = [[NSWindow alloc]
                                initWithContentRect:NSMakeRect(0, 0, viewController.view.frame.size.width,
                                                               viewController.view.frame.size.height)
                                          styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
                                            backing:NSBackingStoreBuffered
                                              defer:NO];

                            auf->window.contentViewController = viewController;
                            auf->window.title = @"Audio Unit View";
                            [auf->window center];
                            [auf->window makeKeyAndOrderFront:nil];
                        }
                    })];

    return true;
}

static const char *audio_unit_getname(void *unused)
{
    UNUSED_PARAMETER(unused);
    return obs_module_text("AudioUnit");
}

static inline void *audio_unit_create_internal(obs_data_t *settings, obs_source_t *source)
{
    struct audio_unit_filter *auf = bzalloc(sizeof(struct audio_unit_filter));

    struct obs_audio_info oai;
    obs_get_audio_info(&oai);

    auf->source = source;

    deque_init(&auf->input_buffer);

    int numberOfBuffers = MAX_AUDIO_CHANNELS; //oai.speakers;
	int mNumberChannels = 1; // 1 channel per buffer
	
	
	auf->outBufferList = (AudioBufferList *) malloc(sizeof(AudioBufferList) + (sizeof(AudioBuffer) * numberOfBuffers));

	auf->outBufferList->mNumberBuffers = numberOfBuffers;

	for (UInt32 i = 0; i < auf->outBufferList->mNumberBuffers; i++) {
		auf->outBufferList->mBuffers[i].mNumberChannels = mNumberChannels;
		auf->outBufferList->mBuffers[i].mDataByteSize = 4096 * sizeof(float);
		auf->outBufferList->mBuffers[i].mData = malloc(4096 * sizeof(float));
	}

    audio_unit_update(auf, settings);

    return auf;
}

static void *audio_unit_create(obs_data_t *settings, obs_source_t *source)
{
    @autoreleasepool {
        return audio_unit_create_internal(settings, source);
    }
}

static inline void audio_unit_update_internal(struct audio_unit_filter *auf, obs_data_t *settings)
{
    const char *desc_code = obs_data_get_string(settings, "audio_unit");

    if (desc_code == NULL) {
        return;
    }

    struct obs_audio_info oai;
    obs_get_audio_info(&oai);

    AudioComponentDescription desc = description_string_to_description(desc_code);

    [AUAudioUnit instantiateWithComponentDescription:desc options:kAudioComponentInstantiation_LoadOutOfProcess
                                   completionHandler:^(AUAudioUnit *_Nullable audioUnit, NSError *_Nullable error) {
                                       if (error != nil) {
                                           blog(LOG_INFO,
                                                "[audio unit: '%s'] instantiateWithComponentDescription:\n"
                                                "\terror: %s\n",
                                                obs_source_get_name(auf->source),
                                                [error.localizedDescription UTF8String]);
                                       }

                                       blog(LOG_ERROR, "loaded: %s", [audioUnit.audioUnitName UTF8String]);

                                       for (NSNumber *num in audioUnit.channelCapabilities) {
                                           blog(LOG_ERROR, "channelCapabilities: %d", num.intValue);
                                       }
	    
	    

                                       //blog(LOG_ERROR, "%d",  audioUnit.canPerformInput);
                                       //blog(LOG_ERROR, "%d", audioUnit.canPerformOutput);

                                       [audioUnit retain];

                                       auf->audioUnit = audioUnit;

                                       auf->renderBlock = audioUnit.renderBlock;

                                       [auf->renderBlock retain];

                                       auf->audioUnit.maximumFramesToRender = 2048;
	    

                                       AVAudioFormat *format = [[AVAudioFormat alloc]
                                           initWithCommonFormat:AVAudioPCMFormatFloat32
                                                     sampleRate:oai.samples_per_sec
                                                       channels:8
                                                    interleaved:NO];

                                       for (NSUInteger i = 0; i < auf->audioUnit.inputBusses.count; i++) {
                                           NSError *formatError = nil;
                                           [auf->audioUnit.inputBusses[i] setFormat:format error:&formatError];
					       
                                           if (formatError != nil) {
                                               blog(LOG_INFO,
                                                    "[audio unit: '%s'] set input format:\n"
                                                    "\terror: %s\n",
                                                    obs_source_get_name(auf->source),
                                                    [formatError.localizedDescription UTF8String]);
                                           }
                                       }

                                       auf->audioUnit.inputBusses[0].enabled = true;

                                       for (NSUInteger i = 0; i < auf->audioUnit.outputBusses.count; i++) {
                                           NSError *formatError = nil;
                                           [auf->audioUnit.outputBusses[i] setFormat:format error:&formatError];

                                           if (formatError != nil) {
                                               blog(LOG_INFO,
                                                    "[audio unit: '%s'] set output format:\n"
                                                    "\terror: %s\n",
                                                    obs_source_get_name(auf->source),
                                                    [formatError.localizedDescription UTF8String]);
                                           }
                                       }

                                       auf->audioUnit.outputBusses[0].enabled = true;
	    

                                       NSError *error3;
                                       BOOL success = [auf->audioUnit allocateRenderResourcesAndReturnError:&error3];

                                       if (!success) {
                                           /*blog(LOG_INFO,
             "[audio unit: '%s'] allocateRenderResourcesAndReturnError:\n"
             "\terror: %s\n",
             obs_source_get_name(auf->source), [error3.localizedDescription UTF8String]);*/
                                       } else {
                                           blog(LOG_INFO, "[audio unit: '%s'] Setup Audio Unit:\n",
                                                obs_source_get_name(auf->source));
                                       }

                                       /* NSError *error2;
         [auf->audioEngine startAndReturnError:&error2];
         
         blog(LOG_INFO,
         "[audio unit: '%s'] audio_unit_update_internal:\n"
         "\terror: %s\n",
         obs_source_get_name(
         auf->source),
         [error2.localizedDescription
         UTF8String]);*/
                                   }];
}

static void audio_unit_update(void *data, obs_data_t *settings)
{
    @autoreleasepool {
        return audio_unit_update_internal(data, settings);
    }
}

static void audio_unit_destroy(void *data)
{
    struct audio_unit_filter *auf = data;

    bfree(auf);
}

static struct obs_audio_data *audio_unit_filter_audio(void *data, struct obs_audio_data *audio)
{
    struct audio_unit_filter *auf = data;

    if (auf == NULL || auf->audioUnit == NULL) {
        return audio;
    }

    AudioUnitRenderActionFlags actionFlags = kAudioOfflineUnitRenderAction_Render;
	
	size_t numberOfBuffers = MAX_AUDIO_CHANNELS; //oai.speakers;
	    int mNumberChannels = 1; // 1 channel per buffer

    AudioBufferList *bufferList = (AudioBufferList *) malloc(sizeof(AudioBufferList) + (sizeof(AudioBuffer) * numberOfBuffers));

	bufferList->mNumberBuffers = numberOfBuffers;

    for (UInt32 i = 0; i < bufferList->mNumberBuffers; i++) {
        bufferList->mBuffers[i].mNumberChannels = mNumberChannels;
        bufferList->mBuffers[i].mDataByteSize = audio->frames * sizeof(float);
        bufferList->mBuffers[i].mData = malloc(audio->frames * sizeof(float));
    }

    auf->timestamp.mFlags = kAudioTimeStampSampleTimeValid;
    auf->timestamp.mSampleTime += audio->frames;

    auf->renderBlock(&actionFlags, &auf->timestamp, audio->frames, 0, bufferList,
                     ^AUAudioUnitStatus(AudioUnitRenderActionFlags *_Nonnull actionFlags,
                                        const AudioTimeStamp *_Nonnull timestamp, AUAudioFrameCount frameCount,
                                        NSInteger inputBusNumber, AudioBufferList *_Nonnull inputData) {
                         UNUSED_PARAMETER(actionFlags);
                         UNUSED_PARAMETER(timestamp);
                         UNUSED_PARAMETER(inputBusNumber);

                         for (size_t i = 0; i < numberOfBuffers; i++) {
				 inputData->mBuffers[i].mNumberChannels = mNumberChannels;
				 inputData->mBuffers[i].mDataByteSize = frameCount * sizeof(float);
                             if (audio->data[i] != NULL) {
                                 memcpy(inputData->mBuffers[i].mData, audio->data[i], frameCount * sizeof(float));
                             }
                         }

                         return noErr;
                     });

	for (size_t i = 0; i < numberOfBuffers; i++) {
		if (audio->data[i] != NULL) {
			memcpy(audio->data[i], bufferList->mBuffers[i].mData, audio->frames * sizeof(float));
		}
	}

    return audio;
}

static bool audio_unit_changed(void *data, obs_properties_t *props, obs_property_t *list, obs_data_t *settings)
{
    UNUSED_PARAMETER(data);
    UNUSED_PARAMETER(props);
    UNUSED_PARAMETER(settings);
    UNUSED_PARAMETER(list);

    return true;
}

static obs_properties_t *audio_unit_properties(void *data)
{
    obs_properties_t *props = obs_properties_create();

    AVAudioUnitComponentManager *manager = [AVAudioUnitComponentManager sharedAudioUnitComponentManager];
    NSPredicate *predicate = [NSPredicate predicateWithFormat:@"typeName CONTAINS 'Effect'"];

    NSArray<AVAudioUnitComponent *> *components = [manager componentsMatchingPredicate:predicate];

    obs_property_t *list =
        obs_properties_add_list(props, "audio_unit", "PLUG_IN_NAME", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);

    obs_property_list_add_string(list, "{Please select a plug-in}", NULL);

    for (AVAudioUnitComponent *component in components) {
        AudioComponentDescription desc = component.audioComponentDescription;

        NSString *desc_code = audio_unit_description_to_NSString(desc);

        obs_property_list_add_string(list, [component.name UTF8String], [desc_code UTF8String]);
    }

    obs_property_set_modified_callback2(list, audio_unit_changed, data);

    obs_properties_add_button(props, "show", "Show UI", open_editor_button_clicked);

    return props;
}

struct obs_source_info audio_unit_info = {
    .id = "mac_au",
    .type = OBS_SOURCE_TYPE_FILTER,
    .get_name = audio_unit_getname,

    .create = audio_unit_create,
    .destroy = audio_unit_destroy,

    .output_flags = OBS_SOURCE_AUDIO,
    .filter_audio = audio_unit_filter_audio,
    .get_properties = audio_unit_properties,
    .update = audio_unit_update,
};
