#pragma once

#include <QStatusBar>
#include <QPointer>
#include <QTimer>
#include <util/platform.h>
#include <obs.h>
#include <map>
#include <vector>

class QLabel;

struct StreamStats {
	OBSOutput output;
	int retries;
	int reconnectTimeout;
	uint64_t lastBytesSent;
	QLabel *droppedFrame;

	~StreamStats()
	{
		if (!droppedFrame) {
			delete droppedFrame;
			droppedFrame = nullptr;
		}
	}
};

class OBSBasicStatusBar : public QStatusBar {
	Q_OBJECT

private:
	QLabel *delayInfo;
	QLabel *message;
	QTimer *messageDuration;
	QLabel *droppedFrames;
	QWidget *multipleDroppedFrames;
	QLabel *streamIcon;
	QLabel *streamTime;
	QLabel *recordTime;
	QLabel *recordIcon;
	QLabel *cpuUsage;
	QLabel *kbps;
	QLabel *statusSquare;

	int reconnectingServices = 0;

	std::vector<StreamStats> streamStats;

	obs_output_t *recordOutput = nullptr;
	bool active = false;
	bool overloadedNotify = true;
	bool streamPauseIconToggle = false;

	int totalStreamSeconds = 0;
	int totalRecordSeconds = 0;

	int delaySecTotal = 0;
	int delaySecStarting = 0;
	int delaySecStopping = 0;

	int startSkippedFrameCount = 0;
	int startTotalFrameCount = 0;
	int lastSkippedFrameCount = 0;

	int bitrateUpdateSeconds = 0;
	uint64_t lastBytesSent = 0;
	uint64_t lastBytesSentTime = 0;

	QPixmap transparentPixmap;
	QPixmap greenPixmap;
	QPixmap grayPixmap;
	QPixmap redPixmap;

	QPixmap recordingActivePixmap;
	QPixmap recordingPausePixmap;
	QPixmap recordingPauseInactivePixmap;
	QPixmap recordingInactivePixmap;
	QPixmap streamingActivePixmap;
	QPixmap streamingInactivePixmap;

	float lastCongestion = 0.0f;

	QPointer<QTimer> refreshTimer;

	obs_output_t *GetOutput();

	void Activate();
	void Deactivate();

	void UpdateDelayMsg();
	void UpdateBandwidth();
	void UpdateStreamTime();
	void UpdateRecordTime();
	void UpdateDroppedFrames();
	void InitializeStats();

	static void OBSOutputReconnect(void *data, calldata_t *params);
	static void OBSOutputReconnectSuccess(void *data, calldata_t *params);

private slots:
	void messageTimeout();
	void Reconnect(int seconds, OBSOutput output);
	void ReconnectSuccess(OBSOutput output);
	void UpdateStatusBar();
	void UpdateCPUUsage();

public:
	OBSBasicStatusBar(QWidget *parent);

	void StreamDelayStarting(OBSOutput output);
	void StreamDelayStopping(OBSOutput output);

	void StreamStarted();
	void StreamStopped();
	void RecordingStarted(obs_output_t *output);
	void RecordingStopped();
	void RecordingPaused();
	void RecordingUnpaused();

	void ReconnectClear();

public slots:
	void showMessage(const QString &message_, int timeout = 0);
	void showMessage(const QString &message_, const QString &toolTip,
			 int timeout = 0);
};
