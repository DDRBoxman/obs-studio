#pragma once

#include <string>
#include <set>
#include <vector>
#include <map>

class OBSBasic;

struct Encoders {
	OBSEncoder audio;
	OBSEncoder video;
};

struct BasicOutputHandler {
	OBSOutput fileOutput;
	OBSOutput streamOutput; // TODO -- remove this
	std::vector<OBSOutput> streamOutputs;

	OBSOutput replayBuffer;
	OBSOutput virtualCam;
	bool streamingActive = false;
	bool recordingActive = false;
	bool delayActive = false;
	bool replayBufferActive = false;
	bool virtualCamActive = false;
	OBSBasic *main;

	std::string outputType;
	std::string lastError;

	std::string lastRecordingPath;

	OBSSignal startRecording;
	OBSSignal stopRecording;
	OBSSignal startReplayBuffer;
	OBSSignal stopReplayBuffer;
	OBSSignal startStreaming;
	OBSSignal stopStreaming;
	OBSSignal startVirtualCam;
	OBSSignal stopVirtualCam;
	OBSSignal streamDelayStarting;
	OBSSignal streamStopping;
	OBSSignal recordStopping;
	OBSSignal replayBufferStopping;
	OBSSignal replayBufferSaved;

	inline BasicOutputHandler(OBSBasic *main_);

	virtual ~BasicOutputHandler(){};

	virtual bool SetupStreaming(obs_service_t *service) = 0;
	virtual bool StartStreaming(obs_service_t *service) = 0;
	virtual bool
	StartStreaming(const std::vector<OBSService> &services,
		       const std::map<int, OBSData> &outputConfigs) = 0;
	virtual bool StartRecording() = 0;
	virtual bool StartReplayBuffer() { return false; }
	virtual bool StartVirtualCam();
	virtual void StopStreaming(bool force = false) = 0;
	virtual void StopRecording(bool force = false) = 0;
	virtual void StopReplayBuffer(bool force = false) { (void)force; }
	virtual void StopVirtualCam();
	virtual bool StreamingActive() const = 0;
	virtual bool RecordingActive() const = 0;
	virtual bool ReplayBufferActive() const { return false; }
	virtual bool VirtualCamActive() const;

	virtual void DisconnectSignals();
	virtual void ConnectToSignals(const OBSOutput &output);
	virtual void SetStreamOutputConfig();
	virtual bool StartStreamOutputs();
	virtual bool StartStreamOutputs(int retryDelay, int maxRetries,
					bool useDelay, int delaySec,
					int preserveDelay);

	virtual void Update(const std::vector<OBSService> &services,
			    const std::map<int, OBSData> &outputConfigs) = 0;
        virtual void SetupOutputs(const std::map<int, OBSData> &outputConfigs) = 0;

	inline bool Active() const
	{
		return streamingActive || recordingActive || delayActive ||
		       replayBufferActive || virtualCamActive;
	}

	inline std::vector<OBSOutput> GetOutputs() { return streamOutputs; }

protected:
	void SetupAutoRemux(const char *&ext);
	std::string GetRecordingFilename(const char *path, const char *ext,
					 bool noSpace, bool overwrite,
					 const char *format, bool ffmpeg);
};

BasicOutputHandler *CreateSimpleOutputHandler(OBSBasic *main);
BasicOutputHandler *CreateAdvancedOutputHandler(OBSBasic *main);

BasicOutputHandler *
CreateSimpleOutputHandler(OBSBasic *main,
			  const std::map<int, OBSData> &outputConfig);
BasicOutputHandler *
CreateAdvancedOutputHandler(OBSBasic *main,
			    const std::map<int, OBSData> &outputConfig);