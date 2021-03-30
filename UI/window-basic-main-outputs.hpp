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
	OBSOutput streamOutput;

	std::vector<OBSOutput> streamOutputs;
	std::map<int, struct Encoders> streamingEncoders;

	OBSOutput replayBuffer;
	bool streamingActive = false;
	bool recordingActive = false;
	bool delayActive = false;
	bool replayBufferActive = false;
	OBSBasic *main;

	std::string outputType;
	std::string lastError;

	OBSSignal startRecording;
	OBSSignal stopRecording;
	OBSSignal startReplayBuffer;
	OBSSignal stopReplayBuffer;
	OBSSignal startStreaming;
	OBSSignal stopStreaming;
	OBSSignal streamDelayStarting;
	OBSSignal streamStopping;
	OBSSignal recordStopping;
	OBSSignal replayBufferStopping;

	inline BasicOutputHandler(OBSBasic *main_) : main(main_) {}

	virtual ~BasicOutputHandler(){};

	virtual bool StartStreaming(obs_service_t *service) = 0;
	virtual bool
	StartStreaming(const std::vector<OBSService> &services,
		       const std::map<int, OBSData> &outputConfigs) = 0;
	virtual bool StartRecording() = 0;
	virtual bool StartReplayBuffer() { return false; }

	virtual void StopStreaming(bool force = false) = 0;
	virtual void StopRecording(bool force = false) = 0;
	virtual void StopReplayBuffer(bool force = false) { (void)force; }

	virtual bool StreamingActive() const = 0;
	virtual bool RecordingActive() const = 0;
	virtual bool ReplayBufferActive() const { return false; }

	virtual void DisconnectSignals();
	virtual void ConnectToSignals(const OBSOutput &output);
	virtual void SetStreamOutputConfig();
	virtual bool StartStreamOutputs();
	virtual bool StartStreamOutputs(int retryDelay, int maxRetries,
					bool useDelay, int delaySec,
					int preserveDelay);

	virtual void Update() = 0;
	virtual void Update(const std::vector<OBSService> &services,
			    const std::map<int, OBSData> &outputConfigs) = 0;

	inline bool Active() const
	{
		return streamingActive || recordingActive || delayActive ||
		       replayBufferActive;
	}

	inline std::vector<OBSOutput> GetOutputs() { return streamOutputs; }
};

BasicOutputHandler *CreateSimpleOutputHandler(OBSBasic *main);
BasicOutputHandler *CreateAdvancedOutputHandler(OBSBasic *main);

BasicOutputHandler *
CreateSimpleOutputHandler(OBSBasic *main,
			  const std::map<int, OBSData> &outputConfig);
BasicOutputHandler *
CreateAdvancedOutputHandler(OBSBasic *main,
			    const std::map<int, OBSData> &outputConfig);