#include <chrono>

#include <QFormLayout>

#include <obs.hpp>
#include <util/platform.h>
#include <util/util_uint64.h>
#include <graphics/vec4.h>
#include <graphics/graphics.h>
#include <graphics/math-extra.h>

#include "window-basic-auto-config.hpp"
#include "window-basic-main.hpp"
#include "qt-wrappers.hpp"
#include "obs-app.hpp"

#include "ui_AutoConfigTestPage.h"

#define wiz reinterpret_cast<AutoConfig *>(wizard())

using namespace std;

/* ------------------------------------------------------------------------- */

class TestMode {
	obs_video_info ovi;
	OBSSource source[6];

	static void render_rand(void *, uint32_t cx, uint32_t cy)
	{
		gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
		gs_eparam_t *randomvals[3] = {
			gs_effect_get_param_by_name(solid, "randomvals1"),
			gs_effect_get_param_by_name(solid, "randomvals2"),
			gs_effect_get_param_by_name(solid, "randomvals3")};

		struct vec4 r;

		for (int i = 0; i < 3; i++) {
			vec4_set(&r, rand_float(true) * 100.0f,
				 rand_float(true) * 100.0f,
				 rand_float(true) * 50000.0f + 10000.0f, 0.0f);
			gs_effect_set_vec4(randomvals[i], &r);
		}

		while (gs_effect_loop(solid, "Random"))
			gs_draw_sprite(nullptr, 0, cx, cy);
	}

public:
	inline TestMode()
	{
		obs_get_video_info(&ovi);
		obs_add_main_render_callback(render_rand, this);

		for (uint32_t i = 0; i < 6; i++) {
			source[i] = obs_get_output_source(i);
			obs_source_release(source[i]);
			obs_set_output_source(i, nullptr);
		}
	}

	inline ~TestMode()
	{
		for (uint32_t i = 0; i < 6; i++)
			obs_set_output_source(i, source[i]);

		obs_remove_main_render_callback(render_rand, this);
		obs_reset_video(&ovi);
	}

	inline void SetVideo(int cx, int cy, int fps_num, int fps_den)
	{
		obs_video_info newOVI = ovi;

		newOVI.output_width = (uint32_t)cx;
		newOVI.output_height = (uint32_t)cy;
		newOVI.fps_num = (uint32_t)fps_num;
		newOVI.fps_den = (uint32_t)fps_den;

		obs_reset_video(&newOVI);
	}
};

/* ------------------------------------------------------------------------- */

#define TEST_STR(x) "Basic.AutoConfig.TestPage." x
#define SUBTITLE_TESTING TEST_STR("Subtitle.Testing")
#define SUBTITLE_COMPLETE TEST_STR("Subtitle.Complete")
#define TEST_BW TEST_STR("TestingBandwidth")
#define TEST_BW_CONNECTING TEST_STR("TestingBandwidth.Connecting")
#define TEST_BW_CONNECT_FAIL TEST_STR("TestingBandwidth.ConnectFailed")
#define TEST_BW_SERVER TEST_STR("TestingBandwidth.Server")
#define TEST_RES TEST_STR("TestingRes")
#define TEST_RES_VAL TEST_STR("TestingRes.Resolution")
#define TEST_RES_FAIL TEST_STR("TestingRes.Fail")
#define TEST_SE TEST_STR("TestingStreamEncoder")
#define TEST_RE TEST_STR("TestingRecordingEncoder")
#define TEST_RESULT_SE TEST_STR("Result.StreamingEncoder")
#define TEST_RESULT_RE TEST_STR("Result.RecordingEncoder")

void AutoConfigTestPage::UpdateStage(QString name, QString stage)
{
	ui->progressLabel->setText(name + ": " + stage);
}

void AutoConfigTestPage::StartBandwidthStage(const OBSData &settings)
{
	emit StageChanged(QString(obs_data_get_string(settings, "name")),
			  QTStr(TEST_BW));
	testThread = std::thread(
		[this](const OBSData &settings_) {
			TestBandwidthThread(settings_);
		},
		settings);
}

void AutoConfigTestPage::StartStreamEncoderStage(const OBSData &settings)
{
	emit StageChanged(QString(obs_data_get_string(settings, "name")),
			  QTStr(TEST_SE));
	testThread = std::thread(
		[this](const OBSData &settings_) {
			TestStreamEncoderThread(settings_);
		},
		settings);
}

void AutoConfigTestPage::StartRecordingEncoderStage(const OBSData &settings)
{
	emit StageChanged(QString(""), QTStr(TEST_RE));
	testThread = std::thread(
		[this](const OBSData &settings_) {
			TestRecordingEncoderThread(settings_);
		},
		settings);
}

std::vector<ServerInfo>
AutoConfigTestPage::GetServers(const std::string &service)
{
	std::vector<ServerInfo> servers;
	OBSData settings = obs_data_create();
	obs_data_release(settings);
	obs_data_set_string(settings, "service", service.c_str());

	obs_properties_t *ppts = obs_get_service_properties("rtmp_common");
	obs_property_t *p = obs_properties_get(ppts, "service");
	obs_property_modified(p, settings);

	p = obs_properties_get(ppts, "server");
	size_t count = obs_property_list_item_count(p);
	servers.reserve(count);

	for (size_t i = 0; i < count; i++) {
		const char *name = obs_property_list_item_name(p, i);
		const char *server = obs_property_list_item_string(p, i);

		if (wiz->CanTestServer(name)) {
			ServerInfo info(name, server);
			servers.push_back(info);
		}
	}

	obs_properties_destroy(ppts);
	return servers;
}

static inline void string_depad_key(string &key)
{
	while (!key.empty()) {
		char ch = key.back();
		if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r')
			key.pop_back();
		else
			break;
	}
}

const char *FindAudioEncoderFromCodec(const char *type);

void AutoConfigTestPage::TestBandwidthThread(const OBSData &settings)
{
	bool connected = false;
	bool stopped = false;

	TestMode testMode;
	testMode.SetVideo(128, 128, 60, 1);

	QMetaObject::invokeMethod(this, "Progress", Q_ARG(int, 0));

	/*
	 * create encoders
	 * create output
	 * test for 10 seconds
	 */

	QMetaObject::invokeMethod(this, "UpdateMessage",
				  Q_ARG(QString, QStringLiteral("")));

	/* -----------------------------------*/
	/* create obs objects                 */
	std::string name = obs_data_get_string(settings, "name");
	const char *serverType = obs_data_get_string(settings, "type");

	std::string vencName = name + ": test_x264";
	OBSEncoder vencoder = obs_video_encoder_create(
		"obs_x264", vencName.c_str(), nullptr, nullptr);
	std::string aencName = name + ": test_acc";
	OBSEncoder aencoder = obs_audio_encoder_create(
		"ffmpeg_aac", aencName.c_str(), nullptr, 0, nullptr);
	std::string streamName = name + ": test_service";
	OBSService service = obs_service_create(serverType, streamName.c_str(),
						nullptr, nullptr);
	obs_encoder_release(vencoder);
	obs_encoder_release(aencoder);
	obs_service_release(service);

	/* -----------------------------------*/
	/* configure settings                 */

	// service: "service", "server", "key"
	// vencoder: "bitrate", "rate_control",
	//           obs_service_apply_encoder_settings
	// aencoder: "bitrate"
	// output: "bind_ip" via main config -> "Output", "BindIP"
	//         obs_output_set_service

	OBSData service_settings = obs_data_create();
	OBSData vencoder_settings = obs_data_create();
	OBSData aencoder_settings = obs_data_create();
	OBSData output_settings = obs_data_create();
	obs_data_release(service_settings);
	obs_data_release(vencoder_settings);
	obs_data_release(aencoder_settings);
	obs_data_release(output_settings);

	std::string key = obs_data_get_string(settings, "key");
	std::string serviceName = obs_data_get_string(settings, "service");

	if (obs_data_get_int(settings, "serviceType") == (int)Service::Twitch) {
		string_depad_key(key);
		key += "?bandwidthtest";
	} else if (serviceName == "Restream.io" ||
		   serviceName == "Restream.io - RTMP") {
		string_depad_key(key);
		key += "?test=true";
	} else if (serviceName == "Restream.io - FTL") {
		string_depad_key(key);
		key += "?test";
	}

	obs_data_set_string(service_settings, "service", serviceName.c_str());
	obs_data_set_string(service_settings, "key", key.c_str());

	obs_data_set_int(vencoder_settings, "bitrate",
			 obs_data_get_int(settings, "startingBitrate"));
	obs_data_set_string(vencoder_settings, "rate_control", "CBR");
	obs_data_set_string(vencoder_settings, "preset", "veryfast");
	obs_data_set_int(vencoder_settings, "keyint_sec", 2);

	obs_data_set_int(aencoder_settings, "bitrate", 32);

	OBSBasic *main = reinterpret_cast<OBSBasic *>(App()->GetMainWindow());
	const char *bind_ip =
		config_get_string(main->Config(), "Output", "BindIP");
	obs_data_set_string(output_settings, "bind_ip", bind_ip);

	/* -----------------------------------*/
	/* determine which servers to test    */

	std::vector<ServerInfo> servers;
	std::string server = obs_data_get_string(settings, "server");
	if (strcmp(obs_data_get_string(settings, "type"), "rtmp_custom") == 0)
		servers.emplace_back(server.c_str(), server.c_str());
	else
		servers = GetServers(serviceName);

	/* just use the first server if it only has one alternate server,
	 * or if using Restream or Nimo TV due to their "auto" servers */
	if (servers.size() < 3 || serviceName.substr(0, 11) == "Restream.io" ||
	    serviceName == "Nimo TV") {
		servers.resize(1);

	} else if (obs_data_get_int(settings, "serviceType") ==
			   (int)Service::Twitch &&
		   wiz->twitchAuto) {
		/* if using Twitch and "Auto" is available, test 3 closest
		 * server */
		servers.erase(servers.begin() + 1);
		servers.resize(3);
	}

	/* -----------------------------------*/
	/* apply service settings             */

	obs_service_update(service, service_settings);
	obs_service_apply_encoder_settings(service, vencoder_settings,
					   aencoder_settings);

	/* -----------------------------------*/
	/* create output                      */

	const char *output_type = obs_service_get_output_type(service);
	if (!output_type)
		output_type = "rtmp_output";

	std::string outputName = name + ": test_stream";
	OBSOutput output = obs_output_create(output_type, outputName.c_str(),
					     nullptr, nullptr);
	obs_output_release(output);
	obs_output_update(output, output_settings);

	const char *audio_codec = obs_output_get_supported_audio_codecs(output);

	if (strcmp(audio_codec, "aac") != 0) {
		const char *id = FindAudioEncoderFromCodec(audio_codec);
		std::string audioName = name + ": test_audio";
		aencoder = obs_audio_encoder_create(id, audioName.c_str(),
						    nullptr, 0, nullptr);
		obs_encoder_release(aencoder);
	}

	/* -----------------------------------*/
	/* connect encoders/services/outputs  */

	obs_encoder_update(vencoder, vencoder_settings);
	obs_encoder_update(aencoder, aencoder_settings);
	obs_encoder_set_video(vencoder, obs_get_video());
	obs_encoder_set_audio(aencoder, obs_get_audio());

	obs_output_set_video_encoder(output, vencoder);
	obs_output_set_audio_encoder(output, aencoder, 0);

	obs_output_set_service(output, service);

	/* -----------------------------------*/
	/* connect signals                    */

	auto on_started = [&]() {
		unique_lock<mutex> lock(m);
		connected = true;
		stopped = false;
		cv.notify_one();
	};

	auto on_stopped = [&]() {
		unique_lock<mutex> lock(m);
		connected = false;
		stopped = true;
		cv.notify_one();
	};

	using on_started_t = decltype(on_started);
	using on_stopped_t = decltype(on_stopped);

	auto pre_on_started = [](void *data, calldata_t *) {
		on_started_t &on_started =
			*reinterpret_cast<on_started_t *>(data);
		on_started();
	};

	auto pre_on_stopped = [](void *data, calldata_t *) {
		on_stopped_t &on_stopped =
			*reinterpret_cast<on_stopped_t *>(data);
		on_stopped();
	};

	signal_handler *sh = obs_output_get_signal_handler(output);
	signal_handler_connect(sh, "start", pre_on_started, &on_started);
	signal_handler_connect(sh, "stop", pre_on_stopped, &on_stopped);

	/* -----------------------------------*/
	/* test servers                       */

	bool success = false;

	for (size_t i = 0; i < servers.size(); i++) {
		auto &server = servers[i];

		connected = false;
		stopped = false;

		int per = int((i + 1) * 100 / servers.size());
		QMetaObject::invokeMethod(this, "Progress", Q_ARG(int, per));
		QMetaObject::invokeMethod(
			this, "UpdateMessage",
			Q_ARG(QString, QTStr(TEST_BW_CONNECTING)
					       .arg(server.name.c_str())));

		obs_data_set_string(service_settings, "server",
				    server.address.c_str());
		obs_service_update(service, service_settings);

		if (!obs_output_start(output))
			continue;

		unique_lock<mutex> ul(m);
		if (cancel) {
			ul.unlock();
			obs_output_force_stop(output);
			return;
		}
		if (!stopped && !connected)
			cv.wait(ul);
		if (cancel) {
			ul.unlock();
			obs_output_force_stop(output);
			return;
		}
		if (!connected)
			continue;

		QMetaObject::invokeMethod(
			this, "UpdateMessage",
			Q_ARG(QString,
			      QTStr(TEST_BW_SERVER).arg(server.name.c_str())));

		/* ignore first 2.5 seconds due to possible buffering skewing
		 * the result */
		cv.wait_for(ul, chrono::milliseconds(2500));
		if (stopped)
			continue;
		if (cancel) {
			ul.unlock();
			obs_output_force_stop(output);
			return;
		}

		/* continue test */
		int start_bytes = (int)obs_output_get_total_bytes(output);
		uint64_t t_start = os_gettime_ns();

		cv.wait_for(ul, chrono::seconds(10));
		if (stopped)
			continue;
		if (cancel) {
			ul.unlock();
			obs_output_force_stop(output);
			return;
		}

		obs_output_stop(output);
		cv.wait(ul);

		uint64_t total_time = os_gettime_ns() - t_start;
		if (total_time == 0)
			total_time = 1;

		int total_bytes =
			(int)obs_output_get_total_bytes(output) - start_bytes;
		uint64_t bitrate = util_mul_div64(
			total_bytes, 8ULL * 1000000000ULL / 1000ULL,
			total_time);
		if (obs_output_get_frames_dropped(output) ||
		    (int)bitrate <
			    (obs_data_get_int(settings, "startingBitrate") *
			     75 / 100)) {
			server.bitrate = (int)bitrate * 70 / 100;
		} else {
			server.bitrate =
				obs_data_get_int(settings, "startingBitrate");
		}

		server.ms = obs_output_get_connect_time_ms(output);
		success = true;
	}

	if (!success) {
		QMetaObject::invokeMethod(this, "Failure",
					  Q_ARG(QString,
						QTStr(TEST_BW_CONNECT_FAIL)));
		return;
	}

	int bestBitrate = 0;
	int bestMS = 0x7FFFFFFF;
	string bestServer;
	string bestServerName;

	for (auto &server : servers) {
		bool close = abs(server.bitrate - bestBitrate) < 400;

		if ((!close && server.bitrate > bestBitrate) ||
		    (close && server.ms < bestMS)) {
			bestServer = server.address;
			bestServerName = server.name;
			bestBitrate = server.bitrate;
			bestMS = server.ms;
		}
	}

	obs_data_set_string(settings, "server", bestServer.c_str());
	obs_data_set_string(settings, "serverName", bestServerName.c_str());
	obs_data_set_int(settings, "idealBitrate", bestBitrate);

	QMetaObject::invokeMethod(this, "NextStage");
}

/* this is used to estimate the lower bitrate limit for a given
 * resolution/fps.  yes, it is a totally arbitrary equation that gets
 * the closest to the expected values */
static long double EstimateBitrateVal(int cx, int cy, int fps_num, int fps_den)
{
	long fps = (long double)fps_num / (long double)fps_den;
	long double areaVal = pow((long double)(cx * cy), 0.85l);
	return areaVal * sqrt(pow(fps, 1.1l));
}

static long double EstimateMinBitrate(int cx, int cy, int fps_num, int fps_den)
{
	long double val = EstimateBitrateVal(1920, 1080, 60, 1) / 5800.0l;
	return EstimateBitrateVal(cx, cy, fps_num, fps_den) / val;
}

static long double EstimateUpperBitrate(int cx, int cy, int fps_num,
					int fps_den)
{
	long double val = EstimateBitrateVal(1280, 720, 30, 1) / 3000.0l;
	return EstimateBitrateVal(cx, cy, fps_num, fps_den) / val;
}

struct Result {
	int cx;
	int cy;
	int fps_num;
	int fps_den;

	inline Result(int cx_, int cy_, int fps_num_, int fps_den_)
		: cx(cx_), cy(cy_), fps_num(fps_num_), fps_den(fps_den_)
	{
	}
};

static void CalcBaseRes(int &baseCX, int &baseCY)
{
	const int maxBaseArea = 1920 * 1200;
	const int clipResArea = 1920 * 1080;

	/* if base resolution unusually high, recalculate to a more reasonable
	 * value to start the downscaling at, based upon 1920x1080's area.
	 *
	 * for 16:9 resolutions this will always change the starting value to
	 * 1920x1080 */
	if ((baseCX * baseCY) > maxBaseArea) {
		long double xyAspect =
			(long double)baseCX / (long double)baseCY;
		baseCY = (int)sqrt((long double)clipResArea / xyAspect);
		baseCX = (int)((long double)baseCY * xyAspect);
	}
}

bool AutoConfigTestPage::TestSoftwareEncoding(const OBSData &settings)
{
	TestMode testMode;
	QMetaObject::invokeMethod(this, "UpdateMessage",
				  Q_ARG(QString, QStringLiteral("")));

	/* -----------------------------------*/
	/* create obs objects                 */

	OBSEncoder vencoder = obs_video_encoder_create("obs_x264", "test_x264",
						       nullptr, nullptr);
	OBSEncoder aencoder = obs_audio_encoder_create("ffmpeg_aac", "test_aac",
						       nullptr, 0, nullptr);
	OBSOutput output =
		obs_output_create("null_output", "null", nullptr, nullptr);
	obs_output_release(output);
	obs_encoder_release(vencoder);
	obs_encoder_release(aencoder);

	/* -----------------------------------*/
	/* configure settings                 */

	OBSData aencoder_settings = obs_data_create();
	OBSData vencoder_settings = obs_data_create();
	obs_data_release(aencoder_settings);
	obs_data_release(vencoder_settings);
	obs_data_set_int(aencoder_settings, "bitrate", 32);

	if (wiz->type != AutoConfig::Type::Recording) {
		obs_data_set_int(vencoder_settings, "keyint_sec", 2);
		obs_data_set_int(vencoder_settings, "bitrate",
				 wiz->idealBitrate);
		obs_data_set_string(vencoder_settings, "rate_control", "CBR");
		obs_data_set_string(vencoder_settings, "profile", "main");
		obs_data_set_string(vencoder_settings, "preset", "veryfast");
	} else {
		obs_data_set_int(vencoder_settings, "crf", 20);
		obs_data_set_string(vencoder_settings, "rate_control", "CRF");
		obs_data_set_string(vencoder_settings, "profile", "high");
		obs_data_set_string(vencoder_settings, "preset", "veryfast");
	}

	/* -----------------------------------*/
	/* apply settings                     */

	obs_encoder_update(vencoder, vencoder_settings);
	obs_encoder_update(aencoder, aencoder_settings);

	/* -----------------------------------*/
	/* connect encoders/services/outputs  */

	obs_output_set_video_encoder(output, vencoder);
	obs_output_set_audio_encoder(output, aencoder, 0);

	/* -----------------------------------*/
	/* connect signals                    */

	auto on_stopped = [&]() {
		unique_lock<mutex> lock(m);
		cv.notify_one();
	};

	using on_stopped_t = decltype(on_stopped);

	auto pre_on_stopped = [](void *data, calldata_t *) {
		on_stopped_t &on_stopped =
			*reinterpret_cast<on_stopped_t *>(data);
		on_stopped();
	};

	signal_handler *sh = obs_output_get_signal_handler(output);
	signal_handler_connect(sh, "deactivate", pre_on_stopped, &on_stopped);

	/* -----------------------------------*/
	/* calculate starting resolution      */

	int baseCX = wiz->baseResolutionCX;
	int baseCY = wiz->baseResolutionCY;
	CalcBaseRes(baseCX, baseCY);

	/* -----------------------------------*/
	/* calculate starting test rates      */

	int pcores = os_get_physical_cores();
	int lcores = os_get_logical_cores();
	int maxDataRate;
	if (lcores > 8 || pcores > 4) {
		/* superb */
		maxDataRate = 1920 * 1200 * 60 + 1000;

	} else if (lcores > 4 && pcores == 4) {
		/* great */
		maxDataRate = 1920 * 1080 * 60 + 1000;

	} else if (pcores == 4) {
		/* okay */
		maxDataRate = 1920 * 1080 * 30 + 1000;

	} else {
		/* toaster */
		maxDataRate = 960 * 540 * 30 + 1000;
	}

	/* -----------------------------------*/
	/* perform tests                      */

	vector<Result> results;
	int i = 0;
	int count = 1;

	auto testRes = [&](int cy, int fps_num, int fps_den, bool force) {
		int per = ++i * 100 / count;
		QMetaObject::invokeMethod(this, "Progress", Q_ARG(int, per));

		if (cy > baseCY)
			return true;

		/* no need for more than 3 tests max */
		if (results.size() >= 3)
			return true;

		if (!fps_num || !fps_den) {
			fps_num = wiz->specificFPSNum;
			fps_den = wiz->specificFPSDen;
		}

		long double fps = ((long double)fps_num / (long double)fps_den);

		int cx = int(((long double)baseCX / (long double)baseCY) *
			     (long double)cy);

		if (!force && wiz->type != AutoConfig::Type::Recording) {
			int est = EstimateMinBitrate(cx, cy, fps_num, fps_den);
			if (est > obs_data_get_int(settings, "idealBitrate"))
				return true;
		}

		long double rate = (long double)cx * (long double)cy * fps;
		if (!force && rate > maxDataRate)
			return true;

		testMode.SetVideo(cx, cy, fps_num, fps_den);

		obs_encoder_set_video(vencoder, obs_get_video());
		obs_encoder_set_audio(aencoder, obs_get_audio());
		obs_encoder_update(vencoder, vencoder_settings);

		obs_output_set_media(output, obs_get_video(), obs_get_audio());

		QString cxStr = QString::number(cx);
		QString cyStr = QString::number(cy);

		QString fpsStr = (fps_den > 1) ? QString::number(fps, 'f', 2)
					       : QString::number(fps, 'g', 2);

		QMetaObject::invokeMethod(
			this, "UpdateMessage",
			Q_ARG(QString,
			      QTStr(TEST_RES_VAL).arg(cxStr, cyStr, fpsStr)));

		unique_lock<mutex> ul(m);
		if (cancel)
			return false;

		if (!obs_output_start(output)) {
			QMetaObject::invokeMethod(this, "Failure",
						  Q_ARG(QString,
							QTStr(TEST_RES_FAIL)));
			return false;
		}

		cv.wait_for(ul, chrono::seconds(5));

		obs_output_stop(output);
		cv.wait(ul);

		int skipped =
			(int)video_output_get_skipped_frames(obs_get_video());
		if (force || skipped <= 10)
			results.emplace_back(cx, cy, fps_num, fps_den);

		return !cancel;
	};

	if (wiz->specificFPSNum && wiz->specificFPSDen) {
		count = 7;
		if (!testRes(2160, 0, 0, false))
			return false;
		if (!testRes(1440, 0, 0, false))
			return false;
		if (!testRes(1080, 0, 0, false))
			return false;
		if (!testRes(720, 0, 0, false))
			return false;
		if (!testRes(480, 0, 0, false))
			return false;
		if (!testRes(360, 0, 0, false))
			return false;
		if (!testRes(240, 0, 0, true))
			return false;
	} else {
		count = 14;
		if (!testRes(2160, 60, 1, false))
			return false;
		if (!testRes(2160, 30, 1, false))
			return false;
		if (!testRes(1440, 60, 1, false))
			return false;
		if (!testRes(1440, 30, 1, false))
			return false;
		if (!testRes(1080, 60, 1, false))
			return false;
		if (!testRes(1080, 30, 1, false))
			return false;
		if (!testRes(720, 60, 1, false))
			return false;
		if (!testRes(720, 30, 1, false))
			return false;
		if (!testRes(480, 60, 1, false))
			return false;
		if (!testRes(480, 30, 1, false))
			return false;
		if (!testRes(360, 60, 1, false))
			return false;
		if (!testRes(360, 30, 1, false))
			return false;
		if (!testRes(240, 60, 1, false))
			return false;
		if (!testRes(240, 30, 1, true))
			return false;
	}

	/* -----------------------------------*/
	/* find preferred settings            */

	int minArea = 960 * 540 + 1000;

	if (!wiz->specificFPSNum && wiz->preferHighFPS && results.size() > 1) {
		Result &result1 = results[0];
		Result &result2 = results[1];

		if (result1.fps_num == 30 && result2.fps_num == 60) {
			int nextArea = result2.cx * result2.cy;
			if (nextArea >= minArea)
				results.erase(results.begin());
		}
	}

	Result result = results.front();
	obs_data_set_int(settings, "resX", result.cx);
	obs_data_set_int(settings, "resY", result.cy);
	obs_data_set_int(settings, "fpsNum", result.fps_num);
	obs_data_set_int(settings, "fpsDen", result.fps_den);

	long double fUpperBitrate = EstimateUpperBitrate(
		result.cx, result.cy, result.fps_num, result.fps_den);

	int upperBitrate = int(floor(fUpperBitrate / 50.0l) * 50.0l);

	if (obs_data_get_int(settings, "videoEncoderType") !=
	    (int)AutoConfig::Encoder::x264) {
		upperBitrate *= 114;
		upperBitrate /= 100;
	}

	if (obs_data_get_int(settings, "idealBitrate") > upperBitrate)
		obs_data_set_int(settings, "idealBitrate", upperBitrate);

	obs_data_set_bool(settings, "softwareTested", true);
	obs_data_set_int(settings, "audio_bitrate",
			 obs_data_get_int(aencoder_settings, "bitrate"));
	return true;
}

void AutoConfigTestPage::FindIdealHardwareResolution(const OBSData &settings)
{
	int baseCX = wiz->baseResolutionCX;
	int baseCY = wiz->baseResolutionCY;
	CalcBaseRes(baseCX, baseCY);

	vector<Result> results;

	int pcores = os_get_physical_cores();
	int maxDataRate;
	if (pcores >= 4) {
		maxDataRate = 1920 * 1200 * 60 + 1000;
	} else {
		maxDataRate = 1280 * 720 * 30 + 1000;
	}

	auto testRes = [&](int cy, int fps_num, int fps_den, bool force) {
		if (cy > baseCY)
			return;

		if (results.size() >= 3)
			return;

		if (!fps_num || !fps_den) {
			fps_num = wiz->specificFPSNum;
			fps_den = wiz->specificFPSDen;
		}

		long double fps = ((long double)fps_num / (long double)fps_den);

		int cx = int(((long double)baseCX / (long double)baseCY) *
			     (long double)cy);

		long double rate = (long double)cx * (long double)cy * fps;
		if (!force && rate > maxDataRate)
			return;

		AutoConfig::Encoder encType =
			(AutoConfig::Encoder)obs_data_get_int(
				settings, "videoEncoderType");
		bool nvenc = encType == AutoConfig::Encoder::NVENC;

		int minBitrate = EstimateMinBitrate(cx, cy, fps_num, fps_den);

		/* most hardware encoders don't have a good quality to bitrate
		 * ratio, so increase the minimum bitrate estimate for them.
		 * NVENC currently is the exception because of the improvements
		 * its made to its quality in recent generations. */
		if (!nvenc)
			minBitrate = minBitrate * 114 / 100;

		if (wiz->type == AutoConfig::Type::Recording)
			force = true;
		if (force ||
		    obs_data_get_int(settings, "idealBitrate") >= minBitrate)
			results.emplace_back(cx, cy, fps_num, fps_den);
	};

	if (wiz->specificFPSNum && wiz->specificFPSDen) {
		testRes(2160, 0, 0, false);
		testRes(1440, 0, 0, false);
		testRes(1080, 0, 0, false);
		testRes(720, 0, 0, false);
		testRes(480, 0, 0, false);
		testRes(360, 0, 0, false);
		testRes(240, 0, 0, true);
	} else {
		testRes(2160, 60, 1, false);
		testRes(2160, 30, 1, false);
		testRes(1440, 60, 1, false);
		testRes(1440, 30, 1, false);
		testRes(1080, 60, 1, false);
		testRes(1080, 30, 1, false);
		testRes(720, 60, 1, false);
		testRes(720, 30, 1, false);
		testRes(480, 60, 1, false);
		testRes(480, 30, 1, false);
		testRes(360, 60, 1, false);
		testRes(360, 30, 1, false);
		testRes(240, 60, 1, false);
		testRes(240, 30, 1, true);
	}

	int minArea = 960 * 540 + 1000;

	if (!wiz->specificFPSNum && wiz->preferHighFPS && results.size() > 1) {
		Result &result1 = results[0];
		Result &result2 = results[1];

		if (result1.fps_num == 30 && result2.fps_num == 60) {
			int nextArea = result2.cx * result2.cy;
			if (nextArea >= minArea)
				results.erase(results.begin());
		}
	}

	Result result = results.front();
	obs_data_set_int(settings, "resX", result.cx);
	obs_data_set_int(settings, "resY", result.cy);
	obs_data_set_int(settings, "fpsNum", result.fps_num);
	obs_data_set_int(settings, "fpsDen", result.fps_den);
}

void AutoConfigTestPage::TestStreamEncoderThread(const OBSData &settings)
{
	bool preferHardware = obs_data_get_bool(settings, "preferHardware");

	if (!obs_data_get_bool(settings, "softwareTested")) {
		if (!preferHardware || !wiz->hardwareEncodingAvailable) {
			if (!TestSoftwareEncoding(settings)) {
				return;
			}
		}
	}

	AutoConfig::Encoder encType;
	if (!obs_data_get_bool(settings, "softwareTested")) {
		if (wiz->nvencAvailable)
			encType = AutoConfig::Encoder::NVENC;
		else if (wiz->qsvAvailable)
			encType = AutoConfig::Encoder::QSV;
		else
			encType = AutoConfig::Encoder::AMD;
	} else {
		encType = AutoConfig::Encoder::x264;
	}
	obs_data_set_int(settings, "videoEncoderType", (int)encType);

	if (preferHardware && !obs_data_get_bool(settings, "softwareTested") &&
	    wiz->hardwareEncodingAvailable) {
		FindIdealHardwareResolution(settings);
	}

	QMetaObject::invokeMethod(this, "NextStage");
}

void AutoConfigTestPage::TestRecordingEncoderThread(const OBSData &settings)
{
	if (!wiz->hardwareEncodingAvailable &&
	    !obs_data_get_bool(settings, "softwareTested")) {
		if (!TestSoftwareEncoding(settings)) {
			return;
		}
	}

	if (wiz->type == AutoConfig::Type::Recording &&
	    wiz->hardwareEncodingAvailable)
		FindIdealHardwareResolution(settings);

	bool recordingOnly = wiz->type == AutoConfig::Type::Recording;

	AutoConfig::Quality quality = AutoConfig::Quality::High;
	AutoConfig::Encoder encType;
	if (wiz->hardwareEncodingAvailable) {
		if (wiz->nvencAvailable)
			encType = AutoConfig::Encoder::NVENC;
		else if (wiz->qsvAvailable)
			encType = AutoConfig::Encoder::QSV;
		else
			encType = AutoConfig::Encoder::AMD;
	} else {
		encType = AutoConfig::Encoder::x264;
	}

	if (encType != AutoConfig::Encoder::NVENC) {
		if (!recordingOnly) {
			encType = AutoConfig::Encoder::Stream;
			quality = AutoConfig::Quality::Stream;
		}
	}

	obs_data_set_int(settings, "recordingQuality", (int)quality);
	obs_data_set_int(settings, "videoEncoderType", (int)encType);

	QMetaObject::invokeMethod(this, "NextStage");
}

#define ENCODER_TEXT(x) "Basic.Settings.Output.Simple.Encoder." x
#define ENCODER_SOFTWARE ENCODER_TEXT("Software")
#define ENCODER_NVENC ENCODER_TEXT("Hardware.NVENC")
#define ENCODER_QSV ENCODER_TEXT("Hardware.QSV")
#define ENCODER_AMD ENCODER_TEXT("Hardware.AMD")

#define QUALITY_SAME "Basic.Settings.Output.Simple.RecordingQuality.Stream"
#define QUALITY_HIGH "Basic.Settings.Output.Simple.RecordingQuality.Small"

void set_closest_res(int &cx, int &cy, struct obs_service_resolution *res_list,
		     size_t count)
{
	int best_pixel_diff = 0x7FFFFFFF;
	int start_cx = cx;
	int start_cy = cy;

	for (size_t i = 0; i < count; i++) {
		struct obs_service_resolution &res = res_list[i];
		int pixel_cx_diff = abs(start_cx - res.cx);
		int pixel_cy_diff = abs(start_cy - res.cy);
		int pixel_diff = pixel_cx_diff + pixel_cy_diff;

		if (pixel_diff < best_pixel_diff) {
			best_pixel_diff = pixel_diff;
			cx = res.cx;
			cy = res.cy;
		}
	}
}

void AutoConfigTestPage::FinalizeResults()
{
	ui->stackedWidget->setCurrentIndex(1);
	setSubTitle(QTStr(SUBTITLE_COMPLETE));

	ClearResults();
	for (auto &test : tests) {
		if (failures.find(test.first) == failures.end()) {
			QWidget *resultWidget =
				CreateResultWidget(test.second, ui->results);
			int index = ui->results->addWidget(resultWidget);
			ui->streams->addItem(obs_data_get_string(test.second,
								 "name"),
					     index);
		}
	}
	ui->streams->model()->sort(0);

	if (failures.size() != 0) {
		QString errors;

		for (auto &failure : failures) {
			errors += failure.second;
		}
		ui->errorLabel->setText(errors);
		ui->showErrors->setVisible(true);
	}

	if (failures.size() == tests.size()) {
		ui->stackedWidget->setCurrentIndex(2);
	} else {
		ui->hideErrors->setVisible(true);
		ui->stackedWidget->setCurrentIndex(1);
		ui->streams->setCurrentIndex(0);
		StreamIndexChanged();
	}
}

QWidget *AutoConfigTestPage::CreateResultWidget(const OBSData &settings,
						QWidget *parent)
{
	QWidget *result = new QWidget(parent);
	QFormLayout *form = new QFormLayout(result);
	form->setContentsMargins(0, 0, 0, 0);
	result->setLayout(form);

	auto encName = [](int enc) -> QString {
		switch (enc) {
		case (int)AutoConfig::Encoder::x264:
			return QTStr(ENCODER_SOFTWARE);
		case (int)AutoConfig::Encoder::NVENC:
			return QTStr(ENCODER_NVENC);
		case (int)AutoConfig::Encoder::QSV:
			return QTStr(ENCODER_QSV);
		case (int)AutoConfig::Encoder::AMD:
			return QTStr(ENCODER_AMD);
		case (int)AutoConfig::Encoder::Stream:
			return QTStr(QUALITY_SAME);
		}

		return QTStr(ENCODER_SOFTWARE);
	};

	auto newLabel = [this](const char *str) -> QLabel * {
		return new QLabel(QTStr(str), this);
	};

	if (wiz->type == AutoConfig::Type::Streaming) {
		const char *serverType = wiz->customServer ? "rtmp_custom"
							   : "rtmp_common";

		OBSService service = obs_service_create(
			serverType, "temp_service", nullptr, nullptr);
		obs_service_release(service);

		OBSData service_settings = obs_data_create();
		OBSData vencoder_settings = obs_data_create();
		obs_data_release(service_settings);
		obs_data_release(vencoder_settings);

		obs_data_set_int(vencoder_settings, "bitrate",
				 obs_data_get_int(settings, "idealBitrate"));
		obs_data_set_string(service_settings, "service",
				    obs_data_get_string(settings, "service"));

		obs_service_update(service, service_settings);
		obs_service_apply_encoder_settings(service, vencoder_settings,
						   nullptr);

		struct obs_service_resolution *res_list;
		size_t res_count;
		int maxFPS;
		int cx = obs_data_get_int(settings, "resX");
		int cy = obs_data_get_int(settings, "resY");

		obs_service_get_supported_resolutions(service, &res_list,
						      &res_count);
		obs_service_get_max_fps(service, &maxFPS);

		if (res_list) {
			set_closest_res(cx, cy, res_list, res_count);
			obs_data_set_int(settings, "resX", cx);
			obs_data_set_int(settings, "resY", cy);
		}
		if (maxFPS) {
			double idealFPS =
				(double)obs_data_get_int(settings, "fpsNum") /
				(double)obs_data_get_int(settings, "fpsDen");
			if (idealFPS > (double)maxFPS) {
				wiz->idealFPSNum = maxFPS;
				wiz->idealFPSDen = 1;
			}
		}

		obs_data_set_int(settings, "idealBitrate",
				 (int)obs_data_get_int(vencoder_settings,
						       "bitrate"));

		form->addRow(new QLabel(QString("Name"), result),
			     new QLabel(obs_data_get_string(settings, "name"),
					result));
		if (strcmp(obs_data_get_string(settings, "type"),
			   "rtmp_common") == 0) {
			form->addRow(
				newLabel("Basic.AutoConfig.StreamPage.Service"),
				new QLabel(obs_data_get_string(settings,
							       "service"),
					   result));
		} else {
			form->addRow(
				newLabel("Basic.AutoConfig.StreamPage.Service"),
				new QLabel("<Custom>", result));
		}
		form->addRow(
			newLabel("Basic.AutoConfig.StreamPage.Server"),
			new QLabel(obs_data_get_string(settings, "serverName"),
				   result));
		form->addRow(newLabel("Basic.Settings.Output.VideoBitrate"),
			     new QLabel(QString::number(obs_data_get_int(
						settings, "idealBitrate")),
					result));
		form->addRow(newLabel(TEST_RESULT_SE),
			     new QLabel(encName(obs_data_get_int(
						settings, "videoEncoderType")),
					result));
	}

	QString baseRes =
		QString("%1x%2").arg(QString::number(wiz->baseResolutionCX),
				     QString::number(wiz->baseResolutionCY));
	QString scaleRes = QString("%1x%2").arg(
		QString::number(obs_data_get_int(settings, "resX")),
		QString::number(obs_data_get_int(settings, "resY")));

	if (wiz->type == AutoConfig::Type::Recording) {
		if (obs_data_get_int(settings, "videoEncoderType") !=
			    (int)AutoConfig::Encoder::Stream ||
		    obs_data_get_int(settings, "recordingQuality") !=
			    (int)AutoConfig::Quality::Stream)
			form->addRow(newLabel(TEST_RESULT_RE),
				     new QLabel(encName(obs_data_get_int(
							settings,
							"videoEncoderType")),
						result));
		QString recQuality;

		switch (obs_data_get_int(settings, "recordingQuality")) {
		case (int)AutoConfig::Quality::High:
			recQuality = QTStr(QUALITY_HIGH);
			break;
		case (int)AutoConfig::Quality::Stream:
			recQuality = QTStr(QUALITY_SAME);
			break;
		}
		form->addRow(
			newLabel(
				"Basic.Settings.Output.Simple.RecordingQuality"),
			new QLabel(recQuality, result));
	}

	long double idealFPSNum =
		(long double)obs_data_get_int(settings, "fpsNum");
	long double idealFPSDen =
		(long double)obs_data_get_int(settings, "fpsDen");

	long double fps = idealFPSNum / idealFPSDen;

	QString fpsStr = (idealFPSDen > 1) ? QString::number(fps, 'f', 2)
					   : QString::number(fps, 'g', 2);

	form->addRow(newLabel("Basic.Settings.Video.BaseResolution"),
		     new QLabel(baseRes, result));
	form->addRow(newLabel("Basic.Settings.Video.ScaledResolution"),
		     new QLabel(scaleRes, result));
	form->addRow(newLabel("Basic.Settings.Video.FPS"),
		     new QLabel(fpsStr, result));

	return result;
}

void AutoConfigTestPage::ClearResults()
{
	for (int i = 0; i < (int)ui->results->count(); i++) {
		QWidget *result = ui->results->widget(i);
		ui->results->removeWidget(result);
		delete result;
	}
	ui->streams->clear();
	ui->showErrors->setVisible(false);
	ui->hideErrors->setVisible(false);
	ui->errorLabel->setText(QString());
}

void AutoConfigTestPage::StreamIndexChanged()
{
	int index = ui->streams->currentData().toInt();
	ui->results->setCurrentIndex(index);
}

void AutoConfigTestPage::OnErrorButtonClicked()
{
	if (QObject::sender() == ui->showErrors)
		ui->stackedWidget->setCurrentIndex(2);
	else if (QObject::sender() == ui->hideErrors)
		ui->stackedWidget->setCurrentIndex(1);
}

#define STARTING_SEPARATOR \
	"\n==== Auto-config wizard testing commencing ======\n"
#define STOPPING_SEPARATOR \
	"\n==== Auto-config wizard testing stopping ========\n"
#define ERROR_FORMAT \
	"---------------------------------------------------\n\
	 Stream: %1 (Service: %2)\n\
	 Error: \n\
	 %3\n\
	 ---------------------------------------------------\n"

void AutoConfigTestPage::NextStage()
{
	if (testThread.joinable())
		testThread.join();
	if (cancel)
		return;

	ui->subProgressLabel->setText(QString());

	/* If all tests were run, exit */
	if (currentTest == tests.end() ||
	    tests.size() == completeTests.size() + failures.size()) {
		stage = Stage::Finished;
		emit completeChanged();
		emit Finished();
		return;
	}

	/* make it skip to bandwidth stage if only set to config recording */
	if (stage == Stage::Starting) {
		if (!started) {
			blog(LOG_INFO, STARTING_SEPARATOR);
			started = true;
		}

		if (wiz->type != AutoConfig::Type::Streaming) {
			stage = Stage::StreamEncoder;
		} else if (!wiz->bandwidthTest) {
			stage = Stage::BandwidthTest;
		}
	}

	if (stage == Stage::Starting) {
		stage = Stage::BandwidthTest;
		StartBandwidthStage(currentTest->second);

	} else if (stage == Stage::BandwidthTest) {
		stage = Stage::StreamEncoder;
		StartStreamEncoderStage(currentTest->second);
	} else if (stage == Stage::StreamEncoder) {
		stage = Stage::RecordingEncoder;
		StartRecordingEncoderStage(currentTest->second);

	} else {
		completeTests.push_back(currentTest->first);
		currentTest++;

		stage = Stage::Starting;
		started = false;

		QMetaObject::invokeMethod(this, "NextStage");
	}
}

void AutoConfigTestPage::UpdateMessage(QString message)
{
	ui->subProgressLabel->setText(message);
}

void AutoConfigTestPage::Failure(QString message)
{
	QString current = ui->errorLabel->text();
	failures[currentTest->first] = message;

	current +=
		QString(ERROR_FORMAT)
			.arg(obs_data_get_string(currentTest->second, "name"))
			.arg(obs_data_get_string(currentTest->second,
						 "service"))
			.arg(message);

	stage = Stage::Failed;
	QMetaObject::invokeMethod(this, "NextStage");
}

void AutoConfigTestPage::Progress(int percentage)
{
	ui->progressBar->setValue(percentage);
}

AutoConfigTestPage::AutoConfigTestPage(QWidget *parent)
	: QWizardPage(parent), ui(new Ui_AutoConfigTestPage)
{
	ui->setupUi(this);
	setTitle(QTStr("Basic.AutoConfig.TestPage"));
	setSubTitle(QTStr(SUBTITLE_TESTING));
	setCommitPage(true);

	connect(this, SIGNAL(StageChanged(QString, QString)), this,
		SLOT(UpdateStage(QString, QString)));
	connect(ui->streams, SIGNAL(currentIndexChanged(int)), this,
		SLOT(StreamIndexChanged()));
	connect(this, SIGNAL(Finished()), this, SLOT(FinalizeResults()));

	connect(ui->showErrors, SIGNAL(clicked()), this,
		SLOT(OnErrorButtonClicked()));
	connect(ui->hideErrors, SIGNAL(clicked()), this,
		SLOT(OnErrorButtonClicked()));
}

AutoConfigTestPage::~AutoConfigTestPage()
{
	delete ui;

	if (testThread.joinable()) {
		{
			unique_lock<mutex> ul(m);
			cancel = true;
			cv.notify_one();
		}
		testThread.join();
	}

	if (started)
		blog(LOG_INFO, STOPPING_SEPARATOR);
}

void AutoConfigTestPage::initializePage()
{
	if (wiz->type == AutoConfig::Type::VirtualCam) {
		wiz->idealResolutionCX = wiz->baseResolutionCX;
		wiz->idealResolutionCY = wiz->baseResolutionCY;
		wiz->idealFPSNum = 30;
		wiz->idealFPSDen = 1;
		stage = Stage::Finished;
	} else {
		stage = Stage::Starting;
	}

	setSubTitle(QTStr(SUBTITLE_TESTING));
	cancel = false;
	DeleteLayout(results);
	results = new QFormLayout();
	results->setContentsMargins(0, 0, 0, 0);
	ui->finishPageLayout->insertLayout(1, results);
	ui->stackedWidget->setCurrentIndex(0);

	for (auto &config : wiz->streamPage->GetConfigs()) {
		if (obs_data_get_int(config.second, "output_id") == -1 &&
		    obs_data_get_bool(config.second, "bwtest")) {
			OBSData testConfig = obs_data_create();
			obs_data_release(testConfig);
			obs_data_apply(testConfig, config.second);
			obs_data_set_bool(testConfig, "softwareTested", false);

			tests[config.first] = testConfig;
		}
	}
	currentTest = tests.begin();
	NextStage();
}

bool AutoConfigTestPage::validatePage()
{
	std::map<int, OBSData> success;

	for (auto &id : completeTests) {
		success.insert({id, tests[id]});
		obs_data_set_bool(wiz->serviceConfigs[id], "tested", true);
	}

	wiz->successfulTests = success;
	return true;
}

void AutoConfigTestPage::cleanupPage()
{
	if (testThread.joinable()) {
		{
			unique_lock<mutex> ul(m);
			cancel = true;
			cv.notify_one();
		}
		testThread.join();
	}
}

bool AutoConfigTestPage::isComplete() const
{
	return stage == Stage::Finished;
}

int AutoConfigTestPage::nextId() const
{
	return -1;
}
