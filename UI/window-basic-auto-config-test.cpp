#include <chrono>

#include <QFormLayout>
#include <QVBoxLayout>

#include <obs.hpp>
#include <map>
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

Test::Test(const OBSData &settings_, 
	  AutoConfigTestPage *parent,
	  AutoConfig::Type type_) : QWidget(parent), type(type_)  {
	testPage = parent;

	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);

	settings = obs_data_create();
	obs_data_release(settings);
	obs_data_apply(settings, settings_);
	testPage->wizard()->Lock();

	name = QString(obs_data_get_string(settings, "name"));
	
	baseCX = testPage->wizard()->baseResolutionCX;
	baseCY = testPage->wizard()->baseResolutionCY;
	type = testPage->wizard()->type;
	specFpsNum = testPage->wizard()->specificFPSNum;
	specFpsDen = testPage->wizard()->specificFPSDen;
	preferHighFps = testPage->wizard()->preferHighFPS;
	enc = testPage->wizard()->streamingEncoder;

	testPage->wizard()->Unlock();

	progress = new QProgressBar(this);
	progressLabel = new QLabel(name, this);
	progressLabel->setStyleSheet(" QLabel { font-weight: bold; } ");
	subProgressLabel = new QLabel(this);

	layout->addWidget(progressLabel);
	layout->addWidget(subProgressLabel);
	layout->addWidget(progress);

	this->setLayout(layout);

	connect(this, SIGNAL(Failure(int, QString&)), testPage, 
		SLOT(Failure(int, QString&)));
	connect(this, SIGNAL(Finished(OBSData&)), testPage, 
		SLOT(TestComplete(OBSData&)));

	connect(testPage, SIGNAL(StartBandwidthTests()), this, SLOT(StartTest()));
	connect(testPage, SIGNAL(CancelTests()), this, SLOT(CancelTest()));
}

Test::~Test() {
	if (progress)
		delete progress;
	if (progressLabel)
		delete progressLabel;
	if (subProgressLabel)
		delete subProgressLabel;
	
}

void Test::StartBandwidthStage()
{
	progressLabel->setText(name + QString(" ") + QTStr(TEST_BW));
	testThread = std::thread([this]() { TestBandwidthThread(); });
}

void Test::StartStreamEncoderStage()
{
	progressLabel->setText(name + QString(" ") + QTStr(TEST_SE));
	testThread = std::thread([this]() { TestStreamEncoderThread(); });
}

void Test::StartRecordingEncoderStage() {
	progressLabel->setText(name + QString(" ") + QTStr(TEST_RE));
	testThread = std::thread([this]() { TestRecordingEncoderThread(); });
}

void Test::GetServers(std::vector<ServerInfo> &servers,
				    const std::string &server)
{
	OBSData settings = obs_data_create();
	obs_data_release(settings);
	obs_data_set_string(settings, "service", server.c_str());

	obs_properties_t *ppts = obs_get_service_properties("rtmp_common");
	obs_property_t *p = obs_properties_get(ppts, "service");
	obs_property_modified(p, settings);

	p = obs_properties_get(ppts, "server");
	size_t count = obs_property_list_item_count(p);
	servers.reserve(count);

	for (size_t i = 0; i < count; i++) {
		const char *name = obs_property_list_item_name(p, i);
		const char *server = obs_property_list_item_string(p, i);

		if (CanTestServer(name)) {
			ServerInfo info(name, server);
			servers.push_back(info);
		}
	}

	obs_properties_destroy(ppts);
}

bool Test::CanTestServer(const char *server)
{	
	bool testRegions = 
		(obs_data_get_int(settings, "serviceType") == (int)Service::Twitch ||
		obs_data_get_int(settings, "serviceType") == (int)Service::Smashcast);
	testRegions = obs_data_get_int(settings, "serviceType") == (int)Service::Twitch ?
		      !testPage->wizard()->hasTwitchAuto() :
		      testRegions;

	bool regionUS = obs_data_get_bool(settings, "regionUS");
	bool regionEU = obs_data_get_bool(settings, "regionEU");
	bool regionAsia = obs_data_get_bool(settings, "regionAsia");
	bool regionOther = obs_data_get_bool(settings, "regionOther");

	if (!testRegions || (regionUS && regionEU && regionAsia && regionOther))
		return true;

	if (obs_data_get_int(settings, "serviceType") == (int)Service::Twitch) {
		if (astrcmp_n(server, "US West:", 8) == 0 ||
		    astrcmp_n(server, "US East:", 8) == 0 ||
		    astrcmp_n(server, "US Central:", 11) == 0) {
			return regionUS;
		} else if (astrcmp_n(server, "EU:", 3) == 0) {
			return regionEU;
		} else if (astrcmp_n(server, "Asia:", 5) == 0) {
			return regionAsia;
		} else if (regionOther) {
			return true;
		}
	} else if (obs_data_get_int(settings, "serviceType") == 
		   (int)Service::Smashcast) {
		if (strcmp(server, "Default") == 0) {
			return true;
		} else if (astrcmp_n(server, "US-West:", 8) == 0 ||
			   astrcmp_n(server, "US-East:", 8) == 0) {
			return regionUS;
		} else if (astrcmp_n(server, "EU-", 3) == 0) {
			return regionEU;
		} else if (astrcmp_n(server, "South Korea:", 12) == 0 ||
			   astrcmp_n(server, "Asia:", 5) == 0 ||
			   astrcmp_n(server, "China:", 6) == 0) {
			return regionAsia;
		} else if (regionOther) {
			return true;
		}
	} else {
		return true;
	}

	return false;
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

void Test::TestBandwidthThread()
{
	bool connected = false;
	bool stopped = false;

	TestMode testMode;
	testMode.SetVideo(128, 128, 60, 1);

	Progress(0);

	/*
	 * create encoders
	 * create output
	 * test for 10 seconds
	 */

	UpdateMessage("");

	/* -----------------------------------*/
	/* create obs objects                 */

	const char *serverType = obs_data_get_string(settings, "type");
	std::string vencName, aencName, servName = obs_data_get_string(settings, "name");
	vencName.append("-test_x264");
	aencName.append("-test_aac");
	servName.append("-test_service");
	OBSEncoder vencoder = obs_video_encoder_create("obs_x264", vencName.c_str(),
						       nullptr, nullptr);
	OBSEncoder aencoder = obs_audio_encoder_create("ffmpeg_aac", aencName.c_str(),
						       nullptr, 0, nullptr);
	OBSService service = obs_service_create(serverType, servName.c_str(),
						nullptr, nullptr);
	obs_encoder_release(vencoder);
	obs_encoder_release(aencoder);
	obs_service_release(service);

	/* -----------------------------------*/
	/* configure settings                 */
	// vencoder: "bitrate", "rate_control",
	//           obs_service_apply_encoder_settings
	// aencoder: "bitrate"
	// output: "bind_ip" via main config -> "Output", "BindIP"
	//         obs_output_set_service

	OBSData vencoder_settings = obs_data_create();
	OBSData aencoder_settings = obs_data_create();
	OBSData output_settings = obs_data_create();
	
	obs_data_release(vencoder_settings);
	obs_data_release(aencoder_settings);
	obs_data_release(output_settings);

	std::string key = obs_data_get_string(settings, "key");
	std::string serviceName = obs_data_get_string(settings, "service");
	if (obs_data_get_int(settings, "serviceType") == 
	    (int)Service::Twitch) {
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

	obs_data_set_string(settings, "key", key.c_str());

	obs_data_set_int(vencoder_settings, "bitrate",
			 obs_data_get_int(settings, "bitrate"));
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
		GetServers(servers, serviceName);

	/* just use the first server if it only has one alternate server,
	 * or if using Mixer or Restream due to their "auto" servers */
	if (servers.size() < 3 || serviceName == "Mixer.com - FTL" ||
	    serviceName.substr(0, 11) == "Restream.io") {
		servers.resize(1);

	} else if (obs_data_get_int(settings, "serviceType") == 
		   (int)Service::Twitch &&
		   testPage->wizard()->hasTwitchAuto()) {
		/* if using Twitch and "Auto" is available, test 3 closest
		 * server */
		servers.erase(servers.begin() + 1);
		servers.resize(3);
	}

	/* -----------------------------------*/
	/* apply service settings             */

	obs_service_update(service, settings);
	obs_service_apply_encoder_settings(service, vencoder_settings,
					   aencoder_settings);

	/* -----------------------------------*/
	/* create output                      */

	const char *output_type = obs_service_get_output_type(service);
	if (!output_type)
		output_type = "rtmp_output";

	std::string outputName = obs_data_get_string(settings, "name");
	std::string audTestName = obs_data_get_string(settings, "name");

	outputName.append("-test_stream");
	audTestName.append("-test_audio");

	OBSOutput output =
		obs_output_create(output_type, outputName.c_str(), nullptr, nullptr);
	obs_output_release(output);
	obs_output_update(output, output_settings);

	const char *audio_codec = obs_output_get_supported_audio_codecs(output);

	if (strcmp(audio_codec, "aac") != 0) {
		const char *id = FindAudioEncoderFromCodec(audio_codec);
		aencoder = obs_audio_encoder_create(id, audTestName.c_str(), nullptr,
						    0, nullptr);
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
		Progress(per);
		UpdateMessage(QTStr(TEST_BW_CONNECTING).arg(server.name.c_str()));
		obs_data_set_string(settings, "server", 
				    server.address.c_str());
		obs_service_update(service, settings);

		if (!obs_output_start(output))
			continue;

		unique_lock<mutex> ul(m);
		if (cancel) {
			ul.unlock();
			obs_output_force_stop(output);
			emit Failure(obs_data_get_int(settings, "id"), "Cancelled");
			return;
		}
		if (!stopped && !connected)
			cv.wait(ul);
		if (cancel) {
			ul.unlock();
			obs_output_force_stop(output);
			emit Failure(obs_data_get_int(settings, "id"), "Cancelled");
			return;
		}
		if (!connected)
			continue;

		UpdateMessage(QTStr(TEST_BW_SERVER).arg(server.name.c_str()));

		/* ignore first 2.5 seconds due to possible buffering skewing
		 * the result */
		cv.wait_for(ul, chrono::milliseconds(2500));
		if (stopped)
			continue;
		if (cancel) {
			ul.unlock();
			obs_output_force_stop(output);
			emit Failure(obs_data_get_int(settings, "id"), "Cancelled");
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
			emit Failure(obs_data_get_int(settings, "id"), "Cancelled");
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
		    (int)bitrate < (obs_data_get_int(settings, "startingBitrate") * 75 / 100)) {
			server.bitrate = (int)bitrate * 70 / 100;
		} else {
			server.bitrate = obs_data_get_int(settings, "startingBitrate");
		}

		server.ms = obs_output_get_connect_time_ms(output);
		success = true;
	}

	if (!success) {
		Failure(QTStr(TEST_BW_CONNECT_FAIL));
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

bool Test::TestSoftwareEncoding()
{
	TestMode testMode;
	UpdateMessage("");

	/* -----------------------------------*/
	/* create obs objects                 */
	std::string vencName, aencName, outputName = obs_data_get_string(settings, "name");
	vencName.append("-test_x264");
	aencName.append("-test_aac");
	outputName.append("-null_output");
	OBSEncoder vencoder = obs_video_encoder_create("obs_x264", vencName.c_str(),
						       nullptr, nullptr);
	OBSEncoder aencoder = obs_audio_encoder_create("ffmpeg_aac", aencName.c_str(),
						       nullptr, 0, nullptr);
	OBSOutput output =
		obs_output_create("null_output", outputName.c_str(), nullptr, nullptr);
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

	if (type != AutoConfig::Type::Recording) {
		obs_data_set_int(vencoder_settings, "keyint_sec", 2);
		obs_data_set_int(vencoder_settings, "bitrate",
				 obs_data_get_int(settings, "idealBitrate"));
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
		Progress(per);
		//QMetaObject::invokeMethod(this, "Progress", Q_ARG(int, per));

		if (cy > baseCY)
			return true;

		/* no need for more than 3 tests max */
		if (results.size() >= 3)
			return true;

		if (!fps_num || !fps_den) {
			fps_num = specFpsNum;
			fps_den = specFpsDen;
		}

		long double fps = ((long double)fps_num / (long double)fps_den);

		int cx = int(((long double)baseCX / (long double)baseCY) *
			     (long double)cy);

		if (!force && type != AutoConfig::Type::Recording) {
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
		UpdateMessage(QTStr(TEST_RES_VAL).arg(cxStr, cyStr, fpsStr));

		unique_lock<mutex> ul(m);
		if (cancel)
			return false;

		if (!obs_output_start(output)) {
			Failure(QTStr(TEST_RES_FAIL));
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

	if (specFpsNum && specFpsDen) {
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

	if (specFpsNum != 0 && preferHighFps && results.size() > 1) {
		Result &result1 = results[0];
		Result &result2 = results[1];

		if (result1.fps_num == 30 && result2.fps_num == 60) {
			int nextArea = result2.cx * result2.cy;
			if (nextArea >= minArea)
				results.erase(results.begin());
		}
	}

	Result result = results.front();
	obs_data_set_int(settings, "idealResX", result.cx);
	obs_data_set_int(settings, "idealResY", result.cy);
	obs_data_set_int(settings, "idealFPSNum", result.fps_num);
	obs_data_set_int(settings, "idealResDen", result.fps_den);

	long double fUpperBitrate = EstimateUpperBitrate(
		result.cx, result.cy, result.fps_num, result.fps_den);

	int upperBitrate = int(floor(fUpperBitrate / 50.0l) * 50.0l);

	if (enc != AutoConfig::Encoder::x264) {
		upperBitrate *= 114;
		upperBitrate /= 100;
	}

	if (obs_data_get_int(settings, "idealBitrate") > upperBitrate)
		obs_data_set_int(settings, "idealBitrate", upperBitrate);

	softwareTested = true;
	return true;
}

void Test::FindIdealHardwareResolution()
{
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
			fps_num = specFpsNum;
			fps_den = specFpsDen;
		}

		long double fps = ((long double)fps_num / (long double)fps_den);

		int cx = int(((long double)baseCX / (long double)baseCY) *
			     (long double)cy);

		long double rate = (long double)cx * (long double)cy * fps;
		if (!force && rate > maxDataRate)
			return;

		bool nvenc = enc == AutoConfig::Encoder::NVENC;

		int minBitrate = EstimateMinBitrate(cx, cy, fps_num, fps_den);

		/* most hardware encoders don't have a good quality to bitrate
		 * ratio, so increase the minimum bitrate estimate for them.
		 * NVENC currently is the exception because of the improvements
		 * its made to its quality in recent generations. */
		if (!nvenc)
			minBitrate = minBitrate * 114 / 100;

		if (type == AutoConfig::Type::Recording)
			force = true;
		if (force || 
		    obs_data_get_int(settings, "idealBitrate") >= minBitrate){
			results.emplace_back(cx, cy, fps_num, fps_den);
		}
	};

	if (specFpsNum && specFpsDen) {
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

	if (!specFpsNum && preferHighFps && results.size() > 1) {
		Result &result1 = results[0];
		Result &result2 = results[1];

		if (result1.fps_num == 30 && result2.fps_num == 60) {
			int nextArea = result2.cx * result2.cy;
			if (nextArea >= minArea)
				results.erase(results.begin());
		}
	}

	Result result = results.front();
	obs_data_set_int(settings, "idealResX", result.cx);
	obs_data_set_int(settings, "idealResY", result.cy);
	obs_data_set_int(settings, "idealFPSNum", result.fps_num);
	obs_data_set_int(settings, "idealResDen", result.fps_den);
}

void Test::TestStreamEncoderThread()
{
	bool preferHardware = obs_data_get_bool(settings, "preferHardware");
	if (!softwareTested) {
		if (!preferHardware || 
		    !testPage->wizard()->hardwareEncodingAvailable) {
			if (!TestSoftwareEncoding()) {
				return;
			}
		}
	}

	if (!softwareTested) {
		if (testPage->wizard()->nvencAvailable) {
			enc = AutoConfig::Encoder::NVENC;
			obs_data_set_string(settings, "streamEncoder",
					    "ffmpeg_nvenc");
		}
		else if (testPage->wizard()->qsvAvailable) {
			enc = AutoConfig::Encoder::QSV;
			obs_data_set_string(settings, "streamEncoder",
					    "obs_qsv11");
		}
		else {
			enc = AutoConfig::Encoder::AMD;
			obs_data_set_string(settings, "streamEncoder",
					    "amd_amf_h264");
		}
	} else {
		enc = AutoConfig::Encoder::x264;
		obs_data_set_string(settings, "streamEncoder",
				    "obs_x264");
	}

	if (preferHardware &&
	    !softwareTested &&
	    testPage->wizard()->hardwareEncodingAvailable)
		FindIdealHardwareResolution();

	QMetaObject::invokeMethod(this, "NextStage");
}

void Test::FinalizeResults() {
	emit Finished(settings);
}

void Test::TestRecordingEncoderThread()
{
	if (!testPage->wizard()->hardwareEncodingAvailable &&
	    !softwareTested) {
		if (!TestSoftwareEncoding()) {
			return;
		}
	}

	if (type == AutoConfig::Type::Recording &&
	    !testPage->wizard()->hardwareEncodingAvailable)
		FindIdealHardwareResolution();

	recordingQuality = AutoConfig::Quality::High;

	bool recordingOnly = type == AutoConfig::Type::Recording;

	if (testPage->wizard()->hardwareEncodingAvailable) {
		if (testPage->wizard()->nvencAvailable) {
			enc = AutoConfig::Encoder::NVENC;
			obs_data_set_string(settings, "recordingEncoder",
					    "ffmpeg_nvenc");
		}
		else if (testPage->wizard()->qsvAvailable) {
			enc = AutoConfig::Encoder::QSV;
			obs_data_set_string(settings, "recordingEncoder",
					    "obs_qsv11");
		}
		else {
			enc = AutoConfig::Encoder::AMD;
			obs_data_set_string(settings, "recordingEncoder",
					    "amd_amf_h264");
		}
	} else {
		enc = AutoConfig::Encoder::x264;
		obs_data_set_string(settings, "recordingEncoder",
				    "obs_x264");
		
	}

	if (enc != AutoConfig::Encoder::NVENC) {
		if (!recordingOnly) {
			enc = AutoConfig::Encoder::Stream;
			recordingQuality = AutoConfig::Quality::Stream;
		}
	}

	QMetaObject::invokeMethod(this, "NextStage");
}

#define ENCODER_TEXT(x) "Basic.Settings.Output.Simple.Encoder." x
#define ENCODER_SOFTWARE ENCODER_TEXT("Software")
#define ENCODER_NVENC ENCODER_TEXT("Hardware.NVENC")
#define ENCODER_QSV ENCODER_TEXT("Hardware.QSV")
#define ENCODER_AMD ENCODER_TEXT("Hardware.AMD")

#define QUALITY_SAME "Basic.Settings.Output.Simple.RecordingQuality.Stream"
#define QUALITY_HIGH "Basic.Settings.Output.Simple.RecordingQuality.Small"

QFormLayout *AutoConfigTestPage::ShowResults(Test *test, QWidget *parent) {
	QFormLayout *form = new QFormLayout(parent);

	auto encName = [](AutoConfig::Encoder enc) -> QString {
		switch (enc) {
		case AutoConfig::Encoder::x264:
			return QTStr(ENCODER_SOFTWARE);
		case AutoConfig::Encoder::NVENC:
			return QTStr(ENCODER_NVENC);
		case AutoConfig::Encoder::QSV:
			return QTStr(ENCODER_QSV);
		case AutoConfig::Encoder::AMD:
			return QTStr(ENCODER_AMD);
		case AutoConfig::Encoder::Stream:
			return QTStr(QUALITY_SAME);
		}
		return QTStr(ENCODER_SOFTWARE);
	};
	
	auto newLabel = [parent](const char *str) -> QLabel * {
		return new QLabel(QTStr(str), parent);
	};

	if (test->type == AutoConfig::Type::Streaming) {
		const char* type = obs_data_get_string(test->settings, "type");

		OBSService service = obs_service_create(
			type, "temp_service", nullptr, nullptr);
		obs_service_release(service);

		OBSData service_settings = obs_data_create();
		OBSData vencoder_settings = obs_data_create();
		obs_data_release(service_settings);
		obs_data_release(vencoder_settings);

		obs_data_set_int(vencoder_settings, "bitrate",
			obs_data_get_int(test->settings, "idealBitrate"));

		obs_data_set_string(service_settings, "service",
			obs_data_get_string(test->settings, "service"));
		obs_service_update(service, service_settings);
		obs_service_apply_encoder_settings(service, vencoder_settings,
						   nullptr);

		obs_data_set_int(test->settings, "idealBitrate",
			(int)obs_data_get_int(vencoder_settings, "bitrate"));

		form->addRow(
			new QLabel("Stream Name"),
			new QLabel(obs_data_get_string(test->settings, 
							"serverName"),
				   parent));
		
		if (strcmp(obs_data_get_string(test->settings, "type"),
			   "rtmp_custom") != 0) {
			form->addRow(
				newLabel("Basic.AutoConfig.StreamPage.Service"),
				new QLabel(obs_data_get_string(test->settings, 
								"service"),
				   	   parent));
		}
		form->addRow(
			newLabel("Basic.AutoConfig.StreamPage.Server"),
			new QLabel(obs_data_get_string(test->settings, 
							"serverName"), 
				   parent));
		form->addRow(
			newLabel("Basic.Settings.Output.VideoBitrate"),
			new QLabel(QString::number(
					obs_data_get_int(test->settings, 
							 "idealBitrate")),
				   parent));
		form->addRow(newLabel(TEST_RESULT_SE),
			new QLabel(encName(wiz->streamingEncoder),
				   parent));
	}
	QString resX = QString::number(obs_data_get_int(test->settings,
							"idealResX"));
	QString resY = QString::number(obs_data_get_int(test->settings,
							"idealResY"));
	QString baseRes = QString("%1x%2").arg(test->baseCX, test->baseCY);
	QString scaleRes = QString("%1x%2").arg(resX, resY);

	if (test->type == AutoConfig::Type::Recording) {
		if (wiz->recordingEncoder != AutoConfig::Encoder::Stream ||
		    wiz->recordingQuality != AutoConfig::Quality::Stream) {
			form->addRow(newLabel(TEST_RESULT_RE),
				new QLabel(encName(wiz->recordingEncoder),
						parent));

			QString recQuality;

			switch (wiz->recordingQuality) {
			case AutoConfig::Quality::High:
				recQuality = QTStr(QUALITY_HIGH);
				break;
			case AutoConfig::Quality::Stream:
				recQuality = QTStr(QUALITY_SAME);
				break;
			}

			form->addRow(
				newLabel("Basic.Settings.Output.Simple.RecordingQuality"),
				new QLabel(recQuality, parent));
		}
	}
	long double fps =
		(long double)wiz->idealFPSNum / (long double)wiz->idealFPSDen;

	QString fpsStr = (wiz->idealFPSDen > 1) ? QString::number(fps, 'f', 2)
						: QString::number(fps, 'g', 2);

	form->addRow(newLabel("Basic.Settings.Video.BaseResolution"),
		     new QLabel(baseRes, parent));
	form->addRow(newLabel("Basic.Settings.Video.ScaledResolution"),
		     new QLabel(scaleRes, parent));
	form->addRow(newLabel("Basic.Settings.Video.FPS"),
		     new QLabel(fpsStr, parent));
	return form;
}

void AutoConfigTestPage::FinalizeResults()
{
	ui->stackedWidget->setCurrentIndex(1);
	setSubTitle(QTStr(SUBTITLE_COMPLETE));

	for (auto &test : tests) {
		QWidget *result = new QWidget(ui->stackedWidget);
		QFormLayout *form = ShowResults(test.second, result);
		result->setLayout(form);
		ui->results->addWidget(result);
	}
}

void AutoConfigTestPage::on_resultsLeft_Clicked() {
	int currentIndex = ui->results->currentIndex();

	if (currentIndex > 0 && ui->results->count() != 0)
		ui->results->setCurrentIndex(currentIndex - 1);
}

void AutoConfigTestPage::on_resultsRight_Clicked() {
	int currentIndex = ui->results->currentIndex();

	if (currentIndex < ui->results->count() - 1 && ui->results->count() != 0)
		ui->results->setCurrentIndex(currentIndex + 1);
}

#define STARTING_SEPARATOR \
	"\n==== Auto-config wizard testing commencing ======\n"
#define STOPPING_SEPARATOR \
	"\n==== Auto-config wizard testing stopping ========\n"
#define ERROR_MESSAGE \
	"Stream: %1\nError:\n%2\n=================================================\n"

void Test::NextStage()
{
	if (testThread.joinable())
		testThread.join();
	if (cancel)
		return;

	subProgressLabel->setText(QString());

	if (type == AutoConfig::Type::Recording) {
		stage = Stage::RecordingEncoder;
		StartRecordingEncoderStage();
	} else if (stage == Stage::Starting) {
		if (!started) {
			blog(LOG_INFO, STARTING_SEPARATOR);
			started = true;
		}
		stage = Stage::BandwidthTest;
		StartBandwidthStage();

	} else if (stage == Stage::BandwidthTest) {
		stage = Stage::StreamEncoder;
		StartStreamEncoderStage();
	} else {
		stage = Stage::Finished;
		FinalizeResults();
	}
}

void Test::UpdateMessage(QString message)
{
	subProgressLabel->setText(message);
}

void Test::Failure(QString message)
{
	m.lock();
	subProgressLabel->setText(message);
	subProgressLabel->setStyleSheet("QLabel { color : red; }");
	m.unlock();

	emit Failure(obs_data_get_int(settings, "id"), message);
}

void Test::Progress(int percentage)
{
	progress->setValue(percentage);
}

void Test::StartTest() {
	m.lock();
	stage = Stage::Starting;
	m.unlock();

	NextStage();
}

void Test::CancelTest() {
	CleanUp();
}

AutoConfigTestPage::AutoConfigTestPage(QWidget *parent)
	: QWizardPage(parent), ui(new Ui_AutoConfigTestPage)
{
	ui->setupUi(this);
	setTitle(QTStr("Basic.AutoConfig.TestPage"));
	setSubTitle(QTStr(SUBTITLE_TESTING));
	setCommitPage(true);

	connect(ui->resultsLeft, SIGNAL(clicked()), this, 
		SLOT(on_resultsLeft_Clicked()));
	connect(ui->resultsRight, SIGNAL(clicked()), this, 
		SLOT(on_resultsRight_Clicked()));
}

AutoConfigTestPage::~AutoConfigTestPage()
{
	delete ui;
	emit CancelTests();
	ClearTests();

	if (started)
		blog(LOG_INFO, STOPPING_SEPARATOR);
}

void AutoConfigTestPage::initializePage()
{
	setSubTitle(QTStr(SUBTITLE_TESTING));
	cancel = false;
	DeleteLayout(results);
	results = new QFormLayout();
	results->setContentsMargins(0, 0, 0, 0);
	ui->finishPageLayout->insertLayout(1, results);
	ui->stackedWidget->setCurrentIndex(0);
	
	StartTests();
}

void AutoConfigTestPage::StartTests() {
	std::map<int, OBSData> configs = wiz->streamPage->GetConfigs();
	ClearTests();

	/* make it skip to bandwidth stage if only set to config recording */
	if (wiz->type == AutoConfig::Type::Recording) {
		if (wiz->recordingEncoder != AutoConfig::Encoder::Stream ||
		    wiz->recordingQuality != AutoConfig::Quality::Stream) {
			OBSData data = obs_data_create();
			obs_data_release(data);

			tests[-1] = new Test(data, this,
					      AutoConfig::Type::Recording);
		}

		for (auto &config : configs) {
			if (obs_data_get_int(config.second, "output_id") == -1) {
				const char *name = 
					obs_data_get_string(config.second, "name");
				int newOutputID = wiz->AddDefaultOutput(name);
				obs_data_set_int(config.second, "output_id", 
						 newOutputID);
			}
		}
	}
	else {
		QVBoxLayout *layout = 
			reinterpret_cast<QVBoxLayout *>(QWidget::layout());
		for (auto &config : configs) {
			OBSData setting = config.second;
			if (obs_data_get_int(setting, "output_id") == -1) {
				bool runTest = obs_data_get_bool(setting,
							      "bwTest");
				bool isYoutube = 
					QString(obs_data_get_string(setting, "service"))
							.startsWith("YouTube");
				if (runTest && !isYoutube) {
					tests[config.first] = new Test(setting, this);
					layout->insertWidget(layout->count() - 1,
							     tests[config.first]);
				}
				else {
					const char *name = 
						obs_data_get_string(setting, "name");
					int newOutputID = wiz->AddDefaultOutput(name);
					obs_data_set_int(setting, "output_id", newOutputID);
				}
			}
		}
	}
	emit StartBandwidthTests();
}

void AutoConfigTestPage::ClearTests() {
	emit CancelTests();
	cancel = true;
	for (auto &test : tests) {
		if (test.second)
			delete test.second;
	}
	tests.clear();
}

void AutoConfigTestPage::Failure(int id, const QString &message) {
	failureMessages += ERROR_MESSAGE;
	failureMessages += failureMessages
			.arg(obs_data_get_string(wiz->GetExistingOutput(id), "name"))
			.arg(message);
	failures[id] = message;
}

void Test::CleanUp()
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

void AutoConfigTestPage::cleanupPage()
{
	emit CancelTests();
}

bool AutoConfigTestPage::isComplete() const
{
	return completeTests.size() + failures.size() == tests.size() && !cancel;
}

int AutoConfigTestPage::nextId() const
{
	return -1;
}

void AutoConfigTestPage::TestComplete(const OBSData &config)
{	
	wiz->SetOutputs(obs_data_get_int(config, "id"),config);
	completeTests.push_back(obs_data_get_int(config, "id"));
	if (isComplete())
		FinalizeResults();
}