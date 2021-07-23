#include <string>
#include <vector>
#include <algorithm>
#include <QMessageBox>
#include "qt-wrappers.hpp"
#include "audio-encoders.hpp"
#include "window-basic-main.hpp"
#include "window-basic-main-outputs.hpp"

using namespace std;

extern bool EncoderAvailable(const char *encoder);

volatile bool streaming_active = false;
volatile bool recording_active = false;
volatile bool recording_paused = false;
volatile bool replaybuf_active = false;
volatile bool virtualcam_active = false;

#define FTL_PROTOCOL "ftl"
#define RTMP_PROTOCOL "rtmp"

static void OBSStreamStarting(void *data, calldata_t *params)
{
	BasicOutputHandler *output = static_cast<BasicOutputHandler *>(data);
	OBSOutput obj = (obs_output_t *)calldata_ptr(params, "output");

	int sec = (int)obs_output_get_active_delay(obj);
	if (sec == 0)
		return;

	output->delayActive = true;
	QMetaObject::invokeMethod(output->main, "StreamDelayStarting",
				  Q_ARG(OBSOutput, obj));
}

static void OBSStreamStopping(void *data, calldata_t *params)
{
	BasicOutputHandler *output = static_cast<BasicOutputHandler *>(data);
	obs_output_t *obj = (obs_output_t *)calldata_ptr(params, "output");

	int sec = (int)obs_output_get_active_delay(obj);
	if (sec == 0)
		QMetaObject::invokeMethod(output->main, "StreamStopping");
	else
		QMetaObject::invokeMethod(output->main, "StreamDelayStopping",
					  Q_ARG(OBSOutput, obj));
}

static void OBSStartStreaming(void *data, calldata_t *params)
{
	BasicOutputHandler *output = static_cast<BasicOutputHandler *>(data);
	output->streamingActive = true;
	os_atomic_set_bool(&streaming_active, true);
	QMetaObject::invokeMethod(output->main, "StreamingStart");

	UNUSED_PARAMETER(params);
}

static void OBSStopStreaming(void *data, calldata_t *params)
{
	BasicOutputHandler *output = static_cast<BasicOutputHandler *>(data);
	int code = (int)calldata_int(params, "code");
	const char *last_error = calldata_string(params, "last_error");

	QString arg_last_error = QString::fromUtf8(last_error);

	output->streamingActive = false;
	output->delayActive = false;
	os_atomic_set_bool(&streaming_active, false);
	QMetaObject::invokeMethod(output->main, "StreamingStop",
				  Q_ARG(int, code),
				  Q_ARG(QString, arg_last_error));
}

static void OBSStartRecording(void *data, calldata_t *params)
{
	BasicOutputHandler *output = static_cast<BasicOutputHandler *>(data);

	output->recordingActive = true;
	os_atomic_set_bool(&recording_active, true);
	QMetaObject::invokeMethod(output->main, "RecordingStart");

	UNUSED_PARAMETER(params);
}

static void OBSStopRecording(void *data, calldata_t *params)
{
	BasicOutputHandler *output = static_cast<BasicOutputHandler *>(data);
	int code = (int)calldata_int(params, "code");
	const char *last_error = calldata_string(params, "last_error");

	QString arg_last_error = QString::fromUtf8(last_error);

	output->recordingActive = false;
	os_atomic_set_bool(&recording_active, false);
	os_atomic_set_bool(&recording_paused, false);
	QMetaObject::invokeMethod(output->main, "RecordingStop",
				  Q_ARG(int, code),
				  Q_ARG(QString, arg_last_error));

	UNUSED_PARAMETER(params);
}

static void OBSRecordStopping(void *data, calldata_t *params)
{
	BasicOutputHandler *output = static_cast<BasicOutputHandler *>(data);
	QMetaObject::invokeMethod(output->main, "RecordStopping");

	UNUSED_PARAMETER(params);
}

static void OBSStartReplayBuffer(void *data, calldata_t *params)
{
	BasicOutputHandler *output = static_cast<BasicOutputHandler *>(data);

	output->replayBufferActive = true;
	os_atomic_set_bool(&replaybuf_active, true);
	QMetaObject::invokeMethod(output->main, "ReplayBufferStart");

	UNUSED_PARAMETER(params);
}

static void OBSStopReplayBuffer(void *data, calldata_t *params)
{
	BasicOutputHandler *output = static_cast<BasicOutputHandler *>(data);
	int code = (int)calldata_int(params, "code");

	output->replayBufferActive = false;
	os_atomic_set_bool(&replaybuf_active, false);
	QMetaObject::invokeMethod(output->main, "ReplayBufferStop",
				  Q_ARG(int, code));

	UNUSED_PARAMETER(params);
}

static void OBSReplayBufferStopping(void *data, calldata_t *params)
{
	BasicOutputHandler *output = static_cast<BasicOutputHandler *>(data);
	QMetaObject::invokeMethod(output->main, "ReplayBufferStopping");

	UNUSED_PARAMETER(params);
}

static void OBSReplayBufferSaved(void *data, calldata_t *)
{
	BasicOutputHandler *output = static_cast<BasicOutputHandler *>(data);
	QMetaObject::invokeMethod(output->main, "ReplayBufferSaved",
				  Qt::QueuedConnection);
}

static void OBSStartVirtualCam(void *data, calldata_t *params)
{
	BasicOutputHandler *output = static_cast<BasicOutputHandler *>(data);

	output->virtualCamActive = true;
	os_atomic_set_bool(&virtualcam_active, true);
	QMetaObject::invokeMethod(output->main, "OnVirtualCamStart");

	UNUSED_PARAMETER(params);
}

static void OBSStopVirtualCam(void *data, calldata_t *params)
{
	BasicOutputHandler *output = static_cast<BasicOutputHandler *>(data);
	int code = (int)calldata_int(params, "code");

	output->virtualCamActive = false;
	os_atomic_set_bool(&virtualcam_active, false);
	QMetaObject::invokeMethod(output->main, "OnVirtualCamStop",
				  Q_ARG(int, code));

	UNUSED_PARAMETER(params);
}

/* ------------------------------------------------------------------------ */

static bool CreateAACEncoder(OBSEncoder &res, string &id, int bitrate,
			     const char *name, size_t idx)
{
	const char *id_ = GetAACEncoderForBitrate(bitrate);
	if (!id_) {
		id.clear();
		res = nullptr;
		return false;
	}

	if (id == id_)
		return true;

	id = id_;
	res = obs_audio_encoder_create(id_, name, nullptr, idx, nullptr);

	if (res) {
		obs_encoder_release(res);
		return true;
	}

	return false;
}

/* ------------------------------------------------------------------------ */

void BasicOutputHandler::DisconnectSignals()
{
	streamDelayStarting.Disconnect();
	streamStopping.Disconnect();
	startStreaming.Disconnect();
	stopStreaming.Disconnect();
}

void BasicOutputHandler::ConnectToSignals(const OBSOutput &output)
{
	streamDelayStarting.Connect(obs_output_get_signal_handler(output),
				    "starting", OBSStreamStarting, this);
	streamStopping.Connect(obs_output_get_signal_handler(output),
			       "stopping", OBSStreamStopping, this);

	startStreaming.Connect(obs_output_get_signal_handler(output), "start",
			       OBSStartStreaming, this);
	stopStreaming.Connect(obs_output_get_signal_handler(output), "stop",
			      OBSStopStreaming, this);
}

void BasicOutputHandler::SetStreamOutputConfig()
{
	const char *bindIP =
		config_get_string(main->Config(), "Output", "BindIP");
	bool enableNewSocketLoop = config_get_bool(main->Config(), "Output",
						   "NewSocketLoopEnable");
	bool enableLowLatencyMode =
		config_get_bool(main->Config(), "Output", "LowLatencyEnable");
	bool enableDynBitrate =
		config_get_bool(main->Config(), "Output", "DynamicBitrate");

	for (auto &output : streamOutputs) {
		obs_data_t *settings = obs_data_create();
		obs_data_set_string(settings, "bind_ip", bindIP);
		obs_data_set_bool(settings, "new_socket_loop_enabled",
				  enableNewSocketLoop);
		obs_data_set_bool(settings, "low_latency_mode_enabled",
				  enableLowLatencyMode);
		obs_data_set_bool(settings, "dyn_bitrate", enableDynBitrate);
		obs_output_update(output, settings);

		obs_data_release(settings);
	}
}

bool BasicOutputHandler::StartStreamOutputs()
{
	bool reconnect = config_get_bool(main->Config(), "Output", "Reconnect");
	int retryDelay =
		config_get_uint(main->Config(), "Output", "RetryDelay");
	int maxRetries =
		config_get_uint(main->Config(), "Output", "MaxRetries");
	bool useDelay =
		config_get_bool(main->Config(), "Output", "DelayEnable");
	int delaySec = config_get_int(main->Config(), "Output", "DelaySec");
	bool preserveDelay =
		config_get_bool(main->Config(), "Output", "DelayPreserve");

	if (!reconnect)
		maxRetries = 0;

	return StartStreamOutputs(retryDelay, maxRetries, useDelay, delaySec,
				  preserveDelay);
}

bool BasicOutputHandler::StartStreamOutputs(int retryDelay, int maxRetries,
					    bool useDelay, int delaySec,
					    int preserveDelay)
{
	int errors = 0;

	for (auto &output : streamOutputs) {
		obs_output_set_delay(output, useDelay ? delaySec : 0,
				     preserveDelay ? OBS_OUTPUT_DELAY_PRESERVE
						   : 0);
		obs_output_set_reconnect_settings(output, maxRetries,
						  retryDelay);

		obs_output_start(output);

		const char *error = obs_output_get_last_error(output);
		bool hasLastError = error && *error;

		if (hasLastError) {
			if (hasLastError)
				lastError = error;
			else
				lastError = string();

			const char *type = obs_service_get_type(
				obs_output_get_service(output));
			blog(LOG_WARNING,
			     "Stream output type '%s' failed to start!%s%s",
			     type, hasLastError ? "  Last Error: " : "",
			     hasLastError ? error : "");

			errors++;
		}
	}

	return errors == 0;
}

/* ------------------------------------------------------------------------ */

inline BasicOutputHandler::BasicOutputHandler(OBSBasic *main_) : main(main_)
{
	if (main->vcamEnabled) {
		virtualCam = obs_output_create("virtualcam_output",
					       "virtualcam_output", nullptr,
					       nullptr);
		obs_output_release(virtualCam);

		signal_handler_t *signal =
			obs_output_get_signal_handler(virtualCam);
		startVirtualCam.Connect(signal, "start", OBSStartVirtualCam,
					this);
		stopVirtualCam.Connect(signal, "stop", OBSStopVirtualCam, this);
	}
}

bool BasicOutputHandler::StartVirtualCam()
{
	if (main->vcamEnabled) {
		obs_output_set_media(virtualCam, obs_get_video(),
				     obs_get_audio());
		if (!Active())
			SetupOutputs(main->GetStreamOutputSettings());

		return obs_output_start(virtualCam);
	}
	return false;
}

void BasicOutputHandler::StopVirtualCam()
{
	if (main->vcamEnabled) {
		obs_output_stop(virtualCam);
	}
}

bool BasicOutputHandler::VirtualCamActive() const
{
	if (main->vcamEnabled) {
		return obs_output_active(virtualCam);
	}
	return false;
}

/* ------------------------------------------------------------------------ */

struct SimpleOutput : BasicOutputHandler {
	std::map<int, struct Encoders> streamingEncoders;
	OBSEncoder aacRecording;
	OBSEncoder aacArchive;
	OBSEncoder h264Recording;

	string aacRecEncID;
	string aacArchiveEncID;

	string videoEncoder;
	string videoQuality;
	bool usingRecordingPreset = false;
	bool recordingConfigured = false;
	bool ffmpegOutput = false;
	bool lowCPUx264 = false;

	SimpleOutput(OBSBasic *main_,
		     const std::map<int, OBSData> &outputConfigs);

	int CalcCRF(int crf);

	void UpdateStreamingSettings_amd(obs_data_t *settings, int bitrate);
	void UpdateRecordingSettings_x264_crf(int crf);
	void UpdateRecordingSettings_qsv11(int crf);
	void UpdateRecordingSettings_nvenc(int cqp);
	void UpdateRecordingSettings_amd_cqp(int cqp);
	void UpdateRecordingSettings();
	void UpdateRecordingAudioSettings();
	virtual void
	Update(const std::vector<OBSService> &services,
	       const std::map<int, OBSData> &outputConfigs) override;
	virtual void
	UpdateServiceEncoders(const std::vector<OBSService> &services);

	void SetupOutputs(const std::map<int, OBSData> &outputConfigs);
	int GetAudioBitrate() const;

	void LoadRecordingPreset_h264(const char *encoder);
	void LoadRecordingPreset_Lossless();
	void LoadRecordingPreset();

	OBSEncoder LoadStreamingPreset(const char *encoder, const char *name);
	virtual OBSEncoder CreateVideoEncoder(const char *encoderType,
					      const char *name);
	virtual void CreateRecordingOutputs();

	void UpdateRecording();
	bool ConfigureRecording(bool useReplayBuffer);

	void SetupVodTrack(obs_service_t *service);

	virtual bool SetupStreaming(const std::vector<OBSService> &services,
				  const std::map<int, OBSData> &outputConfigs) override;
	virtual bool StartStreaming() override;
	virtual bool StartRecording() override;
	virtual bool StartReplayBuffer() override;
	virtual void StopStreaming(bool force) override;
	virtual void StopRecording(bool force) override;
	virtual void StopReplayBuffer(bool force) override;
	virtual bool StreamingActive() const override;
	virtual bool RecordingActive() const override;
	virtual bool ReplayBufferActive() const override;

	virtual bool UpdateOutputEncoders(const OBSOutput &output,
					  struct Encoders &encoders);
};

void SimpleOutput::LoadRecordingPreset_Lossless()
{
	fileOutput = obs_output_create("ffmpeg_output", "simple_ffmpeg_output",
				       nullptr, nullptr);
	if (!fileOutput)
		throw "Failed to create recording FFmpeg output "
		      "(simple output)";
	obs_output_release(fileOutput);

	obs_data_t *settings = obs_data_create();
	obs_data_set_string(settings, "format_name", "avi");
	obs_data_set_string(settings, "video_encoder", "utvideo");
	obs_data_set_string(settings, "audio_encoder", "pcm_s16le");

	int aMixes = 1;
	obs_output_set_mixers(fileOutput, aMixes);
	obs_output_update(fileOutput, settings);
	obs_data_release(settings);
}

void SimpleOutput::LoadRecordingPreset_h264(const char *encoderId)
{
	h264Recording = obs_video_encoder_create(
		encoderId, "simple_h264_recording", nullptr, nullptr);
	if (!h264Recording)
		throw "Failed to create h264 recording encoder (simple output)";
	obs_encoder_release(h264Recording);
}

OBSEncoder SimpleOutput::LoadStreamingPreset(const char *encoderId,
					     const char *name)
{
	char encoderName[128];
	sprintf(encoderName, "%s.%s", name, encoderId);

	OBSEncoder videoEncoder = obs_video_encoder_create(
		encoderId, encoderName, nullptr, nullptr);
	if (!(obs_encoder_t *)videoEncoder)
		throw "Failed to create h264 streaming encoder (simple output)";
	obs_encoder_release(videoEncoder);

	return videoEncoder;
}

void SimpleOutput::LoadRecordingPreset()
{
	const char *quality =
		config_get_string(main->Config(), "SimpleOutput", "RecQuality");
	const char *encoder =
		config_get_string(main->Config(), "SimpleOutput", "RecEncoder");

	videoEncoder = encoder;
	videoQuality = quality;
	ffmpegOutput = false;

	if (strcmp(quality, "Stream") == 0) {
		h264Recording = streamingEncoders.at(0).video;
		aacRecording = streamingEncoders.at(0).audio;
		usingRecordingPreset = false;
		return;

	} else if (strcmp(quality, "Lossless") == 0) {
		LoadRecordingPreset_Lossless();
		usingRecordingPreset = true;
		ffmpegOutput = true;
		return;

	} else {
		lowCPUx264 = false;

		if (strcmp(encoder, SIMPLE_ENCODER_X264) == 0) {
			LoadRecordingPreset_h264("obs_x264");
		} else if (strcmp(encoder, SIMPLE_ENCODER_X264_LOWCPU) == 0) {
			LoadRecordingPreset_h264("obs_x264");
			lowCPUx264 = true;
		} else if (strcmp(encoder, SIMPLE_ENCODER_QSV) == 0) {
			LoadRecordingPreset_h264("obs_qsv11");
		} else if (strcmp(encoder, SIMPLE_ENCODER_AMD) == 0) {
			LoadRecordingPreset_h264("amd_amf_h264");
		} else if (strcmp(encoder, SIMPLE_ENCODER_NVENC) == 0) {
			const char *id = EncoderAvailable("jim_nvenc")
						 ? "jim_nvenc"
						 : "ffmpeg_nvenc";
			LoadRecordingPreset_h264(id);
		}
		usingRecordingPreset = true;

		if (!CreateAACEncoder(aacRecording, aacRecEncID, 192,
				      "simple_aac_recording", 0))
			throw "Failed to create aac recording encoder "
			      "(simple output)";
	}
}

void SimpleOutput::CreateRecordingOutputs()
{
	if (!ffmpegOutput) {
		bool useReplayBuffer = config_get_bool(main->Config(),
						       "SimpleOutput", "RecRB");
		if (useReplayBuffer) {
			obs_data_t *hotkey;
			const char *str = config_get_string(
				main->Config(), "Hotkeys", "ReplayBuffer");
			if (str)
				hotkey = obs_data_create_from_json(str);
			else
				hotkey = nullptr;

			replayBuffer = obs_output_create("replay_buffer",
							 Str("ReplayBuffer"),
							 nullptr, hotkey);

			obs_data_release(hotkey);
			if (!replayBuffer)
				throw "Failed to create replay buffer output "
				      "(simple output)";
			obs_output_release(replayBuffer);

			signal_handler_t *signal =
				obs_output_get_signal_handler(replayBuffer);

			startReplayBuffer.Connect(signal, "start",
						  OBSStartReplayBuffer, this);
			stopReplayBuffer.Connect(signal, "stop",
						 OBSStopReplayBuffer, this);
			replayBufferStopping.Connect(signal, "stopping",
						     OBSReplayBufferStopping,
						     this);
		}

		fileOutput = obs_output_create(
			"ffmpeg_muxer", "simple_file_output", nullptr, nullptr);
		if (!fileOutput)
			throw "Failed to create recording output "
			      "(simple output)";
		obs_output_release(fileOutput);
	}

	startRecording.Connect(obs_output_get_signal_handler(fileOutput),
			       "start", OBSStartRecording, this);
	stopRecording.Connect(obs_output_get_signal_handler(fileOutput), "stop",
			      OBSStopRecording, this);
	recordStopping.Connect(obs_output_get_signal_handler(fileOutput),
			       "stopping", OBSRecordStopping, this);
}

OBSEncoder SimpleOutput::CreateVideoEncoder(const char *encoderType,
					    const char *name)
{
	OBSEncoder videoEncoder = nullptr;
	if (strcmp(encoderType, SIMPLE_ENCODER_QSV) == 0) {
		videoEncoder = LoadStreamingPreset("obs_qsv11", name);

	} else if (strcmp(encoderType, SIMPLE_ENCODER_AMD) == 0) {
		videoEncoder = LoadStreamingPreset("amd_amf_h264", name);

	} else if (strcmp(encoderType, SIMPLE_ENCODER_NVENC) == 0) {
		const char *id = EncoderAvailable("jim_nvenc") ? "jim_nvenc"
							       : "ffmpeg_nvenc";
		videoEncoder = LoadStreamingPreset(id, name);

	} else {
		videoEncoder = LoadStreamingPreset("obs_x264", name);
	}

	return videoEncoder;
}

#define SIMPLE_ARCHIVE_NAME "simple_archive_aac"

SimpleOutput::SimpleOutput(OBSBasic *main_,
			   const std::map<int, OBSData> &outputConfigs)
	: BasicOutputHandler(main_)
{
	for (auto &i : outputConfigs) {
		const char *videoEncoderType =
			obs_data_get_string(i.second, "stream_encoder");
		const char *name = obs_data_get_string(i.second, "name");

		const int bitrate = obs_data_get_int(i.second, "audio_bitrate");

		OBSEncoder audioEncoder;

		string encName(name);
		if (!CreateAACEncoder(audioEncoder, encName, bitrate,
				      "simple_aac", 0))
			throw "Failed to create aac streaming encoder (simple output)";

		if (!CreateAACEncoder(aacArchive, aacArchiveEncID,
				      GetAudioBitrate(), SIMPLE_ARCHIVE_NAME,
				      1))
			throw "Failed to create aac arhive encoder (simple output)";

		OBSEncoder videoEncoder =
			CreateVideoEncoder(videoEncoderType, name);

		int id = obs_data_get_int(i.second, "id_num");

		struct Encoders encoders = {audioEncoder, videoEncoder};
		streamingEncoders.insert({id, encoders});
	}

	LoadRecordingPreset();
	CreateRecordingOutputs();
}

int SimpleOutput::GetAudioBitrate() const
{
	int bitrate = (int)config_get_uint(main->Config(), "SimpleOutput",
					   "ABitrate");

	return FindClosestAvailableAACBitrate(bitrate);
}

void SimpleOutput::Update(const std::vector<OBSService> &services,
			  const std::map<int, OBSData> &outputConfigs)
{
	for (auto &output : streamingEncoders) {
		struct Encoders encoders = output.second;
		OBSData config = outputConfigs.at(output.first);

		int videoBitrate = obs_data_get_int(config, "video_bitrate");
		int audioBitrate = obs_data_get_int(config, "audio_bitrate");
		bool advanced = obs_data_get_bool(config, "use_advanced");
	        bool enforceBitrate = !config_get_bool(main->Config(), "Stream1",
					       "IgnoreRecommended");
		const char *custom =
			obs_data_get_string(config, "x264Settings");
		const char *type =
			obs_data_get_string(config, "stream_encoder");

		OBSData audioSettings = obs_data_create();
		OBSData videoSettings = obs_data_create();

		const char *presetType;
		const char *preset;

		if (strcmp(type, SIMPLE_ENCODER_QSV) == 0) {
			presetType = "QSVPreset";

		} else if (strcmp(type, SIMPLE_ENCODER_AMD) == 0) {
			presetType = "AMDPreset";
			UpdateStreamingSettings_amd(videoSettings,
						    videoBitrate);

		} else if (strcmp(type, SIMPLE_ENCODER_NVENC) == 0) {
			presetType = "NVENCPreset";

		} else {
			presetType = "Preset";
		}

		preset = obs_data_get_string(config, presetType);

		obs_data_set_string(audioSettings, "rate_control", "CBR");
		obs_data_set_int(audioSettings, "bitrate", audioBitrate);

		if (advanced) {
			obs_data_set_string(videoSettings, "preset", preset);
			obs_data_set_string(videoSettings, "x264opts", custom);
		}

		obs_data_set_string(videoSettings, "rate_control", "CBR");
		obs_data_set_int(videoSettings, "bitrate", videoBitrate);

	        if (!enforceBitrate) {
		        obs_data_set_int(videoSettings, "bitrate", videoBitrate);
		        obs_data_set_int(audioSettings, "bitrate", audioBitrate);
	        }

		video_t *video = obs_get_video();
		enum video_format format = video_output_get_format(video);

		if (format != VIDEO_FORMAT_NV12 && format != VIDEO_FORMAT_I420)
			obs_encoder_set_preferred_video_format(
				encoders.video, VIDEO_FORMAT_NV12);

		obs_encoder_update(encoders.video, videoSettings);
		obs_encoder_update(encoders.audio, audioSettings);
		obs_encoder_update(aacArchive, audioSettings);

		obs_data_release(audioSettings);
		obs_data_release(videoSettings);
	}

	UpdateServiceEncoders(services);
}

void SimpleOutput::UpdateServiceEncoders(const std::vector<OBSService> &services)
{
	for (auto &service : services) {
		int outputID = obs_service_get_output_id(service);

		OBSEncoder audioEncoder = streamingEncoders.at(outputID).audio;
		OBSEncoder videoEncoder = streamingEncoders.at(outputID).video;

		OBSData videoSettings = obs_encoder_get_settings(videoEncoder);
		OBSData audioSettings = obs_encoder_get_settings(audioEncoder);

		obs_data_release(videoSettings);
		obs_data_release(audioSettings);

		obs_service_apply_encoder_settings(service, videoSettings,
						   audioSettings);

		obs_encoder_update(aacArchive, audioSettings);
	}
}

void SimpleOutput::UpdateRecordingAudioSettings()
{
	obs_data_t *settings = obs_data_create();
	obs_data_set_int(settings, "bitrate", 192);
	obs_data_set_string(settings, "rate_control", "CBR");

	obs_encoder_update(aacRecording, settings);

	obs_data_release(settings);
}

#define CROSS_DIST_CUTOFF 2000.0

int SimpleOutput::CalcCRF(int crf)
{
	int cx = config_get_uint(main->Config(), "Video", "OutputCX");
	int cy = config_get_uint(main->Config(), "Video", "OutputCY");
	double fCX = double(cx);
	double fCY = double(cy);

	if (lowCPUx264)
		crf -= 2;

	double crossDist = sqrt(fCX * fCX + fCY * fCY);
	double crfResReduction =
		fmin(CROSS_DIST_CUTOFF, crossDist) / CROSS_DIST_CUTOFF;
	crfResReduction = (1.0 - crfResReduction) * 10.0;

	return crf - int(crfResReduction);
}

void SimpleOutput::UpdateRecordingSettings_x264_crf(int crf)
{
	obs_data_t *settings = obs_data_create();
	obs_data_set_int(settings, "crf", crf);
	obs_data_set_bool(settings, "use_bufsize", true);
	obs_data_set_string(settings, "rate_control", "CRF");
	obs_data_set_string(settings, "profile", "high");
	obs_data_set_string(settings, "preset",
			    lowCPUx264 ? "ultrafast" : "veryfast");

	obs_encoder_update(h264Recording, settings);

	obs_data_release(settings);
}

static bool icq_available(obs_encoder_t *encoder)
{
	obs_properties_t *props = obs_encoder_properties(encoder);
	obs_property_t *p = obs_properties_get(props, "rate_control");
	bool icq_found = false;

	size_t num = obs_property_list_item_count(p);
	for (size_t i = 0; i < num; i++) {
		const char *val = obs_property_list_item_string(p, i);
		if (strcmp(val, "ICQ") == 0) {
			icq_found = true;
			break;
		}
	}

	obs_properties_destroy(props);
	return icq_found;
}

void SimpleOutput::UpdateRecordingSettings_qsv11(int crf)
{
	bool icq = icq_available(h264Recording);

	obs_data_t *settings = obs_data_create();
	obs_data_set_string(settings, "profile", "high");

	if (icq) {
		obs_data_set_string(settings, "rate_control", "ICQ");
		obs_data_set_int(settings, "icq_quality", crf);
	} else {
		obs_data_set_string(settings, "rate_control", "CQP");
		obs_data_set_int(settings, "qpi", crf);
		obs_data_set_int(settings, "qpp", crf);
		obs_data_set_int(settings, "qpb", crf);
	}

	obs_encoder_update(h264Recording, settings);

	obs_data_release(settings);
}

void SimpleOutput::UpdateRecordingSettings_nvenc(int cqp)
{
	obs_data_t *settings = obs_data_create();
	obs_data_set_string(settings, "rate_control", "CQP");
	obs_data_set_string(settings, "profile", "high");
	obs_data_set_string(settings, "preset", "hq");
	obs_data_set_int(settings, "cqp", cqp);

	obs_encoder_update(h264Recording, settings);

	obs_data_release(settings);
}

void SimpleOutput::UpdateStreamingSettings_amd(obs_data_t *settings,
					       int bitrate)
{
	// Static Properties
	obs_data_set_int(settings, "Usage", 0);
	obs_data_set_int(settings, "Profile", 100); // High

	// Rate Control Properties
	obs_data_set_int(settings, "RateControlMethod", 3);
	obs_data_set_int(settings, "Bitrate.Target", bitrate);
	obs_data_set_int(settings, "FillerData", 1);
	obs_data_set_int(settings, "VBVBuffer", 1);
	obs_data_set_int(settings, "VBVBuffer.Size", bitrate);

	// Picture Control Properties
	obs_data_set_double(settings, "KeyframeInterval", 2.0);
	obs_data_set_int(settings, "BFrame.Pattern", 0);
}

void SimpleOutput::UpdateRecordingSettings_amd_cqp(int cqp)
{
	obs_data_t *settings = obs_data_create();

	// Static Properties
	obs_data_set_int(settings, "Usage", 0);
	obs_data_set_int(settings, "Profile", 100); // High

	// Rate Control Properties
	obs_data_set_int(settings, "RateControlMethod", 0);
	obs_data_set_int(settings, "QP.IFrame", cqp);
	obs_data_set_int(settings, "QP.PFrame", cqp);
	obs_data_set_int(settings, "QP.BFrame", cqp);
	obs_data_set_int(settings, "VBVBuffer", 1);
	obs_data_set_int(settings, "VBVBuffer.Size", 100000);

	// Picture Control Properties
	obs_data_set_double(settings, "KeyframeInterval", 2.0);
	obs_data_set_int(settings, "BFrame.Pattern", 0);

	// Update and release
	obs_encoder_update(h264Recording, settings);
	obs_data_release(settings);
}

void SimpleOutput::UpdateRecordingSettings()
{
	bool ultra_hq = (videoQuality == "HQ");
	int crf = CalcCRF(ultra_hq ? 16 : 23);

	if (astrcmp_n(videoEncoder.c_str(), "x264", 4) == 0) {
		UpdateRecordingSettings_x264_crf(crf);

	} else if (videoEncoder == SIMPLE_ENCODER_QSV) {
		UpdateRecordingSettings_qsv11(crf);

	} else if (videoEncoder == SIMPLE_ENCODER_AMD) {
		UpdateRecordingSettings_amd_cqp(crf);

	} else if (videoEncoder == SIMPLE_ENCODER_NVENC) {
		UpdateRecordingSettings_nvenc(crf);
	}
	UpdateRecordingAudioSettings();
}

inline void
SimpleOutput::SetupOutputs(const std::map<int, OBSData> &outputConfigs)
{
	for (auto &encoders : streamingEncoders) {
		obs_encoder_set_audio(encoders.second.audio, obs_get_audio());
		obs_encoder_set_video(encoders.second.video, obs_get_video());
	}
	obs_encoder_set_audio(aacArchive, obs_get_audio());

	if (usingRecordingPreset) {
		if (ffmpegOutput) {
			obs_output_set_media(fileOutput, obs_get_video(),
					     obs_get_audio());
		} else {
			obs_encoder_set_video(h264Recording, obs_get_video());
			obs_encoder_set_audio(aacRecording, obs_get_audio());
		}
	}
}

const char *FindAudioEncoderFromCodec(const char *type)
{
	const char *alt_enc_id = nullptr;
	size_t i = 0;

	while (obs_enum_encoder_types(i++, &alt_enc_id)) {
		const char *codec = obs_get_encoder_codec(alt_enc_id);
		if (strcmp(type, codec) == 0) {
			return alt_enc_id;
		}
	}

	return nullptr;
}

bool SimpleOutput::SetupStreaming(const std::vector<OBSService> &services,
				  const std::map<int, OBSData> &outputConfigs)
{
	if (!Active())
        {
		SimpleOutput::Update(services, outputConfigs);
		SetupOutputs(outputConfigs);
	}

	Auth::ConfigStreamAuths();

	/* --------------------- */
	DisconnectSignals();
	streamOutputs.clear();

	for (auto &service : services) {
		const char *type = obs_service_get_output_type(service);
		if (!type) {
			type = "rtmp_output";
			const char *url = obs_service_get_url(service);
			if (url != NULL && strncmp(url, FTL_PROTOCOL,
						   strlen(FTL_PROTOCOL)) == 0) {
				type = "ftl_output";
			} else if (url != NULL &&
				   strncmp(url, RTMP_PROTOCOL,
					   strlen(RTMP_PROTOCOL)) != 0) {
				type = "ffmpeg_mpegts_muxer";
			}
		}

		char name[138];
		const char *serviceName = obs_data_get_string(
			obs_service_get_settings(service), "name");
		int outputID = obs_service_get_output_id(service);
		const char *outputName =
			obs_data_get_string(outputConfigs.at(outputID), "name");
		sprintf(name, "<simple> %s:%s", serviceName, outputName);

		OBSOutput output =
			obs_output_create(type, name, nullptr, nullptr);

		if (!output) {
			blog(LOG_WARNING,
			     "Creation of stream output type '%s' "
			     "failed!",
			     type);
			return false;
		}

		SetupVodTrack(service);

		obs_output_release(output);

		struct Encoders encoders = streamingEncoders.at(
			obs_service_get_output_id(service));
		if (!UpdateOutputEncoders(output, encoders))
			return false;

		obs_output_set_service(output, service);
		ConnectToSignals(output);

		streamOutputs.push_back(output);
	}

	return true;
}

static inline bool ServiceSupportsVodTrack(const char *service);

static void clear_archive_encoder(obs_output_t *output,
				  const char *expected_name)
{
	obs_encoder_t *last = obs_output_get_audio_encoder(output, 1);
	bool clear = false;

	/* ensures that we don't remove twitch's soundtrack encoder */
	if (last) {
		const char *name = obs_encoder_get_name(last);
		clear = name && strcmp(name, expected_name) == 0;
		obs_encoder_release(last);
	}

	if (clear)
		obs_output_set_audio_encoder(output, nullptr, 1);
}

void SimpleOutput::SetupVodTrack(obs_service_t *service)
{
	bool advanced =
		config_get_bool(main->Config(), "SimpleOutput", "UseAdvanced");
	bool enable = config_get_bool(main->Config(), "SimpleOutput",
				      "VodTrackEnabled");
	bool enableForCustomServer = config_get_bool(
		GetGlobalConfig(), "General", "EnableCustomServerVodTrack");

	obs_data_t *settings = obs_service_get_settings(service);
	const char *name = obs_data_get_string(settings, "service");

	const char *type = obs_data_get_string(settings, "type");
	if (strcmp(type, "rtmp_custom") == 0)
		enable = enableForCustomServer ? enable : false;
	else
		enable = advanced && enable && ServiceSupportsVodTrack(name);

	if (enable)
		obs_output_set_audio_encoder(streamOutput, aacArchive, 1);
	else
		clear_archive_encoder(streamOutput, SIMPLE_ARCHIVE_NAME);

	obs_data_release(settings);
}

bool SimpleOutput::StartStreaming()
{
	SetStreamOutputConfig();
	return StartStreamOutputs();
}

bool SimpleOutput::UpdateOutputEncoders(const OBSOutput &output,
					struct Encoders &encoders)
{
	bool isEncoded = obs_output_get_flags(output) & OBS_OUTPUT_ENCODED;

	if (isEncoded) {
		const char *codec =
			obs_output_get_supported_audio_codecs(output);
		if (!codec) {
			blog(LOG_WARNING, "Failed to load audio codec");
			return false;
		}

		if (strcmp(codec, "aac") != 0) {
			const char *id = FindAudioEncoderFromCodec(codec);
			int audioBitrate = GetAudioBitrate();
			obs_data_t *settings = obs_data_create();
			obs_data_set_int(settings, "bitrate", audioBitrate);

			encoders.audio = obs_audio_encoder_create(
				id, "alt_audio_enc", nullptr, 0, nullptr);
			obs_encoder_release(encoders.audio);

			if (!encoders.audio)
				return false;

			obs_encoder_update(encoders.audio, settings);
			obs_encoder_set_audio(encoders.audio, obs_get_audio());

			obs_data_release(settings);
		}
	}

	obs_output_set_audio_encoder(output, encoders.audio, 0);
	obs_output_set_video_encoder(output, encoders.video);

	return true;
}

static void remove_reserved_file_characters(string &s)
{
	replace(s.begin(), s.end(), '/', '_');
	replace(s.begin(), s.end(), '\\', '_');
	replace(s.begin(), s.end(), '*', '_');
	replace(s.begin(), s.end(), '?', '_');
	replace(s.begin(), s.end(), '"', '_');
	replace(s.begin(), s.end(), '|', '_');
	replace(s.begin(), s.end(), ':', '_');
	replace(s.begin(), s.end(), '>', '_');
	replace(s.begin(), s.end(), '<', '_');
}

static void ensure_directory_exists(string &path)
{
	replace(path.begin(), path.end(), '\\', '/');

	size_t last = path.rfind('/');
	if (last == string::npos)
		return;

	string directory = path.substr(0, last);
	os_mkdirs(directory.c_str());
}

void SimpleOutput::UpdateRecording()
{
        auto outputConfigs = main->GetStreamOutputSettings();
        auto services = main->GetServices();

	if (replayBufferActive || recordingActive)
		return;

	if (usingRecordingPreset) {
		if (!ffmpegOutput)
			UpdateRecordingSettings();
	} else if (!obs_output_active(streamOutput)) {
		Update(services, outputConfigs);
	}

	if (!Active())
	{
		SimpleOutput::Update(services, outputConfigs);
		SetupOutputs(outputConfigs);
	}

	if (!ffmpegOutput) {
		obs_output_set_video_encoder(fileOutput, h264Recording);
		obs_output_set_audio_encoder(fileOutput, aacRecording, 0);
	}
	if (replayBuffer) {
		obs_output_set_video_encoder(replayBuffer, h264Recording);
		obs_output_set_audio_encoder(replayBuffer, aacRecording, 0);
	}

	recordingConfigured = true;
}

bool SimpleOutput::ConfigureRecording(bool updateReplayBuffer)
{
	const char *path =
		config_get_string(main->Config(), "SimpleOutput", "FilePath");
	const char *format =
		config_get_string(main->Config(), "SimpleOutput", "RecFormat");
	const char *mux = config_get_string(main->Config(), "SimpleOutput",
					    "MuxerCustom");
	bool noSpace = config_get_bool(main->Config(), "SimpleOutput",
				       "FileNameWithoutSpace");
	const char *filenameFormat = config_get_string(main->Config(), "Output",
						       "FilenameFormatting");
	bool overwriteIfExists =
		config_get_bool(main->Config(), "Output", "OverwriteIfExists");
	const char *rbPrefix = config_get_string(main->Config(), "SimpleOutput",
						 "RecRBPrefix");
	const char *rbSuffix = config_get_string(main->Config(), "SimpleOutput",
						 "RecRBSuffix");
	int rbTime =
		config_get_int(main->Config(), "SimpleOutput", "RecRBTime");
	int rbSize =
		config_get_int(main->Config(), "SimpleOutput", "RecRBSize");

	string f;
	string strPath;

	obs_data_t *settings = obs_data_create();
	if (updateReplayBuffer) {
		f = GetFormatString(filenameFormat, rbPrefix, rbSuffix);
		strPath = GetOutputFilename(path, ffmpegOutput ? "avi" : format,
					    noSpace, overwriteIfExists,
					    f.c_str());
		obs_data_set_string(settings, "directory", path);
		obs_data_set_string(settings, "format", f.c_str());
		obs_data_set_string(settings, "extension", format);
		obs_data_set_bool(settings, "allow_spaces", !noSpace);
		obs_data_set_int(settings, "max_time_sec", rbTime);
		obs_data_set_int(settings, "max_size_mb",
				 usingRecordingPreset ? rbSize : 0);
	} else {
		f = GetFormatString(filenameFormat, nullptr, nullptr);
		strPath = GetRecordingFilename(path,
					       ffmpegOutput ? "avi" : format,
					       noSpace, overwriteIfExists,
					       f.c_str(), ffmpegOutput);
		obs_data_set_string(settings, ffmpegOutput ? "url" : "path",
				    strPath.c_str());
	}

	obs_data_set_string(settings, "muxer_settings", mux);

	if (updateReplayBuffer)
		obs_output_update(replayBuffer, settings);
	else
		obs_output_update(fileOutput, settings);

	obs_data_release(settings);
	return true;
}

bool SimpleOutput::StartRecording()
{
	UpdateRecording();
	if (!ConfigureRecording(false))
		return false;
	if (!obs_output_start(fileOutput)) {
		QString error_reason;
		const char *error = obs_output_get_last_error(fileOutput);
		if (error)
			error_reason = QT_UTF8(error);
		else
			error_reason = QTStr("Output.StartFailedGeneric");
		QMessageBox::critical(main,
				      QTStr("Output.StartRecordingFailed"),
				      error_reason);
		return false;
	}

	return true;
}

bool SimpleOutput::StartReplayBuffer()
{
	UpdateRecording();
	if (!ConfigureRecording(true))
		return false;
	if (!obs_output_start(replayBuffer)) {
		QMessageBox::critical(main, QTStr("Output.StartReplayFailed"),
				      QTStr("Output.StartFailedGeneric"));
		return false;
	}

	return true;
}

void SimpleOutput::StopStreaming(bool force)
{
	for (auto &output : streamOutputs) {
		if (force)
			obs_output_force_stop(output);
		else
			obs_output_stop(output);
	}
}

void SimpleOutput::StopRecording(bool force)
{
	if (force)
		obs_output_force_stop(fileOutput);
	else
		obs_output_stop(fileOutput);
}

void SimpleOutput::StopReplayBuffer(bool force)
{
	if (force)
		obs_output_force_stop(replayBuffer);
	else
		obs_output_stop(replayBuffer);
}

bool SimpleOutput::StreamingActive() const
{
	for (auto &output : streamOutputs) {
		if (obs_output_active(output))
			return true;
	}
	return false;
}

bool SimpleOutput::RecordingActive() const
{
	return obs_output_active(fileOutput);
}

bool SimpleOutput::ReplayBufferActive() const
{
	return obs_output_active(replayBuffer);
}

/* ------------------------------------------------------------------------ */

struct AdvancedOutput : BasicOutputHandler {
	OBSEncoder streamAudioEnc;
	OBSEncoder streamArchiveEnc;
	OBSEncoder aacTrack[MAX_AUDIO_MIXES];
	std::map<int, struct Encoders> streamingEncoders;
	OBSEncoder h264Recording;

	bool ffmpegOutput;
	bool ffmpegRecording;
	bool useStreamEncoder;
	bool usesBitrate = false;

	string aacEncoderID[MAX_AUDIO_MIXES];

	AdvancedOutput(OBSBasic *main_,
		       const std::map<int, OBSData> &outputConfigs);

	void SetupRecording(const OBSData &config);
	OBSEncoder CreateVideoEncoder(const char *type, const char *name,
				      const OBSData &settings);

	inline void UpdateStreamSettings();
	inline void
	UpdateStreamSettings(const std::map<int, OBSData> &outputConfig);
	inline void UpdateRecordingSettings();
	inline void UpdateAudioSettings();
	inline void
	UpdateAudioSettings(const std::vector<OBSService> &services,
			    const std::map<int, OBSData> &outputConfig);

	virtual void
	Update(const std::vector<OBSService> &services,
	       const std::map<int, OBSData> &outputConfigs) override;

	inline void SetupVodTrack(obs_service_t *service);

	inline void SetupStreaming(const std::map<int, OBSData> &outputConfigs);
	inline void SetupRecording();
	inline void SetupFFmpeg();
	void SetupOutputs(const std::map<int, OBSData> &outputConfigs);
	int GetAudioBitrate(size_t i) const;

	virtual bool SetupStreaming(const std::vector<OBSService> &services,
				    const std::map<int, OBSData> &outputConfigs) override;
	virtual bool StartStreaming() override;
	virtual bool StartRecording() override;
	virtual bool StartReplayBuffer() override;

	virtual void StopStreaming(bool force) override;
	virtual void StopRecording(bool force) override;
	virtual void StopReplayBuffer(bool force) override;

	virtual bool StreamingActive() const override;
	virtual bool RecordingActive() const override;
	virtual bool ReplayBufferActive() const override;
	virtual bool UpdateOutputEncoders(const OBSOutput &output,
					  int streamTrack, int outputID);
};

static OBSData GetDataFromJsonFile(const char *jsonFile)
{
	char fullPath[512];
	obs_data_t *data = nullptr;

	int ret = GetProfilePath(fullPath, sizeof(fullPath), jsonFile);
	if (ret > 0) {
		BPtr<char> jsonData = os_quick_read_utf8_file(fullPath);
		if (!!jsonData) {
			data = obs_data_create_from_json(jsonData);
		}
	}

	if (!data)
		data = obs_data_create();
	OBSData dataRet(data);
	obs_data_release(data);
	return dataRet;
}

static void ApplyEncoderDefaults(OBSData &settings,
				 const obs_encoder_t *encoder)
{
	OBSData dataRet = obs_encoder_get_defaults(encoder);
	obs_data_release(dataRet);

	if (!!settings)
		obs_data_apply(dataRet, settings);
	settings = std::move(dataRet);
}

#define ADV_ARCHIVE_NAME "adv_archive_aac"

#ifdef __APPLE__
static void translate_macvth264_encoder(const char *&encoder)
{
	if (strcmp(encoder, "vt_h264_hw") == 0) {
		encoder = "com.apple.videotoolbox.videoencoder.h264.gva";
	} else if (strcmp(encoder, "vt_h264_sw") == 0) {
		encoder = "com.apple.videotoolbox.videoencoder.h264";
	}
}
#endif

AdvancedOutput::AdvancedOutput(OBSBasic *main_,
			       const std::map<int, OBSData> &outputConfigs)
	: BasicOutputHandler(main_)
{
	for (auto &item : outputConfigs) {
		int id = item.first;
		OBSData config = item.second;

		/* Create Video Encoders */
		const char *streamEncoder =
			obs_data_get_string(config, "adv_stream_encoder");
#ifdef __APPLE__
		translate_macvth264_encoder(streamEncoder);
#endif
		OBSData advEncSettings =
			obs_data_get_obj(config, "adv_encoder_props");
		obs_data_release(advEncSettings);

		const char *outputName = obs_data_get_string(config, "name");

		int streamTrack =
			obs_data_get_int(config, "adv_audio_track") - 1;
		streamTrack = streamTrack < 0 ? 0 : streamTrack;

		OBSEncoder videoEncoder = CreateVideoEncoder(
			streamEncoder, outputName, advEncSettings);

		OBSEncoder audioEncoder;
		string aacName(outputName);
		CreateAACEncoder(audioEncoder, aacName,
				 GetAudioBitrate(streamTrack), "advanced_acc",
				 streamTrack);

		struct Encoders encoders = {audioEncoder, videoEncoder};
		streamingEncoders.insert({id, encoders});
	}
	if (!outputConfigs.empty())
		SetupRecording(outputConfigs.at(0));
}

OBSEncoder AdvancedOutput::CreateVideoEncoder(const char *type,
					      const char *name,
					      const OBSData &settings)
{
	char encoderName[128];
	sprintf(encoderName, "%s.adv_streaming_h264", name);

	OBSEncoder videoEncoder =
		obs_video_encoder_create(type, encoderName, settings, nullptr);

	if (!videoEncoder)
		throw "Failed to create streaming h264 encoder "
		      "(advanced output)";
	obs_encoder_release(videoEncoder);

	return videoEncoder;
}

void AdvancedOutput::SetupRecording(const OBSData &config)
{
	const char *recType =
		config_get_string(main->Config(), "AdvOut", "RecType");
	const char *recordEncoder =
		config_get_string(main->Config(), "AdvOut", "RecEncoder");
#ifdef __APPLE__
	translate_macvth264_encoder(recordEncoder);
#endif
	ffmpegOutput = astrcmpi(recType, "FFmpeg") == 0;
	ffmpegRecording =
		ffmpegOutput &&
		config_get_bool(main->Config(), "AdvOut", "FFOutputToFile");
	useStreamEncoder = astrcmpi(recordEncoder, "none") == 0;

	OBSData recordEncSettings = GetDataFromJsonFile("recordEncoder.json");

	const char *rate_control = obs_data_get_string(
		useStreamEncoder ? config : recordEncSettings, "rate_control");

	if (!rate_control)
		rate_control = "";

	usesBitrate = astrcmpi(rate_control, "CBR") == 0 ||
		      astrcmpi(rate_control, "VBR") == 0 ||
		      astrcmpi(rate_control, "ABR") == 0;

	if (ffmpegOutput) {
		fileOutput = obs_output_create(
			"ffmpeg_output", "adv_ffmpeg_output", nullptr, nullptr);
		if (!fileOutput)
			throw "Failed to create recording FFmpeg output "
			      "(advanced output)";
		obs_output_release(fileOutput);
	} else {
		bool useReplayBuffer =
			config_get_bool(main->Config(), "AdvOut", "RecRB");
		if (useReplayBuffer) {
			const char *str = config_get_string(
				main->Config(), "Hotkeys", "ReplayBuffer");
			obs_data_t *hotkey = obs_data_create_from_json(str);
			replayBuffer = obs_output_create("replay_buffer",
							 Str("ReplayBuffer"),
							 nullptr, hotkey);

			obs_data_release(hotkey);
			if (!replayBuffer)
				throw "Failed to create replay buffer output "
				      "(simple output)";
			obs_output_release(replayBuffer);

			signal_handler_t *signal =
				obs_output_get_signal_handler(replayBuffer);

			startReplayBuffer.Connect(signal, "start",
						  OBSStartReplayBuffer, this);
			stopReplayBuffer.Connect(signal, "stop",
						 OBSStopReplayBuffer, this);
			replayBufferStopping.Connect(signal, "stopping",
						     OBSReplayBufferStopping,
						     this);
		}

		fileOutput = obs_output_create(
			"ffmpeg_muxer", "adv_file_output", nullptr, nullptr);
		if (!fileOutput)
			throw "Failed to create recording output "
			      "(advanced output)";
		obs_output_release(fileOutput);

		if (!useStreamEncoder) {
			h264Recording = obs_video_encoder_create(
				recordEncoder, "recording_h264",
				recordEncSettings, nullptr);
			if (!h264Recording)
				throw "Failed to create recording h264 "
				      "encoder (advanced output)";
			obs_encoder_release(h264Recording);
		}
	}

	// recording aac encoders
	for (int i = 0; i < MAX_AUDIO_MIXES; i++) {
		char name[9];
		sprintf(name, "adv_aac%d", i);

		if (!CreateAACEncoder(aacTrack[i], aacEncoderID[i],
				      GetAudioBitrate(i), name, i))
			throw "Failed to create audio encoder "
			      "(advanced output)";
	}

	std::string id;
	int vodTrack =
		config_get_int(main->Config(), "AdvOut", "VodTrackIndex") - 1;
	if (!CreateAACEncoder(streamArchiveEnc, id, GetAudioBitrate(vodTrack),
			      ADV_ARCHIVE_NAME, vodTrack))
		throw "Failed to create archive audio encoder "
		      "(advanced output)";

	startRecording.Connect(obs_output_get_signal_handler(fileOutput),
			       "start", OBSStartRecording, this);
	stopRecording.Connect(obs_output_get_signal_handler(fileOutput), "stop",
			      OBSStopRecording, this);
	recordStopping.Connect(obs_output_get_signal_handler(fileOutput),
			       "stopping", OBSRecordStopping, this);
}

void AdvancedOutput::UpdateStreamSettings(const std::map<int, OBSData> &outputConfig)
{
	bool dynBitrate =
		config_get_bool(main->Config(), "Output", "DynamicBitrate");
	bool enforceBitrate = !config_get_bool(main->Config(), "Stream1",
					       "IgnoreRecommended");

	for (auto &encoder : streamingEncoders) {
		int id = encoder.first;
		OBSData config = outputConfig.at(id);

		bool applyServiceSettings =
			obs_data_get_bool(config, "apply_service_settings");
		const char *vidEncType =
			obs_data_get_string(config, "adv_stream_encoder");
		OBSData settings =
			obs_data_get_obj(config, "adv_encoder_props");
		obs_data_release(settings);

		if (!vidEncType || strlen(vidEncType) == 0)
			vidEncType = config_get_string(main->Config(), "AdvOut",
						       "Encoder");

		ApplyEncoderDefaults(settings, encoder.second.video);

		if (applyServiceSettings) {
			int bitrate =
				(int)obs_data_get_int(settings, "bitrate");
			auto services = main->GetServices();
			for (auto &service : services) {
				if (obs_service_get_output_id(service) == id)
					obs_service_apply_encoder_settings(
						service, settings, nullptr);

				if (!enforceBitrate)
					obs_data_set_int(settings, "bitrate",
							 bitrate);
			}
		}

		if (dynBitrate && astrcmpi(vidEncType, "jim_nvenc") == 0)
			obs_data_set_bool(settings, "lookahead", false);

		video_t *video = obs_get_video();
		enum video_format format = video_output_get_format(video);

		if (format != VIDEO_FORMAT_NV12 && format != VIDEO_FORMAT_I420)
			obs_encoder_set_preferred_video_format(
				encoder.second.video, VIDEO_FORMAT_NV12);

		obs_encoder_update(encoder.second.video, settings);
	}
}

inline void AdvancedOutput::UpdateRecordingSettings()
{
	OBSData settings = GetDataFromJsonFile("recordEncoder.json");
	obs_encoder_update(h264Recording, settings);
	obs_data_release(settings);
}

void AdvancedOutput::Update(const std::vector<OBSService> &services,
			    const std::map<int, OBSData> &outputConfigs)
{
	UpdateStreamSettings(outputConfigs);
	if (!useStreamEncoder && !ffmpegOutput)
		UpdateRecordingSettings();
	UpdateAudioSettings(services, outputConfigs);
}

static inline bool ServiceSupportsVodTrack(const char *service)
{
	static const char *vodTrackServices[] = {"Twitch"};

	for (const char *vodTrackService : vodTrackServices) {
		if (astrcmpi(vodTrackService, service) == 0)
			return true;
	}

	return false;
}

inline void
AdvancedOutput::SetupStreaming(const std::map<int, OBSData> &outputConfigs)
{

	for (auto &encoders : streamingEncoders) {
		int id = encoders.first;
		OBSData config = outputConfigs.at(id);

		bool rescale = obs_data_get_bool(config, "adv_use_rescale");
		const char *rescaleRes =
			obs_data_get_string(config, "adv_rescale");
		unsigned int cx = 0;
		unsigned int cy = 0;

		if (rescale && rescaleRes && *rescaleRes) {
			if (sscanf(rescaleRes, "%ux%u", &cx, &cy) != 2) {
				cx = 0;
				cy = 0;
			}
		}

		// Audio encoder track defaults to 0 in Advanced
		for (auto &output : streamOutputs) {
			obs_output_set_audio_encoder(output, encoders.second.audio, 0);
		}
		obs_encoder_set_scaled_size(encoders.second.video, cx, cy);
		obs_encoder_set_video(encoders.second.video, obs_get_video());
	}

	auto services = main->GetServices();
	for (auto &service : services) {
		const char *type = obs_service_get_type(service);
		for (auto &encoder : streamingEncoders) {
			int output_id = encoder.first;
			if (strcmp(type, "rtmp_custom") == 0 &&
			    obs_service_get_output_id(service) == output_id) {
				obs_data_t *settings = obs_data_create();
				obs_service_apply_encoder_settings(
					service, settings, nullptr);

				obs_encoder_update(encoder.second.video,
						   settings);
				obs_data_release(settings);
			}
		}
	}
}

inline void AdvancedOutput::SetupRecording()
{
	const char *path =
		config_get_string(main->Config(), "AdvOut", "RecFilePath");
	const char *mux =
		config_get_string(main->Config(), "AdvOut", "RecMuxerCustom");
	bool rescale = config_get_bool(main->Config(), "AdvOut", "RecRescale");
	const char *rescaleRes =
		config_get_string(main->Config(), "AdvOut", "RecRescaleRes");
	int tracks;

	const char *recFormat =
		config_get_string(main->Config(), "AdvOut", "RecFormat");

	bool flv = strcmp(recFormat, "flv") == 0;

	if (flv)
		tracks = config_get_int(main->Config(), "AdvOut", "FLVTrack");
	else
		tracks = config_get_int(main->Config(), "AdvOut", "RecTracks");

	obs_data_t *settings = obs_data_create();
	unsigned int cx = 0;
	unsigned int cy = 0;
	int idx = 0;

	if (tracks == 0)
		tracks = config_get_int(main->Config(), "AdvOut", "TrackIndex");

	if (useStreamEncoder) {
		obs_output_set_video_encoder(fileOutput,
					     streamingEncoders.at(0).video);
		if (replayBuffer)
			obs_output_set_video_encoder(
				replayBuffer, streamingEncoders.at(0).video);
	} else {
		if (rescale && rescaleRes && *rescaleRes) {
			if (sscanf(rescaleRes, "%ux%u", &cx, &cy) != 2) {
				cx = 0;
				cy = 0;
			}
		}

		obs_encoder_set_scaled_size(h264Recording, cx, cy);
		obs_encoder_set_video(h264Recording, obs_get_video());
		obs_output_set_video_encoder(fileOutput, h264Recording);
		if (replayBuffer)
			obs_output_set_video_encoder(replayBuffer,
						     h264Recording);
	}

	if (!flv) {
		for (int i = 0; i < MAX_AUDIO_MIXES; i++) {
			if ((tracks & (1 << i)) != 0) {
				obs_output_set_audio_encoder(fileOutput,
							     aacTrack[i], idx);
				if (replayBuffer)
					obs_output_set_audio_encoder(
						replayBuffer, aacTrack[i], idx);
				idx++;
			}
		}
	} else if (flv && tracks != 0) {
		obs_output_set_audio_encoder(fileOutput, aacTrack[tracks - 1],
					     idx);

		if (replayBuffer)
			obs_output_set_audio_encoder(replayBuffer,
						     aacTrack[tracks - 1], idx);
	}

	obs_data_set_string(settings, "path", path);
	obs_data_set_string(settings, "muxer_settings", mux);
	obs_output_update(fileOutput, settings);
	if (replayBuffer)
		obs_output_update(replayBuffer, settings);
	obs_data_release(settings);
}

inline void AdvancedOutput::SetupFFmpeg()
{
	const char *url = config_get_string(main->Config(), "AdvOut", "FFURL");
	int vBitrate = config_get_int(main->Config(), "AdvOut", "FFVBitrate");
	int gopSize = config_get_int(main->Config(), "AdvOut", "FFVGOPSize");
	bool rescale = config_get_bool(main->Config(), "AdvOut", "FFRescale");
	const char *rescaleRes =
		config_get_string(main->Config(), "AdvOut", "FFRescaleRes");
	const char *formatName =
		config_get_string(main->Config(), "AdvOut", "FFFormat");
	const char *mimeType =
		config_get_string(main->Config(), "AdvOut", "FFFormatMimeType");
	const char *muxCustom =
		config_get_string(main->Config(), "AdvOut", "FFMCustom");
	const char *vEncoder =
		config_get_string(main->Config(), "AdvOut", "FFVEncoder");
	int vEncoderId =
		config_get_int(main->Config(), "AdvOut", "FFVEncoderId");
	const char *vEncCustom =
		config_get_string(main->Config(), "AdvOut", "FFVCustom");
	int aBitrate = config_get_int(main->Config(), "AdvOut", "FFABitrate");
	int aMixes = config_get_int(main->Config(), "AdvOut", "FFAudioMixes");
	const char *aEncoder =
		config_get_string(main->Config(), "AdvOut", "FFAEncoder");
	int aEncoderId =
		config_get_int(main->Config(), "AdvOut", "FFAEncoderId");
	const char *aEncCustom =
		config_get_string(main->Config(), "AdvOut", "FFACustom");
	obs_data_t *settings = obs_data_create();

	obs_data_set_string(settings, "url", url);
	obs_data_set_string(settings, "format_name", formatName);
	obs_data_set_string(settings, "format_mime_type", mimeType);
	obs_data_set_string(settings, "muxer_settings", muxCustom);
	obs_data_set_int(settings, "gop_size", gopSize);
	obs_data_set_int(settings, "video_bitrate", vBitrate);
	obs_data_set_string(settings, "video_encoder", vEncoder);
	obs_data_set_int(settings, "video_encoder_id", vEncoderId);
	obs_data_set_string(settings, "video_settings", vEncCustom);
	obs_data_set_int(settings, "audio_bitrate", aBitrate);
	obs_data_set_string(settings, "audio_encoder", aEncoder);
	obs_data_set_int(settings, "audio_encoder_id", aEncoderId);
	obs_data_set_string(settings, "audio_settings", aEncCustom);

	if (rescale && rescaleRes && *rescaleRes) {
		int width;
		int height;
		int val = sscanf(rescaleRes, "%dx%d", &width, &height);

		if (val == 2 && width && height) {
			obs_data_set_int(settings, "scale_width", width);
			obs_data_set_int(settings, "scale_height", height);
		}
	}

	obs_output_set_mixers(fileOutput, aMixes);
	obs_output_set_media(fileOutput, obs_get_video(), obs_get_audio());
	obs_output_update(fileOutput, settings);

	obs_data_release(settings);
}

static inline void SetEncoderName(obs_encoder_t *encoder, const char *name,
				  const char *defaultName)
{
	obs_encoder_set_name(encoder, (name && *name) ? name : defaultName);
}

inline void AdvancedOutput::UpdateAudioSettings()
{
	bool applyServiceSettings = config_get_bool(main->Config(), "AdvOut",
						    "ApplyServiceSettings");
	bool enforceBitrate = !config_get_bool(main->Config(), "Stream1",
					       "IgnoreRecommended");
	int streamTrackIndex =
		config_get_int(main->Config(), "AdvOut", "TrackIndex");
	int vodTrackIndex =
		config_get_int(main->Config(), "AdvOut", "VodTrackIndex");
	obs_data_t *settings[MAX_AUDIO_MIXES];

	for (size_t i = 0; i < MAX_AUDIO_MIXES; i++) {
		settings[i] = obs_data_create();
		obs_data_set_int(settings[i], "bitrate", GetAudioBitrate(i));
	}

	for (size_t i = 0; i < MAX_AUDIO_MIXES; i++) {
		string cfg_name = "Track";
		cfg_name += to_string((int)i + 1);
		cfg_name += "Name";
		const char *name = config_get_string(main->Config(), "AdvOut",
						     cfg_name.c_str());

		string def_name = "Track";
		def_name += to_string((int)i + 1);
		SetEncoderName(aacTrack[i], name, def_name.c_str());
	}

	for (size_t i = 0; i < MAX_AUDIO_MIXES; i++) {
		int track = (int)(i + 1);

		obs_encoder_update(aacTrack[i], settings[i]);

		if (track == streamTrackIndex || track == vodTrackIndex) {
			if (applyServiceSettings) {
				int bitrate = (int)obs_data_get_int(settings[i],
								    "bitrate");
				for (auto &service : main->GetServices()) {
					obs_service_apply_encoder_settings(
						service, nullptr, settings[i]);
				}

				if (!enforceBitrate)
					obs_data_set_int(settings[i], "bitrate",
							 bitrate);
			}
		}

		if (track == streamTrackIndex)
			obs_encoder_update(streamAudioEnc, settings[i]);
		if (track == vodTrackIndex)
			obs_encoder_update(streamArchiveEnc, settings[i]);

		obs_data_release(settings[i]);
	}
}

inline void
AdvancedOutput::UpdateAudioSettings(const std::vector<OBSService> &services,
				    const std::map<int, OBSData> &outputConfig)
{
	obs_data_t *settings[MAX_AUDIO_MIXES];

	for (size_t i = 0; i < MAX_AUDIO_MIXES; i++) {
		settings[i] = obs_data_create();
		obs_data_set_int(settings[i], "bitrate", GetAudioBitrate(i));
	}

	for (size_t i = 0; i < MAX_AUDIO_MIXES; i++) {
		string cfg_name = "Track";
		cfg_name += to_string((int)i + 1);
		cfg_name += "Name";
		const char *name = config_get_string(main->Config(), "AdvOut",
						     cfg_name.c_str());

		string def_name = "Track";
		def_name += to_string((int)i + 1);
		SetEncoderName(aacTrack[i], name, def_name.c_str());
		obs_encoder_update(aacTrack[i], settings[i]);
	}

	for (auto &encoders : streamingEncoders) {
		int id = encoders.first;
		OBSData config = outputConfig.at(id);

		int streamTrackIndex =
			obs_data_get_int(config, "adv_audio_track") - 1;

		streamTrackIndex = streamTrackIndex < 0 ? 0 : streamTrackIndex;

		for (auto &service : services) {
			if (obs_service_get_output_id(service) == id) {
				obs_service_apply_encoder_settings(
					service, nullptr,
					settings[streamTrackIndex]);
			}
		}

		obs_encoder_update(encoders.second.audio,
				   settings[streamTrackIndex]);
	}

	for (size_t i = 0; i < MAX_AUDIO_MIXES; i++)
		obs_data_release(settings[i]);
}

void AdvancedOutput::SetupOutputs(const std::map<int, OBSData> &outputConfigs)
{

	for (auto &encoders : streamingEncoders) {
		OBSEncoder audioEnc = encoders.second.audio;
		OBSEncoder videoEnc = encoders.second.video;

		obs_encoder_set_audio(audioEnc, obs_get_audio());
		obs_encoder_set_video(videoEnc, obs_get_video());
	}

	if (h264Recording)
		obs_encoder_set_video(h264Recording, obs_get_video());

	for (size_t i = 0; i < MAX_AUDIO_MIXES; i++)
		obs_encoder_set_audio(aacTrack[i], obs_get_audio());
	obs_encoder_set_audio(streamArchiveEnc, obs_get_audio());

	SetupStreaming(outputConfigs);

	if (ffmpegOutput)
		SetupFFmpeg();
	else
		SetupRecording();
}

int AdvancedOutput::GetAudioBitrate(size_t i) const
{
	static const char *names[] = {
		"Track1Bitrate", "Track2Bitrate", "Track3Bitrate",
		"Track4Bitrate", "Track5Bitrate", "Track6Bitrate",
	};
	int bitrate = (int)config_get_uint(main->Config(), "AdvOut", names[i]);
	return FindClosestAvailableAACBitrate(bitrate);
}

inline void AdvancedOutput::SetupVodTrack(obs_service_t *service)
{
	int streamTrack =
		config_get_int(main->Config(), "AdvOut", "TrackIndex");
	bool vodTrackEnabled =
		config_get_bool(main->Config(), "AdvOut", "VodTrackEnabled");
	int vodTrackIndex =
		config_get_int(main->Config(), "AdvOut", "VodTrackIndex");
	bool enableForCustomServer = config_get_bool(
		GetGlobalConfig(), "General", "EnableCustomServerVodTrack");

	OBSData settings = obs_service_get_settings(service);
	const char *type = obs_data_get_string(settings, "type");

	if (strcmp(type, "rtmp_custom") == 0) {
		vodTrackEnabled = enableForCustomServer ? vodTrackEnabled
							: false;
	} else {
		obs_data_t *settings = obs_service_get_settings(service);
		const char *service = obs_data_get_string(settings, "service");
		if (!ServiceSupportsVodTrack(service))
			vodTrackEnabled = false;
		obs_data_release(settings);
	}

	obs_data_release(settings);

	if (vodTrackEnabled && streamTrack != vodTrackIndex)
		obs_output_set_audio_encoder(streamOutput, streamArchiveEnc, 1);
	else
		clear_archive_encoder(streamOutput, ADV_ARCHIVE_NAME);
}

bool AdvancedOutput::SetupStreaming(const std::vector<OBSService> &services,
				    const std::map<int, OBSData> &outputConfigs)
{
	if (!useStreamEncoder ||
	    (!ffmpegOutput && !obs_output_active(fileOutput)))
		UpdateStreamSettings(outputConfigs);

	UpdateAudioSettings(services, outputConfigs);

	if (!Active())
		SetupOutputs(outputConfigs);

	Auth::ConfigStreamAuths();

	/* --------------------- */
	DisconnectSignals();
	streamOutputs.clear();

	for (auto &service : services) {
		const char *type = obs_service_get_output_type(service);
		if (!type) {
			type = "rtmp_output";
			const char *url = obs_service_get_url(service);
			if (url != NULL && strncmp(url, FTL_PROTOCOL,
						   strlen(FTL_PROTOCOL)) == 0) {
				type = "ftl_output";
			} else if (url != NULL &&
				   strncmp(url, RTMP_PROTOCOL,
					   strlen(RTMP_PROTOCOL)) != 0) {
				type = "ffmpeg_mpegts_muxer";
			}
		}

		char name[135];
		const char *serviceName = obs_data_get_string(
			obs_service_get_settings(service), "name");
		int streamTrack =
			obs_data_get_int(obs_service_get_settings(service),
					 "adv_audio_track") -
			1;
		streamTrack = streamTrack < 0 ? 0 : streamTrack;

		int outputID = obs_service_get_output_id(service);
		const char *outputName =
			obs_data_get_string(outputConfigs.at(outputID), "name");
		sprintf(name, "<adv> %s:%s", serviceName, outputName);

		OBSOutput output =
			obs_output_create(type, name, nullptr, nullptr);

		if (!output) {
			blog(LOG_WARNING,
			     "Creation of stream output type '%s' "
			     "failed!",
			     type);
			return false;
		}

		obs_output_release(output);

		ConnectToSignals(output);
		if (!UpdateOutputEncoders(output, streamTrack, outputID))
			return false;

		obs_output_set_service(output, service);

		streamOutputs.push_back(output);
	}

	return true;
}

bool AdvancedOutput::StartStreaming()
{
	SetStreamOutputConfig();
	return StartStreamOutputs();
}

bool AdvancedOutput::UpdateOutputEncoders(const OBSOutput &output,
					  int streamTrack, int outputID)
{
	bool isEncoded = obs_output_get_flags(output) & OBS_OUTPUT_ENCODED;

	OBSEncoder audioEncoder = streamingEncoders.at(outputID).audio;
	OBSEncoder videoEncoder = streamingEncoders.at(outputID).video;

	if (isEncoded) {
		const char *codec =
			obs_output_get_supported_audio_codecs(output);
		if (!codec) {
			blog(LOG_WARNING, "Failed to load audio codec");
			return false;
		}

		if (strcmp(codec, "aac") != 0) {
			OBSData settings =
				obs_encoder_get_settings(audioEncoder);
			obs_data_release(settings);

			const char *id = FindAudioEncoderFromCodec(codec);

			audioEncoder = obs_audio_encoder_create(
				id, "alt_audio_enc", nullptr, streamTrack,
				nullptr);

			if (!audioEncoder)
				return false;

			obs_encoder_release(audioEncoder);
			obs_encoder_update(audioEncoder, settings);
			obs_encoder_set_audio(audioEncoder, obs_get_audio());
		}
	}

	obs_output_set_video_encoder(output, videoEncoder);
	obs_output_set_audio_encoder(output, audioEncoder, 0);

	return true;
}

bool AdvancedOutput::StartRecording()
{
	const char *path;
	const char *recFormat;
	const char *filenameFormat;
	bool noSpace = false;
	bool overwriteIfExists = false;
        auto outputConfigs = main->GetStreamOutputSettings();
        auto services = main->GetServices();

	if (!useStreamEncoder) {
		if (!ffmpegOutput) {
			UpdateRecordingSettings();
		}
	} else if (streamOutputs.empty() || !obs_output_active(streamOutputs.front())) {
		UpdateStreamSettings(outputConfigs);
	}

	UpdateAudioSettings(services, outputConfigs);

	if (!Active())
		SetupOutputs(outputConfigs);

	if (!ffmpegOutput || ffmpegRecording) {
		path = config_get_string(main->Config(), "AdvOut",
					 ffmpegRecording ? "FFFilePath"
							 : "RecFilePath");
		recFormat = config_get_string(main->Config(), "AdvOut",
					      ffmpegRecording ? "FFExtension"
							      : "RecFormat");
		filenameFormat = config_get_string(main->Config(), "Output",
						   "FilenameFormatting");
		overwriteIfExists = config_get_bool(main->Config(), "Output",
						    "OverwriteIfExists");
		noSpace = config_get_bool(main->Config(), "AdvOut",
					  ffmpegRecording
						  ? "FFFileNameWithoutSpace"
						  : "RecFileNameWithoutSpace");

		string strPath = GetRecordingFilename(path, recFormat, noSpace,
						      overwriteIfExists,
						      filenameFormat,
						      ffmpegRecording);

		obs_data_t *settings = obs_data_create();
		obs_data_set_string(settings, ffmpegRecording ? "url" : "path",
				    strPath.c_str());

		obs_output_update(fileOutput, settings);

		obs_data_release(settings);
	}

	if (!obs_output_start(fileOutput)) {
		QString error_reason;
		const char *error = obs_output_get_last_error(fileOutput);
		if (error)
			error_reason = QT_UTF8(error);
		else
			error_reason = QTStr("Output.StartFailedGeneric");
		QMessageBox::critical(main,
				      QTStr("Output.StartRecordingFailed"),
				      error_reason);
		return false;
	}

	return true;
}

bool AdvancedOutput::StartReplayBuffer()
{
	const char *path;
	const char *recFormat;
	const char *filenameFormat;
	bool noSpace = false;
	bool overwriteIfExists = false;
	const char *rbPrefix;
	const char *rbSuffix;
	int rbTime;
	int rbSize;
        auto outputConfigs = main->GetStreamOutputSettings();
        auto services = main->GetServices();

	if (!useStreamEncoder) {
		if (!ffmpegOutput)
			UpdateRecordingSettings();
	} else if (!obs_output_active(streamOutput)) {
		UpdateStreamSettings(outputConfigs);
	}

	UpdateAudioSettings(services, outputConfigs);

	if (!Active())
		SetupOutputs(outputConfigs);

	if (!ffmpegOutput || ffmpegRecording) {
		path = config_get_string(main->Config(), "AdvOut",
					 ffmpegRecording ? "FFFilePath"
							 : "RecFilePath");
		recFormat = config_get_string(main->Config(), "AdvOut",
					      ffmpegRecording ? "FFExtension"
							      : "RecFormat");
		filenameFormat = config_get_string(main->Config(), "Output",
						   "FilenameFormatting");
		overwriteIfExists = config_get_bool(main->Config(), "Output",
						    "OverwriteIfExists");
		noSpace = config_get_bool(main->Config(), "AdvOut",
					  ffmpegRecording
						  ? "FFFileNameWithoutSpace"
						  : "RecFileNameWithoutSpace");
		rbPrefix = config_get_string(main->Config(), "SimpleOutput",
					     "RecRBPrefix");
		rbSuffix = config_get_string(main->Config(), "SimpleOutput",
					     "RecRBSuffix");
		rbTime = config_get_int(main->Config(), "AdvOut", "RecRBTime");
		rbSize = config_get_int(main->Config(), "AdvOut", "RecRBSize");

		string f = GetFormatString(filenameFormat, rbPrefix, rbSuffix);
		string strPath = GetOutputFilename(
			path, recFormat, noSpace, overwriteIfExists, f.c_str());

		obs_data_t *settings = obs_data_create();

		obs_data_set_string(settings, "directory", path);
		obs_data_set_string(settings, "format", f.c_str());
		obs_data_set_string(settings, "extension", recFormat);
		obs_data_set_bool(settings, "allow_spaces", !noSpace);
		obs_data_set_int(settings, "max_time_sec", rbTime);
		obs_data_set_int(settings, "max_size_mb",
				 usesBitrate ? 0 : rbSize);

		obs_output_update(replayBuffer, settings);

		obs_data_release(settings);
	}

	if (!obs_output_start(replayBuffer)) {
		QString error_reason;
		const char *error = obs_output_get_last_error(replayBuffer);
		if (error)
			error_reason = QT_UTF8(error);
		else
			error_reason = QTStr("Output.StartFailedGeneric");
		QMessageBox::critical(main,
				      QTStr("Output.StartRecordingFailed"),
				      error_reason);
		return false;
	}

	return true;
}

void AdvancedOutput::StopStreaming(bool force)
{
	for (auto &output : streamOutputs) {
		if (force)
			obs_output_force_stop(output);
		else
			obs_output_stop(output);
	}
}

void AdvancedOutput::StopRecording(bool force)
{
	if (force)
		obs_output_force_stop(fileOutput);
	else
		obs_output_stop(fileOutput);
}

void AdvancedOutput::StopReplayBuffer(bool force)
{
	if (force)
		obs_output_force_stop(replayBuffer);
	else
		obs_output_stop(replayBuffer);
}

bool AdvancedOutput::StreamingActive() const
{
	for (auto &output : streamOutputs)
		if (obs_output_active(output))
			return true;
	return false;
}

bool AdvancedOutput::RecordingActive() const
{
	return obs_output_active(fileOutput);
}

bool AdvancedOutput::ReplayBufferActive() const
{
	return obs_output_active(replayBuffer);
}

/* ------------------------------------------------------------------------ */

void BasicOutputHandler::SetupAutoRemux(const char *&ext)
{
	bool autoRemux = config_get_bool(main->Config(), "Video", "AutoRemux");
	if (autoRemux && strcmp(ext, "mp4") == 0)
		ext = "mkv";
}

std::string
BasicOutputHandler::GetRecordingFilename(const char *path, const char *ext,
					 bool noSpace, bool overwrite,
					 const char *format, bool ffmpeg)
{
	if (!ffmpeg)
		SetupAutoRemux(ext);

	string dst = GetOutputFilename(path, ext, noSpace, overwrite, format);
	lastRecordingPath = dst;
	return dst;
}

BasicOutputHandler *
CreateSimpleOutputHandler(OBSBasic *main,
			  const std::map<int, OBSData> &outputConfig)
{
	return new SimpleOutput(main, outputConfig);
}

BasicOutputHandler *
CreateAdvancedOutputHandler(OBSBasic *main,
			    const std::map<int, OBSData> &outputConfig)
{
	return new AdvancedOutput(main, outputConfig);
}