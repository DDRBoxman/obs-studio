#pragma once

#include <QWizard>
#include <QPointer>
#include <QFormLayout>
#include <QWizardPage>

#include <condition_variable>
#include <utility>
#include <thread>
#include <memory>
#include <vector>
#include <string>
#include <mutex>
#include <map>

class Ui_AutoConfigStartPage;
class Ui_AutoConfigVideoPage;
class Ui_AutoConfigStreamPage;
class Ui_AutoConfigTestPage;

class AutoConfigStreamPage;
class Auth;

enum class Service {
	Twitch,
	Smashcast,
	Other,
};

class AutoConfig : public QWizard {
	Q_OBJECT

	friend class AutoConfigStartPage;
	friend class AutoConfigVideoPage;
	friend class AutoConfigStreamPage;
	friend class AutoConfigTestPage;
	friend class Test;

	enum class Type {
		Invalid,
		Streaming,
		Recording,
	};

	enum class Encoder {
		x264,
		NVENC,
		QSV,
		AMD,
		Stream,
	};

	enum class Quality {
		Stream,
		High,
	};

	enum class FPSType : int {
		PreferHighFPS,
		PreferHighRes,
		UseCurrent,
		fps30,
		fps60,
	};

	static inline const char *GetEncoderId(Encoder enc);

	AutoConfigStreamPage *streamPage = nullptr;

	Service service = Service::Other;
	Quality recordingQuality = Quality::Stream;
	Encoder recordingEncoder = Encoder::Stream;
	Encoder streamingEncoder = Encoder::x264;
	Type type = Type::Streaming;
	FPSType fpsType = FPSType::PreferHighFPS;
	int idealBitrate = 2500;
	int baseResolutionCX = 1920;
	int baseResolutionCY = 1080;
	int idealResolutionCX = 1280;
	int idealResolutionCY = 720;
	int idealFPSNum = 60;
	int idealFPSDen = 1;
	std::string serviceName;
	std::string serverName;
	std::string server;
	std::string key;

	bool hardwareEncodingAvailable = false;
	bool nvencAvailable = false;
	bool qsvAvailable = false;
	bool vceAvailable = false;

	int startingBitrate = 2500;
	bool customServer = false;
	bool bandwidthTest = false;
	bool testRegions = true;
	bool twitchAuto = false;
	bool regionUS = true;
	bool regionEU = true;
	bool regionAsia = true;
	bool regionOther = true;
	bool preferHighFPS = false;
	bool preferHardware = false;
	int specificFPSNum = 0;
	int specificFPSDen = 0;

	std::map<int, OBSData> existingOutputs;
	std::mutex m;

	void TestHardwareEncoding();
	bool CanTestServer(const char *server);

	virtual void done(int result) override;

	void SaveStreamSettings();
	void SaveSettings();

public:
	AutoConfig(QWidget *parent);
	~AutoConfig();
	bool hasTwitchAuto() { return twitchAuto; }
	static int GetNewSettingID(const std::map<int, OBSData> &map);
	static OBSData GetDefaultOutput(const char* outputName, int id);
	std::map<int, OBSData> GetExistingOutputs() { return existingOutputs; }
	OBSData GetExistingOutput(int id) { return existingOutputs[id]; }
	void SetOutputs(int id, const OBSData &configs) {
		existingOutputs[id] = configs;
	}
	void Lock() { m.lock(); };
	void Unlock() { m.unlock(); };
	void AddOutput(int id, OBSData config) { 
		existingOutputs.insert({id, config}); 
	}
	int AddDefaultOutput(const char* name);
	enum Page {
		StartPage,
		VideoPage,
		StreamPage,
		TestPage,
	};
};

class AutoConfigStartPage : public QWizardPage {
	Q_OBJECT

	friend class AutoConfig;

	Ui_AutoConfigStartPage *ui;

public:
	AutoConfigStartPage(QWidget *parent = nullptr);
	~AutoConfigStartPage();

	virtual int nextId() const override;

public slots:
	void on_prioritizeStreaming_clicked();
	void on_prioritizeRecording_clicked();
};

class AutoConfigVideoPage : public QWizardPage {
	Q_OBJECT

	friend class AutoConfig;

	Ui_AutoConfigVideoPage *ui;

public:
	AutoConfigVideoPage(QWidget *parent = nullptr);
	~AutoConfigVideoPage();

	virtual int nextId() const override;
	virtual bool validatePage() override;
};

class AutoConfigStreamPage : public QWizardPage {
	Q_OBJECT

	friend class AutoConfig;

	enum class Section : int {
		Connect,
		StreamKey,
	};

	std::map<int, OBSData> serviceConfigs;
	int selectedServiceID = -1;
	int autoConfigID = -1;
	std::map<int, std::shared_ptr<Auth>> serviceAuths;
	std::shared_ptr<Auth> auth;

	bool firstLoad = false;
	bool loading = false;
	Ui_AutoConfigStreamPage *ui;
	QString lastService;
	bool ready = false;

	void LoadServices(bool showAll);
	inline bool IsCustomService() const;
	void LoadStreamSettings();
	OBSData ServiceToSettingData(const OBSService& service);
	void LoadOutputComboBox(const std::map<int, OBSData>& outputs);
	void PopulateStreamSettings();
	void ClearStreamSettings();
	void GetStreamSettings();
	bool CheckNameAndKey();

	void AddEmptyServiceSetting(int id);
public:
	AutoConfigStreamPage(QWidget *parent = nullptr);
	~AutoConfigStreamPage();

	virtual bool isComplete() const override;
	virtual int nextId() const override;
	virtual bool validatePage() override;
	std::map<int, OBSData> GetConfigs() { return serviceConfigs; }

	void OnAuthConnected();
	void OnOAuthStreamKeyConnected();

public slots:
	void on_show_clicked();
	void on_connectAccount_clicked();
	void on_disconnectAccount_clicked();
	void on_useStreamKey_clicked();
	void ServiceChanged();
	void UpdateKeyLink();
	void UpdateServerList();
	void UpdateCompleted();
	void UpdateOutputConfigForm();

	void on_actionAddService_trigger();
	void on_actionRemoveService_trigger();
	void on_actionScrollUp_trigger();
	void on_actionScrollDown_trigger();

	void on_serviceList_itemClicked(int id);
};

struct ServerInfo {
	std::string name;
	std::string address;
	int bitrate;
	int ms;

	inline ServerInfo() : bitrate(0) , ms(-1) {}

	inline ServerInfo(const char *name_, const char *address_)
		: name(name_), address(address_)
	{
	}
};

class AutoConfigTestPage : public QWizardPage {
	Q_OBJECT

	friend class AutoConfig;

	QPointer<QFormLayout> results;

	Ui_AutoConfigTestPage *ui;

	std::map<int, OBSData> tests;
	std::map<int, OBSData>::iterator currentTest;
	QString failureMessages;
	std::map<int, QString> failures;

	int baseCX;
	int baseCY;
	int specFpsNum;
	int specFpsDen;
	bool preferHighFps;

	std::thread testThread;
	std::condition_variable cv;
	std::mutex m;

	enum class Stage {
		Starting,
		BandwidthTest,
		StreamEncoder,
		RecordingEncoder,
		Finished,
		Failed
	};

	void StartBandwidthStage(const OBSData &settings);
	void StartStreamEncoderStage(const OBSData &settings);
	void StartRecordingEncoderStage(const OBSData &settings);

	void FindIdealHardwareResolution(const OBSData &settings);
	bool TestSoftwareEncoding(const OBSData &settings);

	void TestBandwidthThread(const OBSData &settings);
	void TestStreamEncoderThread(const OBSData &settings);
	void TestRecordingEncoderThread(const OBSData &settings);

	void FinalizeResults();

	std::vector<int> completeTests;
	bool cancel = false;
	bool started = false;
	Stage stage = Stage::Starting;

	std::vector<ServerInfo> GetServers(const std::string &service);
public:
	AutoConfigTestPage(QWidget *parent = nullptr);
	~AutoConfigTestPage();

	virtual void initializePage() override;
	virtual void cleanupPage() override;
	virtual bool isComplete() const override;
	virtual int nextId() const override;

private slots:
	void NextStage();
	void UpdateMessage(QString message);
	void Failure(QString message);
	void Progress(int percentage);
};