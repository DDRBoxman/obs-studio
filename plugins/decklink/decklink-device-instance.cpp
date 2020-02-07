#include "decklink-device-instance.hpp"
#include "audio-repack.hpp"

#include "DecklinkInput.hpp"
#include "DecklinkOutput.hpp"

#include <util/platform.h>
#include <util/threading.h>

#include <sstream>
#include <iomanip>
#include <algorithm>

#include "OBSVideoFrame.h"

#include <caption/caption.h>

static inline enum video_format ConvertPixelFormat(BMDPixelFormat format)
{
	switch (format) {
	case bmdFormat8BitBGRA:
		return VIDEO_FORMAT_BGRX;

	default:
	case bmdFormat8BitYUV:;
	}

	return VIDEO_FORMAT_UYVY;
}

static inline int ConvertChannelFormat(speaker_layout format)
{
	switch (format) {
	case SPEAKERS_2POINT1:
	case SPEAKERS_4POINT0:
	case SPEAKERS_4POINT1:
	case SPEAKERS_5POINT1:
	case SPEAKERS_7POINT1:
		return 8;

	default:
	case SPEAKERS_STEREO:
		return 2;
	}
}

static inline audio_repack_mode_t ConvertRepackFormat(speaker_layout format,
						      bool swap)
{
	switch (format) {
	case SPEAKERS_2POINT1:
		return repack_mode_8to3ch;
	case SPEAKERS_4POINT0:
		return repack_mode_8to4ch;
	case SPEAKERS_4POINT1:
		return swap ? repack_mode_8to5ch_swap : repack_mode_8to5ch;
	case SPEAKERS_5POINT1:
		return swap ? repack_mode_8to6ch_swap : repack_mode_8to6ch;
	case SPEAKERS_7POINT1:
		return swap ? repack_mode_8ch_swap : repack_mode_8ch;
	default:
		assert(false && "No repack requested");
		return (audio_repack_mode_t)-1;
	}
}

DeckLinkDeviceInstance::DeckLinkDeviceInstance(DecklinkBase *decklink_,
					       DeckLinkDevice *device_)
	: currentFrame(), currentPacket(), decklink(decklink_), device(device_)
{
	currentPacket.samples_per_sec = 48000;
	currentPacket.speakers = SPEAKERS_STEREO;
	currentPacket.format = AUDIO_FORMAT_16BIT;
}

DeckLinkDeviceInstance::~DeckLinkDeviceInstance() {}

void DeckLinkDeviceInstance::HandleAudioPacket(
	IDeckLinkAudioInputPacket *audioPacket, const uint64_t timestamp)
{
	if (audioPacket == nullptr)
		return;

	void *bytes;
	if (audioPacket->GetBytes(&bytes) != S_OK) {
		LOG(LOG_WARNING, "Failed to get audio packet data");
		return;
	}

	const uint32_t frameCount =
		(uint32_t)audioPacket->GetSampleFrameCount();
	currentPacket.frames = frameCount;
	currentPacket.timestamp = timestamp;

	if (decklink && !static_cast<DeckLinkInput *>(decklink)->buffering) {
		currentPacket.timestamp = os_gettime_ns();
		currentPacket.timestamp -=
			(uint64_t)frameCount * 1000000000ULL /
			(uint64_t)currentPacket.samples_per_sec;
	}

	int maxdevicechannel = device->GetMaxChannel();

	if (channelFormat != SPEAKERS_UNKNOWN &&
	    channelFormat != SPEAKERS_MONO &&
	    channelFormat != SPEAKERS_STEREO &&
	    (channelFormat != SPEAKERS_7POINT1 ||
	     static_cast<DeckLinkInput *>(decklink)->swap) &&
	    maxdevicechannel >= 8) {

		if (audioRepacker->repack((uint8_t *)bytes, frameCount) < 0) {
			LOG(LOG_ERROR, "Failed to convert audio packet data");
			return;
		}
		currentPacket.data[0] = (*audioRepacker)->packet_buffer;
	} else {
		currentPacket.data[0] = (uint8_t *)bytes;
	}

	nextAudioTS = timestamp +
		      ((uint64_t)frameCount * 1000000000ULL / 48000ULL) + 1;

	obs_source_output_audio(
		static_cast<DeckLinkInput *>(decklink)->GetSource(),
		&currentPacket);
}

uint8_t pos;
uint8_t subPos = 0x80;

static inline uint8_t readBit(uint8_t *buf) {
    auto bit = (*(buf + pos) & subPos) == subPos ? 1 : 0;

    subPos >>= 0x1;
    if (subPos == 0) {
        subPos = 0x80;
        pos++;
    }

    return bit;
}

static inline uint8_t readBits(uint8_t *buf, int bits) {
    uint8_t res = 0;

    for (int i = 1; i<=bits; i++) {
        res <<= 1;
        res |= readBit(buf);
    }

    return res;
}

double captionFrame = 0;
caption_frame_t frame;

void DeckLinkDeviceInstance::HandleVideoFrame(
	IDeckLinkVideoInputFrame *videoFrame, const uint64_t timestamp)
{
	if (videoFrame == nullptr)
		return;

	IDeckLinkVideoFrameAncillaryPackets *packets;

	if (videoFrame->QueryInterface(IID_IDeckLinkVideoFrameAncillaryPackets, (void**) &packets) == S_OK) {
		IDeckLinkAncillaryPacketIterator *iterator;
		packets->GetPacketIterator(&iterator);

		IDeckLinkAncillaryPacket *packet;
		iterator->Next(&packet);

		if (packet) {

			auto did = packet->GetDID();

			//blog(LOG_ERROR, "did: %x", did);

			auto sdid = packet->GetSDID();

			//blog(LOG_ERROR, "did: %x", sdid);

			// Caption data
			if (did == 0x61 & sdid == 0x01) {
                auto line = packet->GetLineNumber();

                //blog(LOG_ERROR, "line: %d", line);

                const void *data;
                uint32_t size;
                packet->GetBytes(bmdAncillaryPacketFormatUInt8, &data, &size);

                //blog(LOG_ERROR, "size: %d", size);

                //blog(LOG_ERROR, "data: %s", data);

                /*std::ostringstream os;

                os << std::hex << std::setfill('0');

                for (int i=0; i<size; i++) {
                    os << std::hex << std::setw(2) << static_cast<int>(((uint8_t *) data)[i]) << ",";
                }

               // blog(LOG_ERROR, "data %s", os.str().c_str());*/

                auto anc = (uint8_t *) data;

                pos = 0;
                subPos = 0x80;
                auto header1 = readBits(anc, 8);
                auto header2 = readBits(anc, 8);

                uint8_t length = readBits(anc, 8);
                uint8_t frameRate = readBits(anc, 4);
                //reserved
                readBits(anc, 4);

                auto cdp_timecode_added = readBits(anc, 1);
                auto cdp_data_block_added = readBits(anc, 1);
                auto cdp_service_info_added = readBits(anc, 1);
                auto cdp_service_info_start = readBits(anc, 1);
                auto cdp_service_info_changed = readBits(anc, 1);
                auto cdp_service_info_end = readBits(anc, 1);
                auto cdp_contains_captions = readBits(anc, 1);
                //reserved
                readBits(anc, 1);

                auto cdp_counter = readBits(anc, 8);
                auto cdp_counter2 = readBits(anc, 8);

                if (cdp_timecode_added) {
                    auto timecodeSectionID = readBits(anc, 8);
                    //reserved
                    readBits(anc, 2);
                    readBits(anc, 2);
                    readBits(anc, 4);
                    // reserved
                    readBits(anc, 1);
                    readBits(anc, 3);
                    readBits(anc, 4);
                    readBits(anc, 1);
                    readBits(anc, 3);
                    readBits(anc, 4);
                    readBits(anc, 1);
                    readBits(anc, 1);
                    readBits(anc, 3);
                    readBits(anc, 4);
                }

                if (cdp_contains_captions) {
                    auto cdp_data_section = readBits(anc, 8);
                    // 4 flags

                    // 5,6 ?? cdp_counter

                    // 7 * length

                    //

                    // 10 105 114
                   // blog(LOG_ERROR, "sanity %d %d %d", header1, header2, cdp_data_section);

                   // blog(LOG_ERROR, "contains captions %d", cdp_contains_captions);

                    auto process_em_data_flag = readBits(anc, 1);
                    auto process_cc_data_flag = readBits(anc, 1);
                    auto additional_data_flag = readBits(anc, 1);

                    auto cc_count = readBits(anc, 5);

                   /*blog(LOG_ERROR,
                         "em: %d cc: %d add: %d",
                         process_em_data_flag,
                         process_cc_data_flag,
                         additional_data_flag);*/

                   // blog(LOG_ERROR, "cc_count %d", cc_count);

                    for (int i=0; i<cc_count; i++) {
                        readBits(anc, 5);
                        auto valid = readBits(anc, 1);
                        auto type = readBits(anc, 2);
                        auto cc_data1 = readBits(anc, 8);
                        auto cc_data2 = readBits(anc, 8);

                        //NTSC_CC_FIELD_1 = 0, NTSC_CC_FIELD_2 = 1, DTVCC_PACKET_DATA = 2, DTVCC_PACKET_START = 3

                        if (valid && type == 0) {


                            //caption_frame_decode(&frame, cc_data, cea708->timestamp);
                            auto cc_data = ((uint16_t)cc_data1 << 8) | cc_data2;
                            //eia608_dump(cc_data);
                            //caption_frame_decode(&frame, cc_data, captionFrame++/30.0);


                            if (LIBCAPTION_READY == caption_frame_decode(&frame,cc_data, captionFrame++/30.0)) {
                                caption_frame_dump(&frame);
                            }

                            //caption_frame_dump(&frame);
                        }

                       // blog(LOG_ERROR, "cc_type %d", type);
                    }
                }

			}

			packet->Release();
		}

		iterator->Release();
        packets->Release();
	}



	IDeckLinkVideoConversion *frameConverter = CreateVideoConversionInstance();

	IDeckLinkMutableVideoFrame *newFrame = new OBSVideoFrame(videoFrame->GetWidth(),
			videoFrame->GetHeight());

	frameConverter->ConvertFrame(videoFrame, newFrame);

	void *bytes;
	if (newFrame->GetBytes(&bytes) != S_OK) {
		LOG(LOG_WARNING, "Failed to get video frame data");
		return;
	}

	currentFrame.data[0] = (uint8_t *)bytes;
	currentFrame.linesize[0] = (uint32_t)newFrame->GetRowBytes();
	currentFrame.width = (uint32_t)newFrame->GetWidth();
	currentFrame.height = (uint32_t)newFrame->GetHeight();
	currentFrame.timestamp = timestamp;

	obs_source_output_video2(
		static_cast<DeckLinkInput *>(decklink)->GetSource(),
		&currentFrame);
}

void DeckLinkDeviceInstance::FinalizeStream()
{
	input->SetCallback(nullptr);
	input->DisableVideoInput();
	if (channelFormat != SPEAKERS_UNKNOWN)
		input->DisableAudioInput();

	if (audioRepacker != nullptr) {
		delete audioRepacker;
		audioRepacker = nullptr;
	}

	mode = nullptr;
}

//#define LOG_SETUP_VIDEO_FORMAT 1

void DeckLinkDeviceInstance::SetupVideoFormat(DeckLinkDeviceMode *mode_)
{
	if (mode_ == nullptr)
		return;

	currentFrame.format = ConvertPixelFormat(pixelFormat);

	colorSpace = static_cast<DeckLinkInput *>(decklink)->GetColorSpace();
	if (colorSpace == VIDEO_CS_DEFAULT) {
		const BMDDisplayModeFlags flags = mode_->GetDisplayModeFlags();
		if (flags & bmdDisplayModeColorspaceRec709)
			activeColorSpace = VIDEO_CS_709;
		else if (flags & bmdDisplayModeColorspaceRec601)
			activeColorSpace = VIDEO_CS_601;
		else
			activeColorSpace = VIDEO_CS_DEFAULT;
	} else {
		activeColorSpace = colorSpace;
	}

	colorRange = static_cast<DeckLinkInput *>(decklink)->GetColorRange();
	currentFrame.range = colorRange;

	video_format_get_parameters(activeColorSpace, colorRange,
				    currentFrame.color_matrix,
				    currentFrame.color_range_min,
				    currentFrame.color_range_max);

//#ifdef LOG_SETUP_VIDEO_FORMAT
	LOG(LOG_INFO, "Setup video format: %s, %s, %s",
	    pixelFormat == bmdFormat10BitYUV ? "YUV" : "RGB",
	    activeColorSpace == VIDEO_CS_709 ? "BT.709" : "BT.601",
	    colorRange == VIDEO_RANGE_FULL ? "full" : "limited");
//#endif
}

bool DeckLinkDeviceInstance::StartCapture(DeckLinkDeviceMode *mode_,
					  BMDVideoConnection bmdVideoConnection,
					  BMDAudioConnection bmdAudioConnection)
{
	if (mode != nullptr)
		return false;
	if (mode_ == nullptr)
		return false;

    caption_frame_init(&frame);

	LOG(LOG_INFO, "Starting capture...");

	if (!device->GetInput(&input))
		return false;

	IDeckLinkConfiguration *deckLinkConfiguration = NULL;
	HRESULT result = input->QueryInterface(IID_IDeckLinkConfiguration,
					       (void **)&deckLinkConfiguration);
	if (result != S_OK) {
		LOG(LOG_ERROR,
		    "Could not obtain the IDeckLinkConfiguration interface: %08x\n",
		    result);
	} else {
		if (bmdVideoConnection > 0) {
			result = deckLinkConfiguration->SetInt(
				bmdDeckLinkConfigVideoInputConnection,
				bmdVideoConnection);
			if (result != S_OK) {
				LOG(LOG_ERROR,
				    "Couldn't set input video port to %d\n\n",
				    bmdVideoConnection);
			}
		}

		if (bmdAudioConnection > 0) {
			result = deckLinkConfiguration->SetInt(
				bmdDeckLinkConfigAudioInputConnection,
				bmdAudioConnection);
			if (result != S_OK) {
				LOG(LOG_ERROR,
				    "Couldn't set input audio port to %d\n\n",
				    bmdVideoConnection);
			}
		}
	}

	videoConnection = bmdVideoConnection;
	audioConnection = bmdAudioConnection;

	BMDVideoInputFlags flags;

	bool isauto = mode_->GetName() == "Auto";
	if (isauto) {
		displayMode = bmdModeNTSC;
		pixelFormat = bmdFormat10BitYUV;
		flags = bmdVideoInputEnableFormatDetection;
	} else {
		displayMode = mode_->GetDisplayMode();
		pixelFormat =
			static_cast<DeckLinkInput *>(decklink)->GetPixelFormat();
		flags = bmdVideoInputFlagDefault;
	}

	const HRESULT videoResult =
		input->EnableVideoInput(displayMode, pixelFormat, flags);
	if (videoResult != S_OK) {
		LOG(LOG_ERROR, "Failed to enable video input");
		return false;
	}

	SetupVideoFormat(mode_);

	channelFormat =
		static_cast<DeckLinkInput *>(decklink)->GetChannelFormat();
	currentPacket.speakers = channelFormat;
	swap = static_cast<DeckLinkInput *>(decklink)->swap;

	int maxdevicechannel = device->GetMaxChannel();

	if (channelFormat != SPEAKERS_UNKNOWN) {
		const int channel = ConvertChannelFormat(channelFormat);
		const HRESULT audioResult = input->EnableAudioInput(
			bmdAudioSampleRate48kHz, bmdAudioSampleType16bitInteger,
			channel);
		if (audioResult != S_OK)
			LOG(LOG_WARNING,
			    "Failed to enable audio input; continuing...");

		if (channelFormat != SPEAKERS_UNKNOWN &&
		    channelFormat != SPEAKERS_MONO &&
		    channelFormat != SPEAKERS_STEREO &&
		    (channelFormat != SPEAKERS_7POINT1 || swap) &&
		    maxdevicechannel >= 8) {

			const audio_repack_mode_t repack_mode =
				ConvertRepackFormat(channelFormat, swap);
			audioRepacker = new AudioRepacker(repack_mode);
		}
	}

	if (input->SetCallback(this) != S_OK) {
		LOG(LOG_ERROR, "Failed to set callback");
		FinalizeStream();
		return false;
	}

	if (input->StartStreams() != S_OK) {
		LOG(LOG_ERROR, "Failed to start streams");
		FinalizeStream();
		return false;
	}

	mode = mode_;

	return true;
}

bool DeckLinkDeviceInstance::StopCapture(void)
{
	if (mode == nullptr || input == nullptr)
		return false;

	LOG(LOG_INFO, "Stopping capture of '%s'...",
	    GetDevice()->GetDisplayName().c_str());

	input->StopStreams();
	FinalizeStream();

	return true;
}

bool DeckLinkDeviceInstance::StartOutput(DeckLinkDeviceMode *mode_)
{
	if (mode != nullptr)
		return false;
	if (mode_ == nullptr)
		return false;

	LOG(LOG_INFO, "Starting output...");

	if (!device->GetOutput(&output))
		return false;

	const HRESULT videoResult = output->EnableVideoOutput(
		mode_->GetDisplayMode(), bmdVideoOutputFlagDefault);
	if (videoResult != S_OK) {
		LOG(LOG_ERROR, "Failed to enable video output");
		return false;
	}

	const HRESULT audioResult = output->EnableAudioOutput(
		bmdAudioSampleRate48kHz, bmdAudioSampleType16bitInteger, 2,
		bmdAudioOutputStreamTimestamped);
	if (audioResult != S_OK) {
		LOG(LOG_ERROR, "Failed to enable audio output");
		return false;
	}

	mode = mode_;

	int keyerMode = device->GetKeyerMode();

	IDeckLinkKeyer *deckLinkKeyer = nullptr;
	if (device->GetKeyer(&deckLinkKeyer)) {
		if (keyerMode) {
			deckLinkKeyer->Enable(keyerMode == 1);
			deckLinkKeyer->SetLevel(255);
		} else {
			deckLinkKeyer->Disable();
		}
	}

	auto decklinkOutput = dynamic_cast<DeckLinkOutput *>(decklink);
	if (decklinkOutput == nullptr)
		return false;

	int rowBytes = decklinkOutput->GetWidth() * 2;
	if (decklinkOutput->keyerMode != 0) {
		rowBytes = decklinkOutput->GetWidth() * 4;
	}

	BMDPixelFormat pixelFormat = bmdFormat8BitYUV;
	if (keyerMode != 0) {
		pixelFormat = bmdFormat8BitBGRA;
	}

	HRESULT result;
	result = output->CreateVideoFrame(decklinkOutput->GetWidth(),
					  decklinkOutput->GetHeight(), rowBytes,
					  pixelFormat, bmdFrameFlagDefault,
					  &decklinkOutputFrame);
	if (result != S_OK) {
		blog(LOG_ERROR, "failed to make frame 0x%X", result);
		return false;
	}

	return true;
}

bool DeckLinkDeviceInstance::StopOutput()
{
	if (mode == nullptr || output == nullptr)
		return false;

	LOG(LOG_INFO, "Stopping output of '%s'...",
	    GetDevice()->GetDisplayName().c_str());

	output->DisableVideoOutput();
	output->DisableAudioOutput();

	if (decklinkOutputFrame != nullptr) {
		decklinkOutputFrame->Release();
		decklinkOutputFrame = nullptr;
	}

	return true;
}

void DeckLinkDeviceInstance::DisplayVideoFrame(video_data *frame)
{
	auto decklinkOutput = dynamic_cast<DeckLinkOutput *>(decklink);
	if (decklinkOutput == nullptr)
		return;

	uint8_t *destData;
	decklinkOutputFrame->GetBytes((void **)&destData);

	uint8_t *outData = frame->data[0];

	int rowBytes = decklinkOutput->GetWidth() * 2;
	if (device->GetKeyerMode()) {
		rowBytes = decklinkOutput->GetWidth() * 4;
	}

	std::copy(outData, outData + (decklinkOutput->GetHeight() * rowBytes),
		  destData);

	output->DisplayVideoFrameSync(decklinkOutputFrame);
}

void DeckLinkDeviceInstance::WriteAudio(audio_data *frames)
{
	uint32_t sampleFramesWritten;
	output->WriteAudioSamplesSync(frames->data[0], frames->frames,
				      &sampleFramesWritten);
}

#define TIME_BASE 1000000000

HRESULT STDMETHODCALLTYPE DeckLinkDeviceInstance::VideoInputFrameArrived(
	IDeckLinkVideoInputFrame *videoFrame,
	IDeckLinkAudioInputPacket *audioPacket)
{
	BMDTimeValue videoTS = 0;
	BMDTimeValue videoDur = 0;
	BMDTimeValue audioTS = 0;

	if (videoFrame) {
		videoFrame->GetStreamTime(&videoTS, &videoDur, TIME_BASE);
		lastVideoTS = (uint64_t)videoTS;
	}
	if (audioPacket) {
		BMDTimeValue newAudioTS = 0;
		int64_t diff;

		audioPacket->GetPacketTime(&newAudioTS, TIME_BASE);
		audioTS = newAudioTS + audioOffset;

		diff = (int64_t)audioTS - (int64_t)nextAudioTS;
		if (diff > 10000000LL) {
			audioOffset -= diff;
			audioTS = newAudioTS + audioOffset;

		} else if (diff < -1000000) {
			audioOffset = 0;
			audioTS = newAudioTS;
		}
	}

	if (videoFrame && videoTS >= 0)
		HandleVideoFrame(videoFrame, (uint64_t)videoTS);
	if (audioPacket && audioTS >= 0)
		HandleAudioPacket(audioPacket, (uint64_t)audioTS);

	return S_OK;
}

HRESULT STDMETHODCALLTYPE DeckLinkDeviceInstance::VideoInputFormatChanged(
	BMDVideoInputFormatChangedEvents events, IDeckLinkDisplayMode *newMode,
	BMDDetectedVideoInputFormatFlags detectedSignalFlags)
{
	input->PauseStreams();

	mode->SetMode(newMode);

	if (events & bmdVideoInputDisplayModeChanged) {
		displayMode = mode->GetDisplayMode();
	}

	if (events & bmdVideoInputColorspaceChanged) {
		switch (detectedSignalFlags) {
		case bmdDetectedVideoInputRGB444:
			pixelFormat = bmdFormat8BitBGRA;
			break;

		default:
		case bmdDetectedVideoInputYCbCr422:
			pixelFormat = bmdFormat10BitYUV;
			break;
		}
	}

	const HRESULT videoResult = input->EnableVideoInput(
		displayMode, pixelFormat, bmdVideoInputEnableFormatDetection);
	if (videoResult != S_OK) {
		LOG(LOG_ERROR, "Failed to enable video input");
		input->StopStreams();
		FinalizeStream();

		return E_FAIL;
	}

	SetupVideoFormat(mode);

	input->FlushStreams();
	input->StartStreams();

	return S_OK;
}

ULONG STDMETHODCALLTYPE DeckLinkDeviceInstance::AddRef(void)
{
	return os_atomic_inc_long(&refCount);
}

HRESULT STDMETHODCALLTYPE DeckLinkDeviceInstance::QueryInterface(REFIID iid,
								 LPVOID *ppv)
{
	HRESULT result = E_NOINTERFACE;

	*ppv = nullptr;

	CFUUIDBytes unknown = CFUUIDGetUUIDBytes(IUnknownUUID);
	if (memcmp(&iid, &unknown, sizeof(REFIID)) == 0) {
		*ppv = this;
		AddRef();
		result = S_OK;
	} else if (memcmp(&iid, &IID_IDeckLinkNotificationCallback,
			  sizeof(REFIID)) == 0) {
		*ppv = (IDeckLinkNotificationCallback *)this;
		AddRef();
		result = S_OK;
	}

	return result;
}

ULONG STDMETHODCALLTYPE DeckLinkDeviceInstance::Release(void)
{
	const long newRefCount = os_atomic_dec_long(&refCount);
	if (newRefCount == 0) {
		delete this;
		return 0;
	}

	return newRefCount;
}
