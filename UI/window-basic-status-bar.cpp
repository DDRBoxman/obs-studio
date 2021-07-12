#include <QLabel>
#include <QHBoxLayout>
#include <QPainter>
#include <QPixmap>
#include "obs-app.hpp"
#include "window-basic-main.hpp"
#include "window-basic-status-bar.hpp"
#include "window-basic-main-outputs.hpp"

OBSBasicStatusBar::OBSBasicStatusBar(QWidget *parent)
	: QStatusBar(parent),
	  delayInfo(new QLabel),
	  message(new QLabel),
	  messageDuration(new QTimer(this)),
	  droppedFrames(new QLabel),
	  multipleDroppedFrames(new QWidget(this)),
	  streamIcon(new QLabel),
	  streamTime(new QLabel),
	  recordTime(new QLabel),
	  recordIcon(new QLabel),
	  cpuUsage(new QLabel),
	  transparentPixmap(20, 20),
	  greenPixmap(20, 20),
	  grayPixmap(20, 20),
	  redPixmap(20, 20),
	  recordingActivePixmap(QIcon(":/res/images/recording-active.svg")
					.pixmap(QSize(20, 20))),
	  recordingPausePixmap(QIcon(":/res/images/recording-pause.svg")
				       .pixmap(QSize(20, 20))),
	  recordingPauseInactivePixmap(
		  QIcon(":/res/images/recording-pause-inactive.svg")
			  .pixmap(QSize(20, 20))),
	  recordingInactivePixmap(QIcon(":/res/images/recording-inactive.svg")
					  .pixmap(QSize(20, 20))),
	  streamingActivePixmap(QIcon(":/res/images/streaming-active.svg")
					.pixmap(QSize(20, 20))),
	  streamingInactivePixmap(QIcon(":/res/images/streaming-inactive.svg")
					  .pixmap(QSize(20, 20)))
{
	messageDuration->setSingleShot(true);
	connect(messageDuration, SIGNAL(timeout()), this,
		SLOT(messageTimeout()));

	streamTime->setText(QString("LIVE: 00:00:00"));
	recordTime->setText(QString("REC: 00:00:00"));
	cpuUsage->setText(QString("CPU: 0.0%, 0.00 fps"));

	streamIcon->setPixmap(streamingInactivePixmap);
	recordIcon->setPixmap(recordingInactivePixmap);

	QWidget *brWidget = new QWidget(this);
	QHBoxLayout *brLayout = new QHBoxLayout(brWidget);
	brLayout->setContentsMargins(0, 0, 0, 0);

	statusSquare = new QLabel(brWidget);
	brLayout->addWidget(statusSquare);

	kbps = new QLabel(brWidget);
	brLayout->addWidget(kbps);

	brWidget->setLayout(brLayout);

	QHBoxLayout *droppedFramesLayout =
		new QHBoxLayout(multipleDroppedFrames);
	droppedFramesLayout->setContentsMargins(0, 0, 0, 0);
	multipleDroppedFrames->setLayout(droppedFramesLayout);

	delayInfo->setAlignment(Qt::AlignRight);
	delayInfo->setAlignment(Qt::AlignVCenter);
	droppedFrames->setAlignment(Qt::AlignRight);
	droppedFrames->setAlignment(Qt::AlignVCenter);
	message->setAlignment(Qt::AlignRight);
	message->setAlignment(Qt::AlignVCenter);
	streamIcon->setAlignment(Qt::AlignRight);
	streamIcon->setAlignment(Qt::AlignVCenter);
	streamTime->setAlignment(Qt::AlignRight);
	streamTime->setAlignment(Qt::AlignVCenter);
	recordIcon->setAlignment(Qt::AlignRight);
	recordIcon->setAlignment(Qt::AlignVCenter);
	recordTime->setAlignment(Qt::AlignRight);
	recordTime->setAlignment(Qt::AlignVCenter);
	cpuUsage->setAlignment(Qt::AlignRight);
	cpuUsage->setAlignment(Qt::AlignVCenter);
	kbps->setAlignment(Qt::AlignRight);
	kbps->setAlignment(Qt::AlignVCenter);

	delayInfo->setIndent(20);
	droppedFrames->setIndent(20);
	streamIcon->setIndent(20);
	recordIcon->setIndent(20);
	cpuUsage->setIndent(20);
	kbps->setIndent(10);

	addPermanentWidget(message, 1);
	addPermanentWidget(droppedFrames);
	addPermanentWidget(multipleDroppedFrames);
	addPermanentWidget(streamIcon);
	addPermanentWidget(streamTime);
	addPermanentWidget(recordIcon);
	addPermanentWidget(recordTime);
	addPermanentWidget(cpuUsage);
	addPermanentWidget(delayInfo);
	addPermanentWidget(brWidget);

	transparentPixmap.fill(QColor(0, 0, 0, 0));
	greenPixmap.fill(QColor(0, 255, 0));
	grayPixmap.fill(QColor(72, 72, 72));
	redPixmap.fill(QColor(255, 0, 0));

	statusSquare->setPixmap(transparentPixmap);
}

void OBSBasicStatusBar::Activate()
{
	if (!active) {
		refreshTimer = new QTimer(this);
		connect(refreshTimer, SIGNAL(timeout()), this,
			SLOT(UpdateStatusBar()));

		int skipped = video_output_get_skipped_frames(obs_get_video());
		int total = video_output_get_total_frames(obs_get_video());

		totalStreamSeconds = 0;
		totalRecordSeconds = 0;
		lastSkippedFrameCount = 0;
		startSkippedFrameCount = skipped;
		startTotalFrameCount = total;

		refreshTimer->start(1000);
		active = true;

		if (streamStats.size() != 0) {
			statusSquare->setPixmap(grayPixmap);
		}
	}

	if (streamStats.size() != 0) {
		streamIcon->setPixmap(streamingActivePixmap);
	}

	if (recordOutput) {
		recordIcon->setPixmap(recordingActivePixmap);
	}
}

void OBSBasicStatusBar::Deactivate()
{
	OBSBasic *main = qobject_cast<OBSBasic *>(parent());
	if (!main)
		return;

	if (streamStats.size() == 0) {
		streamTime->setText(QString("LIVE: 00:00:00"));
		streamIcon->setPixmap(streamingInactivePixmap);
		totalStreamSeconds = 0;
	}

	if (!recordOutput) {
		recordTime->setText(QString("REC: 00:00:00"));
		recordIcon->setPixmap(recordingInactivePixmap);
		totalRecordSeconds = 0;
	}

	if (main->outputHandler && !main->outputHandler->Active()) {
		delete refreshTimer;

		delayInfo->setText("");
		droppedFrames->setText("");

		QLayout *layout = multipleDroppedFrames->layout();
		delete layout;
		multipleDroppedFrames->setLayout(
			new QHBoxLayout(multipleDroppedFrames));
		multipleDroppedFrames->layout()->setContentsMargins(0, 0, 0, 0);

		kbps->setText("");

		delaySecTotal = 0;
		delaySecStarting = 0;
		delaySecStopping = 0;
		active = false;
		overloadedNotify = true;

		statusSquare->setPixmap(transparentPixmap);
	}
}

void OBSBasicStatusBar::UpdateDelayMsg()
{
	QString msg;

	if (delaySecTotal) {
		if (delaySecStarting && !delaySecStopping) {
			msg = QTStr("Basic.StatusBar.DelayStartingIn");
			msg = msg.arg(QString::number(delaySecStarting));

		} else if (!delaySecStarting && delaySecStopping) {
			msg = QTStr("Basic.StatusBar.DelayStoppingIn");
			msg = msg.arg(QString::number(delaySecStopping));

		} else if (delaySecStarting && delaySecStopping) {
			msg = QTStr("Basic.StatusBar.DelayStartingStoppingIn");
			msg = msg.arg(QString::number(delaySecStopping),
				      QString::number(delaySecStarting));
		} else {
			msg = QTStr("Basic.StatusBar.Delay");
			msg = msg.arg(QString::number(delaySecTotal));
		}
	}

	delayInfo->setText(msg);
}

#define BITRATE_UPDATE_SECONDS 2

void OBSBasicStatusBar::UpdateBandwidth()
{
	if (streamStats.size() == 0)
		return;

	if (++bitrateUpdateSeconds < BITRATE_UPDATE_SECONDS)
		return;

	double sum = 0.0;
	uint64_t sumBytesSent = 0;
	uint64_t sumBytesTime = 0;

	for (auto &stat : streamStats) {
		OBSOutput output = stat.output;

		uint64_t bytesSent = obs_output_get_total_bytes(output);
		uint64_t bytesSentTime = os_gettime_ns();

		if (bytesSent < lastBytesSent)
			bytesSent = 0;
		if (bytesSent == 0)
			lastBytesSent = 0;

		uint64_t bitsBetween = (bytesSent - lastBytesSent) * 8;

		double timePassed = double(bytesSentTime - lastBytesSentTime) /
				    1000000000.0;

		double kbitsPerSec = double(bitsBetween) / timePassed / 1000.0;
		sum += kbitsPerSec;
		sumBytesSent += bytesSent;
		sumBytesTime += bytesSentTime;

		stat.lastBytesSent = bytesSent;
	}

	double avgKBPerSec = sum / (float)streamStats.size();
	QString text;
	text += QString("Avg. kb/s: ") + QString::number(avgKBPerSec, 'f', 0);

	kbps->setText(text);
	kbps->setMinimumWidth(kbps->width());

	lastBytesSent = (uint64_t)(sumBytesSent / streamStats.size());
	lastBytesSentTime = (uint64_t)(sumBytesTime / streamStats.size());
	bitrateUpdateSeconds = 0;
}

void OBSBasicStatusBar::UpdateCPUUsage()
{
	OBSBasic *main = qobject_cast<OBSBasic *>(parent());
	if (!main)
		return;

	QString text;
	text += QString("CPU: ") +
		QString::number(main->GetCPUUsage(), 'f', 1) + QString("%, ") +
		QString::number(obs_get_active_fps(), 'f', 2) + QString(" fps");

	cpuUsage->setText(text);
	cpuUsage->setMinimumWidth(cpuUsage->width());
}

void OBSBasicStatusBar::UpdateStreamTime()
{
	totalStreamSeconds++;

	int seconds = totalStreamSeconds % 60;
	int totalMinutes = totalStreamSeconds / 60;
	int minutes = totalMinutes % 60;
	int hours = totalMinutes / 60;

	QString text = QString::asprintf("LIVE: %02d:%02d:%02d", hours, minutes,
					 seconds);
	streamTime->setText(text);
	streamTime->setMinimumWidth(streamTime->width());

	if (reconnectingServices > 0) {
		QString msg =
			QString("%1 services are reconnecting...")
				.arg(QString::number(reconnectingServices));
		QString toolTip = "";

		for (auto &stat : streamStats) {
			int reconnectTimeout = stat.reconnectTimeout;
			int retries = stat.retries;

			if (reconnectTimeout > 0) {
				toolTip +=
					QTStr("Basic.StatusBar.Reconnecting")
						.arg(QString::number(retries),
						     QString::number(
							     reconnectTimeout));
				toolTip += "\n";
				stat.reconnectTimeout--;

			} else if (retries > 0) {
				QString msg =
					QTStr("Basic.StatusBar.AttemptingReconnect")
						.arg(QString::number(retries));
				toolTip += "\n";
			}
		}

		showMessage(msg, toolTip);
	}

	if (delaySecStopping > 0 || delaySecStarting > 0) {
		if (delaySecStopping > 0)
			--delaySecStopping;
		if (delaySecStarting > 0)
			--delaySecStarting;
		UpdateDelayMsg();
	}
}

extern volatile bool recording_paused;

void OBSBasicStatusBar::UpdateRecordTime()
{
	bool paused = os_atomic_load_bool(&recording_paused);

	if (!paused) {
		totalRecordSeconds++;

		int seconds = totalRecordSeconds % 60;
		int totalMinutes = totalRecordSeconds / 60;
		int minutes = totalMinutes % 60;
		int hours = totalMinutes / 60;

		QString text = QString::asprintf("REC: %02d:%02d:%02d", hours,
						 minutes, seconds);

		recordTime->setText(text);
		recordTime->setMinimumWidth(recordTime->width());
	} else {
		recordIcon->setPixmap(streamPauseIconToggle
					      ? recordingPauseInactivePixmap
					      : recordingPausePixmap);

		streamPauseIconToggle = !streamPauseIconToggle;
	}
}

void OBSBasicStatusBar::UpdateDroppedFrames()
{
	if (streamStats.size() == 0)
		return;

	float congestion = 0.0;

	for (auto &stat : streamStats) {
		OBSOutput output = stat.output;
		QLabel *droppedFrames = stat.droppedFrame;

		int totalDropped = obs_output_get_frames_dropped(output);
		int totalFrames = obs_output_get_total_frames(output);
		double percent =
			(double)totalDropped / (double)totalFrames * 100.0;

		if (!totalFrames)
			continue;

		QString text = QString("%1 (%2\%)")
				       .arg(QString::number(totalDropped),
					    QString::number(percent, 'f', 1));

		droppedFrames->setText(text);
		droppedFrames->setMinimumWidth(droppedFrames->width());

		congestion += obs_output_get_congestion(output);
	}

	congestion /= (float)streamStats.size();
	float avgCongestion = (congestion + lastCongestion) * 0.5f;
	if (avgCongestion < congestion)
		avgCongestion = congestion;
	if (avgCongestion > 1.0f)
		avgCongestion = 1.0f;

	if (avgCongestion < EPSILON) {
		statusSquare->setPixmap(greenPixmap);
	} else if (fabsf(avgCongestion - 1.0f) < EPSILON) {
		statusSquare->setPixmap(redPixmap);
	} else {
		QPixmap pixmap(20, 20);

		float red = avgCongestion * 2.0f;
		if (red > 1.0f)
			red = 1.0f;
		red *= 255.0;

		float green = (1.0f - avgCongestion) * 2.0f;
		if (green > 1.0f)
			green = 1.0f;
		green *= 255.0;

		pixmap.fill(QColor(int(red), int(green), 0));
		statusSquare->setPixmap(pixmap);
	}

	lastCongestion = congestion;
}

void OBSBasicStatusBar::OBSOutputReconnect(void *data, calldata_t *params)
{
	OBSBasicStatusBar *statusBar =
		reinterpret_cast<OBSBasicStatusBar *>(data);

	int seconds = (int)calldata_int(params, "timeout_sec");
	OBSOutput output = (obs_output_t *)calldata_ptr(params, "output");

	QMetaObject::invokeMethod(statusBar, "Reconnect", Q_ARG(int, seconds),
				  Q_ARG(OBSOutput, output));
	UNUSED_PARAMETER(params);
}

void OBSBasicStatusBar::OBSOutputReconnectSuccess(void *data,
						  calldata_t *params)
{
	OBSBasicStatusBar *statusBar =
		reinterpret_cast<OBSBasicStatusBar *>(data);

	OBSOutput output = (obs_output_t *)calldata_ptr(params, "output");
	QMetaObject::invokeMethod(statusBar, "ReconnectSuccess",
				  Q_ARG(OBSOutput, output));
	UNUSED_PARAMETER(params);
}

void OBSBasicStatusBar::Reconnect(int seconds, OBSOutput output)
{
	OBSBasic *main = qobject_cast<OBSBasic *>(parent());

	if (!reconnectingServices)
		main->SysTrayNotify(
			QTStr("Basic.SystemTray.Message.Reconnecting"),
			QSystemTrayIcon::Warning);

	reconnectingServices++;

	for (auto &stat : streamStats) {
		if (output == stat.output) {
			stat.reconnectTimeout = seconds;
			stat.retries++;
			break;
		}
	}

	delaySecTotal = obs_output_get_active_delay(output);
	UpdateDelayMsg();
}

void OBSBasicStatusBar::ReconnectClear()
{
	reconnectingServices--;
	bitrateUpdateSeconds = -1;
	lastBytesSentTime = os_gettime_ns();
	delaySecTotal = 0;
	UpdateDelayMsg();
}

void OBSBasicStatusBar::ReconnectSuccess(OBSOutput output)
{
	OBSBasic *main = qobject_cast<OBSBasic *>(parent());

	QString msg = QTStr("Basic.StatusBar.ReconnectSuccessful");
	showMessage(msg, 4000);
	main->SysTrayNotify(msg, QSystemTrayIcon::Information);
	ReconnectClear();

	for (auto &stat : streamStats) {
		if (output == stat.output) {
			stat.retries = 0;
			stat.lastBytesSent = 0;
			stat.reconnectTimeout = 0;
			break;
		}
	}

	if (output) {
		delaySecTotal = obs_output_get_active_delay(output);
		UpdateDelayMsg();
	}
}

void OBSBasicStatusBar::UpdateStatusBar()
{
	OBSBasic *main = qobject_cast<OBSBasic *>(parent());

	UpdateBandwidth();

	if (streamStats.size() != 0)
		UpdateStreamTime();

	if (recordOutput)
		UpdateRecordTime();

	UpdateDroppedFrames();

	int skipped = video_output_get_skipped_frames(obs_get_video());
	int total = video_output_get_total_frames(obs_get_video());

	skipped -= startSkippedFrameCount;
	total -= startTotalFrameCount;

	int diff = skipped - lastSkippedFrameCount;
	double percentage = double(skipped) / double(total) * 100.0;

	if (diff > 10 && percentage >= 0.1f) {
		showMessage(QTStr("HighResourceUsage"), 4000);
		if (!main->isVisible() && overloadedNotify) {
			main->SysTrayNotify(QTStr("HighResourceUsage"),
					    QSystemTrayIcon::Warning);
			overloadedNotify = false;
		}
	}

	lastSkippedFrameCount = skipped;
}

void OBSBasicStatusBar::StreamDelayStarting(OBSOutput output)
{
	OBSBasic *main = qobject_cast<OBSBasic *>(parent());

	if (!main)
		return;

	int sec = (int)obs_output_get_active_delay(output);
	delaySecTotal = delaySecStarting = sec;
	UpdateDelayMsg();

	if (main->activeStreams == (int)main->services.size()) {
		InitializeStats();
		Activate();
	}
}

void OBSBasicStatusBar::StreamDelayStopping(OBSOutput output)
{
	int sec = (int)obs_output_get_active_delay(output);
	delaySecTotal = delaySecStopping = sec;

	UpdateDelayMsg();
}

void OBSBasicStatusBar::StreamStarted()
{
	InitializeStats();

	droppedFrames->setText(QString("Dropped Frames: "));

	lastBytesSent = 0;
	lastBytesSentTime = os_gettime_ns();
	Activate();
}

void OBSBasicStatusBar::StreamStopped()
{
	if (streamStats.size() != 0) {
		for (auto &stat : streamStats) {
			OBSOutput output = stat.output;
			signal_handler_disconnect(
				obs_output_get_signal_handler(output),
				"reconnect", OBSOutputReconnect, this);
			signal_handler_disconnect(
				obs_output_get_signal_handler(output),
				"reconnect_success", OBSOutputReconnectSuccess,
				this);
			delete stat.droppedFrame;
		}

		droppedFrames->setText(QString(""));

		ReconnectClear();
		streamStats.clear();
		clearMessage();
		Deactivate();
	}
}

void OBSBasicStatusBar::RecordingStarted(obs_output_t *output)
{
	recordOutput = output;
	Activate();
}

void OBSBasicStatusBar::RecordingStopped()
{
	recordOutput = nullptr;
	Deactivate();
}

void OBSBasicStatusBar::RecordingPaused()
{
	QString text = recordTime->text() + QStringLiteral(" (PAUSED)");
	recordTime->setText(text);

	if (recordOutput) {
		recordIcon->setPixmap(recordingPausePixmap);
		streamPauseIconToggle = true;
	}
}

void OBSBasicStatusBar::RecordingUnpaused()
{
	if (recordOutput) {
		recordIcon->setPixmap(recordingActivePixmap);
	}
}

void OBSBasicStatusBar::showMessage(const QString &message_, int timeout)
{
	if (messageDuration->isActive())
		messageDuration->stop();
	message->setText(message_);
	if (timeout > 0)
		messageDuration->start(timeout);
}

void OBSBasicStatusBar::showMessage(const QString &message_,
				    const QString &toolTip, int timeout)
{
	showMessage(message_, timeout);
	message->setToolTip(toolTip);
}

void OBSBasicStatusBar::messageTimeout()
{
	if (messageDuration->isActive())
		messageDuration->stop();
	message->setText("");
	message->setToolTip("");
}

void OBSBasicStatusBar::InitializeStats()
{
	OBSBasic *main = qobject_cast<OBSBasic *>(parent());

	if (!main)
		return;

	std::vector<OBSService> services = main->GetServices();
	std::vector<OBSOutput> outputs = main->outputHandler->GetOutputs();

	streamStats.clear();
	if (!active) {
		for (unsigned int i = 0; i < services.size(); i++) {
			OBSService service = services[i];
			OBSOutput output = outputs[i];

			signal_handler_connect(
				obs_output_get_signal_handler(output),
				"reconnect", OBSOutputReconnect, this);
			signal_handler_connect(
				obs_output_get_signal_handler(output),
				"reconnect_success", OBSOutputReconnectSuccess,
				this);

			QLabel *droppedFrames = new QLabel(this);
			droppedFrames->setStyleSheet("border: 1px solid black");
			droppedFrames->setText(QString("0 (0.0\%)"));

			const char *streamName = obs_data_get_string(
				obs_service_get_settings(service), "name");
			droppedFrames->setToolTip(QString(streamName));

			streamStats.push_back({output, 0, 0, 0, droppedFrames});

			multipleDroppedFrames->layout()->addWidget(
				droppedFrames);
		}
	}
}