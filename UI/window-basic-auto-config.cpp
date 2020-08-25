#include <QMessageBox>
#include <QScreen>
#include <QDebug>

#include <algorithm>
#include <obs.hpp>

#include "window-basic-auto-config.hpp"
#include "window-basic-main.hpp"
#include "qt-wrappers.hpp"
#include "obs-app.hpp"
#include "url-push-button.hpp"

#include "ui_AutoConfigStartPage.h"
#include "ui_AutoConfigVideoPage.h"
#include "ui_AutoConfigStreamPage.h"

#ifdef BROWSER_AVAILABLE
#include <browser-panel.hpp>
#include "auth-oauth.hpp"
#endif

struct QCef;
struct QCefCookieManager;

extern QCef *cef;
extern QCefCookieManager *panel_cookies;

#define wiz reinterpret_cast<AutoConfig *>(wizard())

/* ------------------------------------------------------------------------- */

#define SERVICE_PATH "service.json"

static OBSData OpenServiceSettings(std::string &type)
{
	char serviceJsonPath[512];
	int ret = GetProfilePath(serviceJsonPath, sizeof(serviceJsonPath),
				 SERVICE_PATH);
	if (ret <= 0)
		return OBSData();

	OBSData data =
		obs_data_create_from_json_file_safe(serviceJsonPath, "bak");
	obs_data_release(data);

	obs_data_set_default_string(data, "type", "rtmp_common");
	type = obs_data_get_string(data, "type");

	OBSData settings = obs_data_get_obj(data, "settings");
	obs_data_release(settings);

	return settings;
}

static void GetServiceInfo(std::string &type, std::string &service,
			   std::string &server, std::string &key)
{
	OBSData settings = OpenServiceSettings(type);

	service = obs_data_get_string(settings, "service");
	server = obs_data_get_string(settings, "server");
	key = obs_data_get_string(settings, "key");
}

/* ------------------------------------------------------------------------- */

AutoConfigStartPage::AutoConfigStartPage(QWidget *parent)
	: QWizardPage(parent), ui(new Ui_AutoConfigStartPage)
{
	ui->setupUi(this);
	setTitle(QTStr("Basic.AutoConfig.StartPage"));
	setSubTitle(QTStr("Basic.AutoConfig.StartPage.SubTitle"));
}

AutoConfigStartPage::~AutoConfigStartPage()
{
	delete ui;
}

int AutoConfigStartPage::nextId() const
{
	return AutoConfig::VideoPage;
}

void AutoConfigStartPage::on_prioritizeStreaming_clicked()
{
	wiz->type = AutoConfig::Type::Streaming;
}

void AutoConfigStartPage::on_prioritizeRecording_clicked()
{
	wiz->type = AutoConfig::Type::Recording;
}

/* ------------------------------------------------------------------------- */

#define RES_TEXT(x) "Basic.AutoConfig.VideoPage." x
#define RES_USE_CURRENT RES_TEXT("BaseResolution.UseCurrent")
#define RES_USE_DISPLAY RES_TEXT("BaseResolution.Display")
#define FPS_USE_CURRENT RES_TEXT("FPS.UseCurrent")
#define FPS_PREFER_HIGH_FPS RES_TEXT("FPS.PreferHighFPS")
#define FPS_PREFER_HIGH_RES RES_TEXT("FPS.PreferHighRes")

AutoConfigVideoPage::AutoConfigVideoPage(QWidget *parent)
	: QWizardPage(parent), ui(new Ui_AutoConfigVideoPage)
{
	ui->setupUi(this);

	setTitle(QTStr("Basic.AutoConfig.VideoPage"));
	setSubTitle(QTStr("Basic.AutoConfig.VideoPage.SubTitle"));

	obs_video_info ovi;
	obs_get_video_info(&ovi);

	long double fpsVal =
		(long double)ovi.fps_num / (long double)ovi.fps_den;

	QString fpsStr = (ovi.fps_den > 1) ? QString::number(fpsVal, 'f', 2)
					   : QString::number(fpsVal, 'g', 2);

	ui->fps->addItem(QTStr(FPS_PREFER_HIGH_FPS),
			 (int)AutoConfig::FPSType::PreferHighFPS);
	ui->fps->addItem(QTStr(FPS_PREFER_HIGH_RES),
			 (int)AutoConfig::FPSType::PreferHighRes);
	ui->fps->addItem(QTStr(FPS_USE_CURRENT).arg(fpsStr),
			 (int)AutoConfig::FPSType::UseCurrent);
	ui->fps->addItem(QStringLiteral("30"), (int)AutoConfig::FPSType::fps30);
	ui->fps->addItem(QStringLiteral("60"), (int)AutoConfig::FPSType::fps60);
	ui->fps->setCurrentIndex(0);

	QString cxStr = QString::number(ovi.base_width);
	QString cyStr = QString::number(ovi.base_height);

	int encRes = int(ovi.base_width << 16) | int(ovi.base_height);
	ui->canvasRes->addItem(QTStr(RES_USE_CURRENT).arg(cxStr, cyStr),
			       (int)encRes);

	QList<QScreen *> screens = QGuiApplication::screens();
	for (int i = 0; i < screens.size(); i++) {
		QScreen *screen = screens[i];
		QSize as = screen->size();

		encRes = int(as.width() << 16) | int(as.height());

		QString str = QTStr(RES_USE_DISPLAY)
				      .arg(QString::number(i + 1),
					   QString::number(as.width()),
					   QString::number(as.height()));

		ui->canvasRes->addItem(str, encRes);
	}

	auto addRes = [&](int cx, int cy) {
		encRes = (cx << 16) | cy;
		QString str = QString("%1x%2").arg(QString::number(cx),
						   QString::number(cy));
		ui->canvasRes->addItem(str, encRes);
	};

	addRes(1920, 1080);
	addRes(1280, 720);

	ui->canvasRes->setCurrentIndex(0);
}

AutoConfigVideoPage::~AutoConfigVideoPage()
{
	delete ui;
}

int AutoConfigVideoPage::nextId() const
{
	return wiz->type == AutoConfig::Type::Recording
		       ? AutoConfig::TestPage
		       : AutoConfig::StreamPage;
}

bool AutoConfigVideoPage::validatePage()
{
	int encRes = ui->canvasRes->currentData().toInt();
	wiz->baseResolutionCX = encRes >> 16;
	wiz->baseResolutionCY = encRes & 0xFFFF;
	wiz->fpsType = (AutoConfig::FPSType)ui->fps->currentData().toInt();

	obs_video_info ovi;
	obs_get_video_info(&ovi);

	switch (wiz->fpsType) {
	case AutoConfig::FPSType::PreferHighFPS:
		wiz->specificFPSNum = 0;
		wiz->specificFPSDen = 0;
		wiz->preferHighFPS = true;
		break;
	case AutoConfig::FPSType::PreferHighRes:
		wiz->specificFPSNum = 0;
		wiz->specificFPSDen = 0;
		wiz->preferHighFPS = false;
		break;
	case AutoConfig::FPSType::UseCurrent:
		wiz->specificFPSNum = ovi.fps_num;
		wiz->specificFPSDen = ovi.fps_den;
		wiz->preferHighFPS = false;
		break;
	case AutoConfig::FPSType::fps30:
		wiz->specificFPSNum = 30;
		wiz->specificFPSDen = 1;
		wiz->preferHighFPS = false;
		break;
	case AutoConfig::FPSType::fps60:
		wiz->specificFPSNum = 60;
		wiz->specificFPSDen = 1;
		wiz->preferHighFPS = false;
		break;
	}

	return true;
}

/* ------------------------------------------------------------------------- */

enum class ListOpt : int {
	ShowAll = 1,
	Custom,
};

AutoConfigStreamPage::AutoConfigStreamPage(QWidget *parent)
	: QWizardPage(parent), ui(new Ui_AutoConfigStreamPage)
{
	ui->setupUi(this);
	ui->bitrateLabel->setVisible(false);
	ui->bitrate->setVisible(false);
	ui->connectAccount2->setVisible(false);
	ui->disconnectAccount->setVisible(false);

	int vertSpacing = ui->topLayout->verticalSpacing();

	QMargins m = ui->topLayout->contentsMargins();
	m.setBottom(vertSpacing / 2);
	ui->topLayout->setContentsMargins(m);

	m = ui->loginPageLayout->contentsMargins();
	m.setTop(vertSpacing / 2);
	ui->loginPageLayout->setContentsMargins(m);

	m = ui->streamkeyPageLayout->contentsMargins();
	m.setTop(vertSpacing / 2);
	ui->streamkeyPageLayout->setContentsMargins(m);

	setTitle(QTStr("Basic.AutoConfig.StreamPage"));
	setSubTitle(QTStr("Basic.AutoConfig.StreamPage.SubTitle"));

	LoadServices(false);
	LoadStreamSettings();

	connect(ui->service, SIGNAL(currentIndexChanged(int)), this,
		SLOT(ServiceChanged()));
	connect(ui->customServer, SIGNAL(textChanged(const QString &)), this,
		SLOT(ServiceChanged()));
	connect(ui->doBandwidthTest, SIGNAL(toggled(bool)), this,
		SLOT(ServiceChanged()));

	connect(ui->service, SIGNAL(currentIndexChanged(int)), this,
		SLOT(UpdateServerList()));
	connect(ui->service, SIGNAL(currentIndexChanged(int)), this,
		SLOT(UpdateKeyLink()));
	
	connect(ui->service, SIGNAL(currentIndexChanged(int)), this,
		SLOT(UpdateOutputConfigForm()));
	connect(ui->output, SIGNAL(currentIndexChanged(int)), this,
		SLOT(UpdateOutputConfigForm()));
	
	connect(ui->actionAddService, SIGNAL(triggered()), this,
		SLOT(on_actionAddService_trigger()));
	connect(ui->actionRemoveService, SIGNAL(triggered()), this,
		SLOT(on_actionRemoveService_trigger()));
	connect(ui->actionScrollUp, SIGNAL(triggered()), this,
		SLOT(on_actionScrollUp_trigger()));
	connect(ui->actionScrollDown, SIGNAL(triggered()), this,
		SLOT(on_actionScrollDown_trigger()));
	connect(ui->serviceList, SIGNAL(ItemClicked(int)), this,
		SLOT(on_serviceList_itemClicked(int)));
	
	connect(ui->streamName, SIGNAL(textEdited(QString)), ui->serviceList, 
		SLOT(UpdateItemName(QString)));

	connect(ui->streamName, SIGNAL(textChanged(const QString &)), this,
		SLOT(UpdateCompleted()));
	connect(ui->key, SIGNAL(textChanged(const QString &)), this,
		SLOT(UpdateCompleted()));
	connect(ui->regionUS, SIGNAL(toggled(bool)), this,
		SLOT(UpdateCompleted()));
	connect(ui->regionEU, SIGNAL(toggled(bool)), this,
		SLOT(UpdateCompleted()));
	connect(ui->regionAsia, SIGNAL(toggled(bool)), this,
		SLOT(UpdateCompleted()));
	connect(ui->regionOther, SIGNAL(toggled(bool)), this,
		SLOT(UpdateCompleted()));
}

AutoConfigStreamPage::~AutoConfigStreamPage()
{
	delete ui;
}

bool AutoConfigStreamPage::isComplete() const
{
	return ready;
}

int AutoConfigStreamPage::nextId() const
{
	bool test;

	for (auto &service : serviceConfigs) {
		if (obs_data_get_int(service.second, "output_id") == -1 &&
		    obs_data_get_bool(service.second, "bwtest")) {
			test = true;
			break;
		}
	}
	return test ? AutoConfig::TestPage : -1;
}

inline bool AutoConfigStreamPage::IsCustomService() const
{
	return ui->service->currentData().toInt() == (int)ListOpt::Custom;
}

bool AutoConfigStreamPage::validatePage()
{
	if (serviceConfigs.size() == 0) {
		QMessageBox emptyNotice(this);
		emptyNotice.setIcon(QMessageBox::Critical);
		emptyNotice.setText("Please create at least one stream.");
		emptyNotice.exec();
		on_actionAddService_trigger();
		return false;	
	}
	if (!CheckNameAndKey())
		return false;

	GetStreamSettings();
	
	QString defaultSets;
	QString testSets;

	for (auto &config : serviceConfigs) {
		if (obs_data_get_int(config.second, "output_id") == -1) {
			const char *name = 
				obs_data_get_string(config.second, "name");
			if (obs_data_get_bool(config.second, "bwtest")){
				testSets += "\n";
				testSets += name;
			}
			else {
				defaultSets += name;
				defaultSets += "\n";
			}
		}
	}

	if (defaultSets.size() != 0 || testSets.size() != 0) {
		/* The message will be moved to the locale files in a latter commit */
		QString message = "One or more services have been set to ";
		message += "\"Auto-Config Output\". The services for which you"; 
		message += " also chose to run bandwidth tests will have ";
		message += " their output configurations assigned based on ";
		message += "the results of a bandwidth test. The remaining ";
		message += "streams would be assigned a default output ";
		message += "configuration. Continue?";

		QString detailedMessage;

		if (defaultSets.size() != 0) {
			detailedMessage += "The following services will get default output configuartions:\n\n";
			detailedMessage += defaultSets;
			detailedMessage += "\n-------------------------------------";
			detailedMessage += "\n-------------------------------------\n\n";
		}

		if (testSets.size() != 0) {
			detailedMessage += "The following services will get default output configuartions from bandwidth tests:\n";
			detailedMessage += testSets;
		}

		QMessageBox testNotice(this);
		QPushButton *cancelButton = testNotice.addButton(QMessageBox::Cancel);
		testNotice.addButton(QMessageBox::Ok);
		testNotice.setWindowTitle("Warning");
		testNotice.setIcon(QMessageBox::Information);
		testNotice.setText(message);
		testNotice.setDetailedText(detailedMessage);
		testNotice.exec();
		
		if (testNotice.clickedButton() == cancelButton)
			return false;
	}

	wiz->serviceConfigs = serviceConfigs;
	return true;
}

void AutoConfigStreamPage::on_show_clicked()
{
	if (ui->key->echoMode() == QLineEdit::Password) {
		ui->key->setEchoMode(QLineEdit::Normal);
		ui->show->setText(QTStr("Hide"));
	} else {
		ui->key->setEchoMode(QLineEdit::Password);
		ui->show->setText(QTStr("Show"));
	}
}

void AutoConfigStreamPage::OnOAuthStreamKeyConnected()
{
#ifdef BROWSER_AVAILABLE
	OAuthStreamKey *a = 
		reinterpret_cast<OAuthStreamKey *>(
			serviceAuths.at(selectedServiceID).get());

	if (a) {
		bool validKey = !a->key().empty();

		if (validKey)
			ui->key->setText(QT_UTF8(a->key().c_str()));

		ui->streamKeyWidget->setVisible(!validKey);
		ui->streamKeyLabel->setVisible(!validKey);
		ui->connectAccount2->setVisible(!validKey);
		ui->disconnectAccount->setVisible(validKey);
	}

	ui->stackedWidget->setCurrentIndex((int)Section::StreamKey);
	UpdateCompleted();
#endif
}

void AutoConfigStreamPage::OnAuthConnected()
{
	std::string service = QT_TO_UTF8(ui->service->currentText());
	Auth::Type type = Auth::AuthType(service);

	if (type == Auth::Type::OAuth_StreamKey) {
		OnOAuthStreamKeyConnected();
	}
}

void AutoConfigStreamPage::on_connectAccount_clicked()
{
#ifdef BROWSER_AVAILABLE
	std::string service = QT_TO_UTF8(ui->service->currentText());

	OAuth::DeleteCookies(service);

	std::shared_ptr<Auth> auth = OAuthStreamKey::Login(this, service);
	if (!!auth) {
		serviceAuths.insert({selectedServiceID, auth});
		OnAuthConnected();
	}
#endif
}

#define DISCONNECT_COMFIRM_TITLE \
	"Basic.AutoConfig.StreamPage.DisconnectAccount.Confirm.Title"
#define DISCONNECT_COMFIRM_TEXT \
	"Basic.AutoConfig.StreamPage.DisconnectAccount.Confirm.Text"

void AutoConfigStreamPage::on_disconnectAccount_clicked()
{
	QMessageBox::StandardButton button;

	button = OBSMessageBox::question(this, QTStr(DISCONNECT_COMFIRM_TITLE),
					 QTStr(DISCONNECT_COMFIRM_TEXT));

	if (button == QMessageBox::No) {
		return;
	}

	serviceAuths.erase(selectedServiceID);

	std::string service = QT_TO_UTF8(ui->service->currentText());

#ifdef BROWSER_AVAILABLE
	OAuth::DeleteCookies(service);
#endif

	ui->streamKeyWidget->setVisible(true);
	ui->streamKeyLabel->setVisible(true);
	ui->connectAccount2->setVisible(true);
	ui->disconnectAccount->setVisible(false);
	ui->key->setText("");
}

void AutoConfigStreamPage::on_useStreamKey_clicked()
{
	ui->stackedWidget->setCurrentIndex((int)Section::StreamKey);
	UpdateCompleted();
}

static inline bool is_auth_service(const std::string &service)
{
	return Auth::AuthType(service) != Auth::Type::None;
}

void AutoConfigStreamPage::ServiceChanged()
{
	if (!loading && QObject::sender() && 
	    QObject::sender() == ui->doBandwidthTest &&
	    ui->doBandwidthTest->isChecked()) {
		if (ui->service->currentText().toStdString() != "Twitch") {
			QMessageBox::StandardButton button;
#define WARNING_TEXT(x) QTStr("Basic.AutoConfig.StreamPage.StreamWarning." x)
			button = OBSMessageBox::question(this, 
							 WARNING_TEXT("Title"),
							 WARNING_TEXT("Text"));
#undef WARNING_TEXT
			if (button == QMessageBox::No) {
				ui->doBandwidthTest->setChecked(false);
				return;
			}
		}
	}

	bool showMore = ui->service->currentData().toInt() ==
			(int)ListOpt::ShowAll;
	if (showMore)
		return;

	std::string service = QT_TO_UTF8(ui->service->currentText());
	bool regionBased = service == "Twitch" || service == "Smashcast";
	bool testBandwidth = ui->doBandwidthTest->isChecked();
	bool custom = IsCustomService();

	ui->disconnectAccount->setVisible(false);

#ifdef BROWSER_AVAILABLE
	if (cef) {
		if (lastService != service.c_str()) {
			bool can_auth = is_auth_service(service);
			int page = can_auth ? (int)Section::Connect
					    : (int)Section::StreamKey;

			ui->stackedWidget->setCurrentIndex(page);
			ui->streamKeyWidget->setVisible(true);
			ui->streamKeyLabel->setVisible(true);
			ui->connectAccount2->setVisible(can_auth);
			if (serviceAuths.find(selectedServiceID) != 
			    serviceAuths.end())
				serviceAuths[selectedServiceID].reset();

			if (lastService.isEmpty())
				lastService = service.c_str();
		}
	} else {
		ui->connectAccount2->setVisible(false);
	}
#else
	ui->connectAccount2->setVisible(false);
#endif

	/* Test three closest servers if "Auto" is available for Twitch */
	if (service == "Twitch" && wiz->twitchAuto)
		regionBased = false;

	ui->streamkeyPageLayout->removeWidget(ui->serverLabel);
	ui->streamkeyPageLayout->removeWidget(ui->serverStackedWidget);

	if (custom) {
		ui->streamkeyPageLayout->insertRow(1, ui->serverLabel,
						   ui->serverStackedWidget);

		ui->region->setVisible(false);
		ui->serverStackedWidget->setCurrentIndex(1);
		ui->serverStackedWidget->setVisible(true);
		ui->serverLabel->setVisible(true);
	} else {
		if (!testBandwidth)
			ui->streamkeyPageLayout->insertRow(
				1, ui->serverLabel, ui->serverStackedWidget);

		ui->region->setVisible(regionBased && testBandwidth);
		ui->serverStackedWidget->setCurrentIndex(0);
		ui->serverStackedWidget->setHidden(testBandwidth);
		ui->serverLabel->setHidden(testBandwidth);
	}

	ui->bitrateLabel->setHidden(testBandwidth);
	ui->bitrate->setHidden(testBandwidth);

#ifdef BROWSER_AVAILABLE
	OBSBasic *main = OBSBasic::Get();

	if (serviceAuths.find(selectedServiceID) != serviceAuths.end()) {
		std::map<int, std::shared_ptr<Auth>> auths = main->GetAuths();

		if (auths.find(selectedServiceID) != auths.end() &&
		    service.find(auths[selectedServiceID]->service()) != std::string::npos) {
			serviceAuths[selectedServiceID] = auths[selectedServiceID];
			OnAuthConnected();
		}
	}
#endif

	UpdateCompleted();
}

void AutoConfigStreamPage::UpdateKeyLink()
{
	if (IsCustomService()) {
		ui->doBandwidthTest->setEnabled(true);
		return;
	}

	QString serviceName = ui->service->currentText();
	bool isYoutube = false;
	QString streamKeyLink;

	if (serviceName == "Twitch") {
		streamKeyLink =
			"https://www.twitch.tv/broadcast/dashboard/streamkey";
	} else if (serviceName == "YouTube / YouTube Gaming") {
		streamKeyLink = "https://www.youtube.com/live_dashboard";
		isYoutube = true;
	} else if (serviceName.startsWith("Restream.io")) {
		streamKeyLink =
			"https://restream.io/settings/streaming-setup?from=OBS";
	} else if (serviceName == "Facebook Live") {
		streamKeyLink = "https://www.facebook.com/live/create?ref=OBS";
	} else if (serviceName.startsWith("Twitter")) {
		streamKeyLink = "https://www.pscp.tv/account/producer";
	} else if (serviceName.startsWith("YouStreamer")) {
		streamKeyLink = "https://www.app.youstreamer.com/stream/";
	}

	if (QString(streamKeyLink).isNull()) {
		ui->streamKeyButton->hide();
	} else {
		ui->streamKeyButton->setTargetUrl(QUrl(streamKeyLink));
		ui->streamKeyButton->show();
	}

	if (isYoutube) {
		ui->doBandwidthTest->setChecked(false);
		ui->doBandwidthTest->setEnabled(false);
	} else {
		ui->doBandwidthTest->setEnabled(true);
	}
}

void AutoConfigStreamPage::LoadServices(bool showAll)
{
	obs_properties_t *props = obs_get_service_properties("rtmp_common");

	OBSData settings = obs_data_create();
	obs_data_release(settings);

	obs_data_set_bool(settings, "show_all", showAll);

	obs_property_t *prop = obs_properties_get(props, "show_all");
	obs_property_modified(prop, settings);

	ui->service->blockSignals(true);
	ui->service->clear();

	QStringList names;

	obs_property_t *services = obs_properties_get(props, "service");
	size_t services_count = obs_property_list_item_count(services);
	for (size_t i = 0; i < services_count; i++) {
		const char *name = obs_property_list_item_string(services, i);
		names.push_back(name);
	}

	if (showAll)
		names.sort();

	for (QString &name : names)
		ui->service->addItem(name);

	if (!showAll) {
		ui->service->addItem(
			QTStr("Basic.AutoConfig.StreamPage.Service.ShowAll"),
			QVariant((int)ListOpt::ShowAll));
	}

	ui->service->insertItem(
		0, QTStr("Basic.AutoConfig.StreamPage.Service.Custom"),
		QVariant((int)ListOpt::Custom));

	if (!lastService.isEmpty()) {
		int idx = ui->service->findText(lastService);
		if (idx != -1)
			ui->service->setCurrentIndex(idx);
	}

	obs_properties_destroy(props);

	ui->service->blockSignals(false);
}

bool AutoConfigStreamPage::CheckNameAndKey() {
	bool emptyKey = ui->key->text().size() == 0;
	bool emptyName = ui->streamName->text().size() == 0;

	if (!emptyKey && !emptyName)
		return true;

	QMessageBox emptyNotice(this);
	emptyNotice.setIcon(QMessageBox::Warning);
	emptyNotice.setWindowModality(Qt::WindowModal);
	emptyNotice.setWindowTitle("Notice");

	if (emptyKey && emptyName)
		emptyNotice.setText("Please provide a stream name and key.");
	else if (emptyKey)
		emptyNotice.setText("Please provide a stream key.");
	else
		emptyNotice.setText("Please provide a stream name.");

	emptyNotice.exec();
	return false;
}

OBSData AutoConfigStreamPage::ServiceToSettingData(const OBSService& service) {
	OBSData serviceSetting = obs_service_get_settings(service);
	obs_data_set_obj(serviceSetting, "hotkey-data",
			 obs_hotkeys_save_service(service));
	return serviceSetting;
}

void AutoConfigStreamPage::LoadOutputComboBox(
				const std::map<int, OBSData>& outputs) {
	bool autoConfigSet = false;
	std::vector<int> outputIDs;

	for (auto &item : outputs) {
		ui->output->addItem(obs_data_get_string(item.second, "name"),
				    QVariant(item.first));
		if (strcmp("Auto-Config Output",
			   obs_data_get_string(item.second, "name")) == 0 && 
		    !autoConfigSet) {
			autoConfigSet = true;
			autoConfigID = item.first;
		}
		outputIDs.push_back(item.first);
	}
	ui->output->model()->sort(0);

	if (!autoConfigSet)
		ui->output->insertItem(0, "Auto-Config Output", -1);
}

void AutoConfigStreamPage::LoadStreamSettings() {
	std::vector<OBSService> services = OBSBasic::Get()->GetServices();
	std::map<int, OBSData> outputs = OBSBasic::Get()->GetStreamOutputSettings();

	ready = true;
	for (unsigned int i = 0; i < services.size(); i++) {
		OBSData data = ServiceToSettingData(services[i]);
		int id = obs_data_get_int(data, "id");
		ui->serviceList->AddNewItem(obs_data_get_string(data, "name"),
					     id);
		serviceConfigs.insert({id, data});
		if (i == 0)
			selectedServiceID = id;

		if (strlen(obs_data_get_string(data, "key")) == 0 ||
		    strlen(obs_data_get_string(data, "name")) == 0)
			ready = false;
	}

	LoadOutputComboBox(outputs);

	ui->serviceList->setCurrentRow(0);
	PopulateStreamSettings();
}

void AutoConfigStreamPage::PopulateStreamSettings() {
	loading = true;
	OBSData settings = serviceConfigs.at(selectedServiceID);

	const char *type = obs_data_get_string(settings, "type");
	QString name = obs_data_get_string(settings, "name");
	const char *service = obs_data_get_string(settings, "service");
	const char *server = obs_data_get_string(settings, "server");
	const char *key = obs_data_get_string(settings, "key");
	int outputID = obs_data_get_int(settings, "output_id");
	lastService = service;

	int outputIndex = ui->output->findData(outputID);
	ui->output->setCurrentIndex(outputIndex);

	ui->streamName->setText(name);
	ui->key->setText(key);

	if (strcmp(type, "rtmp_custom") == 0) {
		int idx = ui->service->findData(QVariant((int)ListOpt::Custom));
		ui->service->setCurrentIndex(idx);
		ui->customServer->setText(server);
	} else {
		int idx = ui->service->findText(service);
		if (idx == -1) {
			if (service && *service)
				ui->service->insertItem(1, service);
			idx = 1;
		}
		ui->service->setCurrentIndex(idx);

		UpdateServerList();
		
		idx = ui->server->findData(server);
		if (idx == -1) {
			if (server && *server)
				ui->server->insertItem(0, server, server);
			idx = 0;
		}
		ui->server->setCurrentIndex(idx);
	}

	ui->bitrate->setValue(obs_data_get_int(settings, "bitrate"));
	if (ui->preferHardware) {
		ui->preferHardware->setChecked(
			obs_data_get_bool(settings, "preferHardware"));
	}
	ui->doBandwidthTest->setChecked(
			obs_data_get_bool(settings, "bwtest"));
	ui->regionUS->setChecked(
			obs_data_get_bool(settings, "regionUS"));
	ui->regionEU->setChecked(
			obs_data_get_bool(settings, "regionEU"));
	ui->regionAsia->setChecked(
			obs_data_get_bool(settings, "regionAsia"));
	ui->regionOther->setChecked(
			obs_data_get_bool(settings, "regionOther"));
	UpdateOutputConfigForm();
	loading = false;
}

void AutoConfigStreamPage::ClearStreamSettings() {
	ui->streamName->setText("");
	ui->key->setText("");
	ui->output->setCurrentIndex(ui->output->findData(-1));
	ui->bitrate->setValue(2500);

	ui->preferHardware->setChecked(false);
	ui->doBandwidthTest->setChecked(false);
	
	ui->regionUS->setChecked(false);
	ui->regionEU->setChecked(false);
	ui->regionAsia->setChecked(false);
	ui->regionOther->setChecked(false);

	UpdateOutputConfigForm();
}

void AutoConfigStreamPage::UpdateOutputConfigForm() {
	bool enableAutoOutput = 
			strcmp("Auto-Config Output", 
			       QT_TO_UTF8(ui->output->currentText())) == 0;
	
	ui->bitrate-> setVisible(false);
	ui->bitrateLabel->setVisible(false);
	ui->doBandwidthTest->setVisible(false);
	ui->region->setVisible(false);
	ui->preferHardware->setVisible(false);

	if (!enableAutoOutput)
		return;
	
	bool isYouTube = ui->service->currentText().startsWith("YouTube");

	ui->preferHardware->setVisible(true);

	if (!isYouTube) {
		ui->doBandwidthTest->setVisible(true);
		bool isTwitch = ui->service->currentText().startsWith("Twitch");

		bool regionBased = isTwitch ||
			ui->service->currentText().startsWith("Smashcast");
		
		if (isTwitch && 
		    strcmp(QT_TO_UTF8(ui->server->currentText()), "Auto (Recommended)") == 0)
		    regionBased = false;
		
		if (ui->doBandwidthTest->isChecked() && regionBased)
			ui->region->setVisible(true);
	}
	else {
		ui->doBandwidthTest->setChecked(false);
	}
	
	if (!ui->doBandwidthTest->isChecked()) {
		ui->bitrate->setVisible(true);
		ui->bitrateLabel->setVisible(true);
	}
}

void AutoConfigStreamPage::GetStreamSettings()
{	
	OBSData service_settings = serviceConfigs.at(selectedServiceID);
	bool custom = IsCustomService();
	const char *serverType = custom ? "rtmp_custom" : "rtmp_common";

	std::string service = QT_TO_UTF8(ui->service->currentText());
	obs_data_set_string(service_settings, "service", service.c_str());
	obs_data_set_string(service_settings, "name",
				    QT_TO_UTF8(ui->streamName->text()));
	obs_data_set_string(service_settings, "key",
			    QT_TO_UTF8(ui->key->text()));
	obs_data_set_string(service_settings, "type", serverType);

	int bitrate = 10000;
	if (!ui->doBandwidthTest->isChecked()) {
		bitrate = ui->bitrate->value();
		wiz->idealBitrate = bitrate;
	}
	obs_data_set_int(service_settings, "bitrate", bitrate);

	if (custom) {
		QString server = ui->customServer->text();
		obs_data_set_string(service_settings, "server", QT_TO_UTF8(server));
	} else {
		QString server = ui->server->currentData().toString();
		obs_data_set_string(service_settings, "server", QT_TO_UTF8(server));
	}

	obs_data_set_int(service_settings, "output_id", 
					 ui->output->currentData().toInt());
	obs_data_set_bool(service_settings, "bwtest",
			  ui->doBandwidthTest->isChecked());
	obs_data_set_int(service_settings, "startingBitrate",
			  (int)obs_data_get_int(service_settings, "bitrate"));
	obs_data_set_int(service_settings, "idealBitrate",
			  (int)obs_data_get_int(service_settings, "bitrate"));
	
	obs_data_set_bool(service_settings, "regionUS",
			  ui->regionUS->isChecked());
	obs_data_set_bool(service_settings, "regionEU",
			  ui->regionEU->isChecked());
	obs_data_set_bool(service_settings, "regionAsia",
			  ui->regionAsia->isChecked());
	obs_data_set_bool(service_settings, "regionOther",
			  ui->regionOther->isChecked());

	if (ui->preferHardware) {
		obs_data_set_bool(service_settings, "preferHardware",
			  	  ui->preferHardware->isChecked());
	}

	int serviceType = -1;
	if (service == "Twitch")
		serviceType = (int)Service::Twitch;
	else if (service == "Smashcast")
		serviceType = (int)Service::Smashcast;
	else
		serviceType = (int)Service::Other;

	obs_data_set_int(service_settings, "serviceType", serviceType);

	ready = true;
}

int AutoConfig::GetNewSettingID(const std::map<int, OBSData> &map) {
	std::vector<int> usedIDs;
	for (auto &i : map)
		usedIDs.push_back(i.first);

	std::sort(usedIDs.begin(), usedIDs.end());
	for (int i = 0; i < (int)usedIDs.size(); i++) {
		if (usedIDs[i] != i)
			return i;
	}

	return usedIDs.size();
}

void AutoConfigStreamPage::AddEmptyServiceSetting(int id) {
	OBSData data = obs_data_create();
	obs_data_release(data);

	char serviceName[64];
	sprintf(serviceName, "New Stream %d", id);
 
	obs_data_set_int(data, "id", (long long)id);
	obs_data_set_string(data, "name", serviceName);
	obs_data_set_string(data, "type", "rtmp_common");
	obs_data_set_int(data, "output_id", autoConfigID);

	serviceConfigs.insert({id, data});
	selectedServiceID = id;
	ui->serviceList->AddNewItem(serviceName, id);
}

void AutoConfigStreamPage::on_actionAddService_trigger() {
	if (!CheckNameAndKey())
		return;
	int newID = AutoConfig::GetNewSettingID(serviceConfigs);

	if (serviceConfigs.size() != 0)
		GetStreamSettings();
	
	AddEmptyServiceSetting(newID);
	PopulateStreamSettings();
}

void AutoConfigStreamPage::on_actionRemoveService_trigger() {
	int newSelectedID = -1;
	if (serviceConfigs.size() == 0)
		return;
	
	serviceConfigs.erase(selectedServiceID);
	int row = ui->serviceList->currentRow();
	ui->serviceList->takeItem(row);

	if (serviceConfigs.size() != 0) {
		newSelectedID = ui->serviceList->currentItem()->
						    data(Qt::UserRole).toInt();
	}
	selectedServiceID = newSelectedID;
	
	if (selectedServiceID != -1)
		PopulateStreamSettings();
	else
		ClearStreamSettings();
}

void AutoConfigStreamPage::on_actionScrollUp_trigger() {
	int currentRow = ui->serviceList->currentRow();
	if (currentRow <= 0 || ui->serviceList->count() == 0)
		return;
	
	if (!CheckNameAndKey())
		return;
	
	GetStreamSettings();

	ui->serviceList->setCurrentRow(--currentRow);
	selectedServiceID = ui->serviceList->currentItem()->
						    data(Qt::UserRole).toInt();
	
	PopulateStreamSettings();
}

void AutoConfigStreamPage::on_actionScrollDown_trigger() {
	int currentRow = ui->serviceList->currentRow();
	if (ui->serviceList->count() == 0 || 
	    ui->serviceList->count() == currentRow + 1)
		return;

	if (!CheckNameAndKey())
		return;

	GetStreamSettings();

	ui->serviceList->setCurrentRow(++currentRow);
	selectedServiceID = ui->serviceList->currentItem()->
						    data(Qt::UserRole).toInt();
	
	PopulateStreamSettings();
}

void AutoConfigStreamPage::on_serviceList_itemClicked(int id) {
	if (!CheckNameAndKey())
		return;

	GetStreamSettings();
	selectedServiceID = id;
	PopulateStreamSettings();
}

void AutoConfigStreamPage::UpdateServerList()
{
	QString serviceName = ui->service->currentText();
	bool showMore = ui->service->currentData().toInt() ==
			(int)ListOpt::ShowAll;

	if (showMore) {
		LoadServices(true);
		ui->service->showPopup();
		return;
	} else {
		lastService = serviceName;
	}

	obs_properties_t *props = obs_get_service_properties("rtmp_common");
	obs_property_t *services = obs_properties_get(props, "service");

	OBSData settings = obs_data_create();
	obs_data_release(settings);

	obs_data_set_string(settings, "service", QT_TO_UTF8(serviceName));
	obs_property_modified(services, settings);

	obs_property_t *servers = obs_properties_get(props, "server");

	ui->server->clear();

	size_t servers_count = obs_property_list_item_count(servers);
	for (size_t i = 0; i < servers_count; i++) {
		const char *name = obs_property_list_item_name(servers, i);
		const char *server = obs_property_list_item_string(servers, i);
		ui->server->addItem(name, server);
	}

	obs_properties_destroy(props);
}

void AutoConfigStreamPage::UpdateCompleted()
{	
	bool hasAuth = serviceAuths.find(selectedServiceID) != 
		       serviceAuths.end();
	bool validAuth = hasAuth && !!serviceAuths.at(selectedServiceID);
	if (ui->stackedWidget->currentIndex() == (int)Section::Connect ||
	    (ui->key->text().isEmpty() && !validAuth)) {
		ready = false;
	} else {
		bool custom = IsCustomService();
		if (custom) {
			ready = !ui->customServer->text().isEmpty() && 
				!ui->streamName->text().isEmpty() &&
				!ui->key->text().isEmpty();
		} else {
			std::string service = 
					QT_TO_UTF8(ui->service->currentText());
			bool regionBased = service == "Twitch" ||
					   service == "Smashcast";

			if (service == "Twitch" && wiz->twitchAuto)
				regionBased = false;
			
			bool regionsReady = !regionBased || 
				!ui->doBandwidthTest->isChecked() ||
				ui->regionUS->isChecked() ||
				ui->regionEU->isChecked() ||
				ui->regionAsia->isChecked() ||
				ui->regionOther->isChecked();
			
			ready = regionsReady && 
				!ui->streamName->text().isEmpty() &&
				!ui->key->text().isEmpty();
		}
	}

	if (ui->streamName->text().isEmpty())
		ui->streamName->setStyleSheet("border: 2px solid red");
	else
		ui->streamName->setStyleSheet("");
	
	if (ui->key->text().isEmpty())
		ui->key->setStyleSheet("border: 2px solid red");
	else
		ui->key->setStyleSheet("");
	emit completeChanged();
}

/* ------------------------------------------------------------------------- */

AutoConfig::AutoConfig(QWidget *parent) : QWizard(parent)
{
	EnableThreadedMessageBoxes(true);

	calldata_t cd = {0};
	calldata_set_int(&cd, "seconds", 5);

	proc_handler_t *ph = obs_get_proc_handler();
	proc_handler_call(ph, "twitch_ingests_refresh", &cd);
	calldata_free(&cd);

	OBSBasic *main = reinterpret_cast<OBSBasic *>(parent);
	main->EnableOutputs(false);

	installEventFilter(CreateShortcutFilter());

	std::string serviceType;
	GetServiceInfo(serviceType, serviceName, server, key);
#ifdef _WIN32
	setWizardStyle(QWizard::ModernStyle);
#endif
	streamPage = new AutoConfigStreamPage();

	setPage(StartPage, new AutoConfigStartPage());
	setPage(VideoPage, new AutoConfigVideoPage());
	setPage(StreamPage, streamPage);
	setPage(TestPage, new AutoConfigTestPage());
	setWindowTitle(QTStr("Basic.AutoConfig"));
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	obs_video_info ovi;
	obs_get_video_info(&ovi);

	baseResolutionCX = ovi.base_width;
	baseResolutionCY = ovi.base_height;

	/* ----------------------------------------- */
	/* check to see if Twitch's "auto" available */

	OBSData twitchSettings = obs_data_create();
	obs_data_release(twitchSettings);

	obs_data_set_string(twitchSettings, "service", "Twitch");

	obs_properties_t *props = obs_get_service_properties("rtmp_common");
	obs_properties_apply_settings(props, twitchSettings);

	obs_property_t *p = obs_properties_get(props, "server");
	const char *first = obs_property_list_item_string(p, 0);
	twitchAuto = strcmp(first, "auto") == 0;

	obs_properties_destroy(props);
	existingOutputs = OBSBasic::Get()->GetStreamOutputSettings();

	TestHardwareEncoding();

	setOptions(0);
	setButtonText(QWizard::FinishButton,
		      QTStr("Basic.AutoConfig.ApplySettings"));
	setButtonText(QWizard::BackButton, QTStr("Back"));
	setButtonText(QWizard::NextButton, QTStr("Next"));
	setButtonText(QWizard::CancelButton, QTStr("Cancel"));
}

AutoConfig::~AutoConfig()
{
	OBSBasic *main = reinterpret_cast<OBSBasic *>(App()->GetMainWindow());
	main->EnableOutputs(true);
	EnableThreadedMessageBoxes(false);
}

void AutoConfig::TestHardwareEncoding()
{
	size_t idx = 0;
	const char *id;
	while (obs_enum_encoder_types(idx++, &id)) {
		if (strcmp(id, "ffmpeg_nvenc") == 0)
			hardwareEncodingAvailable = nvencAvailable = true;
		else if (strcmp(id, "obs_qsv11") == 0)
			hardwareEncodingAvailable = qsvAvailable = true;
		else if (strcmp(id, "amd_amf_h264") == 0)
			hardwareEncodingAvailable = vceAvailable = true;
	}
}

bool AutoConfig::CanTestServer(const char *server)
{
	if (!testRegions || (regionUS && regionEU && regionAsia && regionOther))
		return true;

	if (service == Service::Twitch) {
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
	} else if (service == Service::Smashcast) {
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

OBSData AutoConfig::GetDefaultOutput(const char* outputName, int id) {
	OBSBasic *main = OBSBasic::Get();
	int videoBitrate =
		config_get_default_uint(main->Config(), "SimpleOutput", "VBitrate");
	const char *streamEnc = config_get_default_string(
		main->Config(), "SimpleOutput", "StreamEncoder");
	int audioBitrate =
		config_get_default_uint(main->Config(), "SimpleOutput", "ABitrate");
	bool advanced =
		config_get_default_bool(main->Config(), "SimpleOutput", "UseAdvanced");
	bool enforceBitrate = 
		config_get_default_bool(main->Config(), "SimpleOutput",
					"EnforceBitrate");
	
	const char *custom =
		config_get_default_string(main->Config(), "SimpleOutput",
				  "x264Settings");

	bool rescale =
		config_get_default_bool(main->Config(), "AdvOut", "Rescale");
	const char *rescaleRes =
		config_get_default_string(main->Config(), "AdvOut", "RescaleRes");
	int trackIndex =
		config_get_default_int(main->Config(), "AdvOut", "TrackIndex");
	bool applyServiceSettings =
		config_get_default_bool(main->Config(), "AdvOut",
					"ApplyServiceSettings");
	
	const char *curPreset = 
		config_get_default_string(main->Config(), "SimpleOutput", "Preset");
	const char *curQSVPreset =
		config_get_default_string(main->Config(), "SimpleOutput",
					 "QSVPreset");
	const char *curNVENCPreset =
		config_get_default_string(main->Config(), "SimpleOutput",
					   "NVENCPreset");
	const char *curAMDPreset = 
		config_get_default_string(main->Config(), "SimpleOutput", "AMDPreset");
	
	OBSData newOutput = obs_data_create();
	obs_data_release(newOutput);

	obs_data_set_int(newOutput, "id_num", id);
	obs_data_set_string(newOutput, "name", outputName);

	/* simple settings */
	obs_data_set_int(newOutput, "video_bitrate", videoBitrate);
	obs_data_set_int(newOutput, "audio_bitrate", audioBitrate);
	obs_data_set_string(newOutput, "stream_encoder", streamEnc);
	obs_data_set_bool(newOutput, "use_advanced", advanced);
	obs_data_set_bool(newOutput, "enforce_bitrate", enforceBitrate);
	obs_data_set_string(newOutput, "x264Settings", custom);

	/* default presets */
	obs_data_set_string(newOutput, "preset", curPreset);
	obs_data_set_string(newOutput, "QSVPreset", curQSVPreset);
	obs_data_set_string(newOutput, "NVENCPreset", curNVENCPreset);
	obs_data_set_string(newOutput, "AMDPreset", curAMDPreset);

	/* advanced settings */
	obs_data_set_bool(newOutput, "rescale", rescale);
	obs_data_set_string(newOutput, "rescale_resolution", rescaleRes);
	obs_data_set_int(newOutput, "track_index", trackIndex);
	obs_data_set_bool(newOutput, "apply_service_settings", applyServiceSettings);

	return newOutput;
}

void AutoConfig::done(int result)
{
	QWizard::done(result);

	if (result == QDialog::Accepted) {
		if (type == Type::Streaming)
			SaveStreamSettings();
		SaveSettings();
	}
}

inline const char *AutoConfig::GetEncoderId(Encoder enc)
{
	switch (enc) {
	case Encoder::NVENC:
		return SIMPLE_ENCODER_NVENC;
	case Encoder::QSV:
		return SIMPLE_ENCODER_QSV;
	case Encoder::AMD:
		return SIMPLE_ENCODER_AMD;
	default:
		return SIMPLE_ENCODER_X264;
	}
};

void AutoConfig::SaveStreamSettings()
{
	OBSBasic *main = reinterpret_cast<OBSBasic *>(App()->GetMainWindow());
	
	std::map<int, OBSData> configs = streamPage->GetConfigs();
	std::vector<OBSService> services;

	int defaultOutputID = -1;

	for (auto &config : configs) {
		OBSService service = ExtractServiceData(config.second);
		obs_service_release(service);

		if (obs_data_get_int(config.second, "output_id") == -1) {
			if (obs_data_get_bool(config.second, "tested") == true) {
				int outputID = AddTestedOutputData(successfulTests[config.first]);
				obs_service_set_output_id(service, outputID);
			}
			else {
				if (defaultOutputID == -1) {
					defaultOutputID =
						AddDefaultOutput("Auto-config default");
				}
				obs_service_set_output_id(service, defaultOutputID);
			}
		}

		services.push_back(service);
	}

	main->SetStreamOutputSettings(existingOutputs);
	main->SetServices(services);

	main->SaveStreamOutputs();
	main->SaveService();

	main->SetAuths(streamPage->serviceAuths);
	main->LoadAuthUIs();
}

int AutoConfig::AddDefaultOutput(const char* name) {
	int newID = AutoConfig::GetNewSettingID(existingOutputs);
	OBSData newOutput = AutoConfig::GetDefaultOutput(name, newID);
	existingOutputs[newID] = newOutput;
	return newID;
}

int AutoConfig::AddTestedOutputData(const OBSData &config) {
	OBSData output = obs_data_create();
	obs_data_release(output);

	auto encName = [](int enc) -> const char *{
		QString name;
		switch (enc) {
		case (int)AutoConfig::Encoder::x264:
			name = "x264";
			break;
		case (int)AutoConfig::Encoder::NVENC:
			name = "ffmpeg_nvenc";
			break;
		case (int)AutoConfig::Encoder::QSV:
			name = "obs_qsv11";
			break;
		case (int)AutoConfig::Encoder::AMD:
			name = "amd_amf_h264";
			break;
		default:
			name = "x264";
			break;
		}
		return name.toStdString().c_str();
	};

	obs_data_set_string(output, "name", 
			    obs_data_get_string(config, "name"));
	obs_data_set_string(output, "stream_encoder",
			    encName(obs_data_get_int(config, "videoEncoderType")));
	obs_data_set_int(output, "track_index", 1);
	obs_data_set_bool(output, "apply_service_settings", false);
	obs_data_set_bool(output, "use_advanced", false);
	obs_data_set_bool(output, "enforce_bitrate", false);

	int resX = obs_data_get_int(config, "resX");
	int resY = obs_data_get_int(config, "resY");
	const char* resString =
		QString("%1x%2").arg(resX, resY).toStdString().c_str();
	obs_data_set_string(output, "rescale_resolution", resString);
	obs_data_set_bool(output, "rescale", true);


	obs_data_set_string(output, "stream_encoder", 
			encName(obs_data_get_int(config, "videoEncoderType")));
	obs_data_set_int(output, "video_bitrate",
			 obs_data_get_int(config, "idealBitrate"));
	obs_data_set_int(output, "audio_bitrate",
			 obs_data_get_int(config, "audio_bitrate"));
	obs_data_set_string(output, "x264Settings", "");
	obs_data_set_string(output, "preset",
			obs_data_get_string(config, "preset"));

	int newID = GetNewSettingID(existingOutputs);
	obs_data_set_int(output, "id_num", newID);
	existingOutputs[newID] = output;
	
	return newID;
}

OBSService AutoConfig::ExtractServiceData(const OBSData &config) {
	const char *type = obs_data_get_string(config, "type");
	const char *name = obs_data_get_string(config, "name");
	int id = obs_data_get_int(config, "id");

	OBSData settings = obs_data_create();
	obs_data_release(settings);

	obs_data_set_string(settings, "name", name);
	obs_data_set_string(settings, "type", type);
	obs_data_set_string(settings, "key", 
			    obs_data_get_string(config, "key"));
	obs_data_set_string(settings, "server",
			    obs_data_get_string(config, "server"));
	obs_data_set_string(settings, "service",
			    obs_data_get_string(config, "service"));

	obs_data_set_bool(settings, "bwtest", false);
	obs_data_set_bool(settings, "connectedAccount",
			  streamPage->serviceAuths.find(id) != 
			  	streamPage->serviceAuths.end());

	obs_data_set_int(settings, "id", obs_data_get_int(config, "id"));
	obs_data_set_int(settings, "output_id",
			 obs_data_get_int(config, "output_id"));
	
	OBSService tmp = obs_service_create(type, name, settings, nullptr);
	
	return tmp;
}

void AutoConfig::SaveSettings()
{
	OBSBasic *main = reinterpret_cast<OBSBasic *>(App()->GetMainWindow());

	if (recordingEncoder != Encoder::Stream)
		config_set_string(main->Config(), "SimpleOutput", "RecEncoder",
				  GetEncoderId(recordingEncoder));

	const char *quality = recordingQuality == Quality::High ? "Small"
								: "Stream";

	config_set_string(main->Config(), "Output", "Mode", "Simple");
	config_set_string(main->Config(), "SimpleOutput", "RecQuality",
			  quality);
	config_set_int(main->Config(), "Video", "BaseCX", baseResolutionCX);
	config_set_int(main->Config(), "Video", "BaseCY", baseResolutionCY);
	config_set_int(main->Config(), "Video", "OutputCX", idealResolutionCX);
	config_set_int(main->Config(), "Video", "OutputCY", idealResolutionCY);

	if (fpsType != FPSType::UseCurrent) {
		config_set_uint(main->Config(), "Video", "FPSType", 0);
		config_set_string(main->Config(), "Video", "FPSCommon",
				  std::to_string(idealFPSNum).c_str());
	}

	main->ResetVideo();
	main->ResetOutputs();
	config_save_safe(main->Config(), "tmp", nullptr);
}
