#include <QMessageBox>
#include <QUrl>

#include <algorithm>

#include "window-basic-settings.hpp"
#include "obs-frontend-api.h"
#include "obs-app.hpp"
#include "window-basic-main.hpp"
#include "qt-wrappers.hpp"
#include "url-push-button.hpp"

#ifdef BROWSER_AVAILABLE
#include <browser-panel.hpp>
#include "auth-oauth.hpp"
#endif

struct QCef;
struct QCefCookieManager;

extern QCef *cef;
extern QCefCookieManager *panel_cookies;

enum class ListOpt : int {
	ShowAll = 1,
	Custom,
};

enum class Section : int {
	Connect,
	StreamKey,
};

inline bool OBSBasicSettings::IsCustomService() const
{
	return ui->service->currentData().toInt() == (int)ListOpt::Custom;
}

void OBSBasicSettings::InitStreamPage()
{
	ui->connectAccount2->setVisible(false);
	ui->disconnectAccount->setVisible(false);
	ui->bandwidthTestEnable->setVisible(false);
	ui->twitchAddonDropdown->setVisible(false);
	ui->twitchAddonLabel->setVisible(false);
	ui->mixerAddonDropdown->setVisible(false);
	ui->mixerAddonLabel->setVisible(false);

	int vertSpacing = ui->topStreamLayout->verticalSpacing();

	QMargins m = ui->topStreamLayout->contentsMargins();
	m.setBottom(vertSpacing / 2);
	ui->topStreamLayout->setContentsMargins(m);

	m = ui->loginPageLayout->contentsMargins();
	m.setTop(vertSpacing / 2);
	ui->loginPageLayout->setContentsMargins(m);

	m = ui->streamkeyPageLayout->contentsMargins();
	m.setTop(vertSpacing / 2);
	ui->streamkeyPageLayout->setContentsMargins(m);

	LoadServices(false);

	ui->twitchAddonDropdown->addItem(
		QTStr("Basic.Settings.Stream.TTVAddon.None"));
	ui->twitchAddonDropdown->addItem(
		QTStr("Basic.Settings.Stream.TTVAddon.BTTV"));
	ui->twitchAddonDropdown->addItem(
		QTStr("Basic.Settings.Stream.TTVAddon.FFZ"));
	ui->twitchAddonDropdown->addItem(
		QTStr("Basic.Settings.Stream.TTVAddon.Both"));

	ui->mixerAddonDropdown->addItem(
		QTStr("Basic.Settings.Stream.MixerAddon.None"));
	ui->mixerAddonDropdown->addItem(
		QTStr("Basic.Settings.Stream.MixerAddon.MEE"));

	connect(ui->service, SIGNAL(currentIndexChanged(int)), this,
		SLOT(UpdateServerList()));
	connect(ui->service, SIGNAL(currentIndexChanged(int)), this,
		SLOT(UpdateKeyLink()));

	connect(ui->actionAddService, SIGNAL(triggered(bool)), this, 
		SLOT(AddService()));
	connect(ui->actionRemoveService, SIGNAL(triggered(bool)), ui->servicesList, 
		SLOT(RemoveItem()));
	connect(ui->actionScrollUp, SIGNAL(triggered(bool)), ui->servicesList, 
		SLOT(ScrollUp()));
	connect(ui->actionScrollDown, SIGNAL(triggered(bool)), ui->servicesList, 
		SLOT(ScrollDown()));

	connect(ui->servicesList, SIGNAL(RemovedKey(int, int)), this,
		SLOT(RemoveService(int)));
	connect(ui->servicesList, SIGNAL(ItemClicked(int)), this,
		SLOT(DisplayServiceSettings(int)));

	connect(ui->serviceNameInput, SIGNAL(textEdited(QString)), ui->servicesList, 
		SLOT(UpdateItemName(QString)));
}

OBSData OBSBasicSettings::ServiceToSettingData(const OBSService& service) {
	OBSData serviceSetting = obs_service_get_settings(service);
	obs_data_set_obj(serviceSetting, "hotkey-data", obs_hotkeys_save_service(service));
	return serviceSetting;
}

void OBSBasicSettings::LoadStream1Settings() {
	std::vector<OBSService> services = main->GetServices();

	int selectedIndex = -1;

	selectedServiceID = main->GetSelectedSettingID();
	serviceAuths = main->GetAuths();

	for(unsigned i = 0; i < services.size(); i++) {
		OBSData data = ServiceToSettingData(services[i]);

		if (obs_data_get_bool(data, "connectedAccount")) {
			int id = obs_data_get_int(data, "id");
			int addon = config_get_int(main->Config(), 
						   serviceAuths.at(id)->Name(),
						   "AddonChoice");
			obs_data_set_int(data, "AddonChoice", addon);
		}

		serviceSettings.Add(data);
		ui->servicesList->AddNewItem(obs_data_get_string(data, "name"), 
						  obs_data_get_int(data, "id"));
		
		if (selectedServiceID == obs_data_get_int(data, "id")) 
			selectedIndex = i;
	}

	if (selectedServiceID == -1) {
		selectedIndex = 0;
		selectedServiceID = serviceSettings.GetIdAtIndex(0);
	}

	ui->servicesList->setCurrentRow(selectedIndex);
	PopulateStreamSettingsForm(selectedServiceID);
}

void OBSBasicSettings::SaveStream1Settings() {
	if (ui->servicesList->count() != 0) {
		int currentID = ui->servicesList->currentItem()->
			data(Qt::UserRole).toInt();
		SaveStreamSettingsChanges(currentID);
	}

	OBSService defaultService;
	std::vector<OBSService> services;

	for (int i = 0; i < serviceSettings.GetCount(); i++) {
		int id = serviceSettings.GetIdAtIndex(i);
		OBSData settings = serviceSettings.GetSettings(id);
		
		if (serviceAuths.find(id) != serviceAuths.end()) {
			const char *name = serviceAuths.at(id)->Name();
			int addOnChoice = 
				obs_data_get_int(settings, "AddonChoice");
			obs_data_erase(settings, "AddonChoice");
			config_set_int(main->Config(), name, "AddonChoice",
				       addOnChoice);
		}

		const char* type = obs_data_get_string(settings, "type");
		const char* name = obs_data_get_string(settings, "name");

		OBSData hotkeyData = obs_data_get_obj(settings, "hotkey-data");

		OBSService tmp = obs_service_create(type, name, settings, hotkeyData);
		
		services.push_back(tmp); 
	}

	main->SetServices(services);
	main->SetSelectedSettingID(selectedServiceID);
	main->SaveService();
	main->SetAuths(serviceAuths);
	main->LoadAuthUIs();
}

void OBSBasicSettings::UpdateKeyLink()
{
	if (IsCustomService()) {
		ui->getStreamKeyButton->hide();
		return;
	}

	QString serviceName = ui->service->currentText();
	QString streamKeyLink;
	if (serviceName == "Twitch") {
		streamKeyLink =
			"https://www.twitch.tv/broadcast/dashboard/streamkey";
	} else if (serviceName == "YouTube / YouTube Gaming") {
		streamKeyLink = "https://www.youtube.com/live_dashboard";
	} else if (serviceName.startsWith("Restream.io")) {
		streamKeyLink =
			"https://restream.io/settings/streaming-setup?from=OBS";
	} else if (serviceName == "Facebook Live") {
		streamKeyLink = "https://www.facebook.com/live/create?ref=OBS";
	} else if (serviceName.startsWith("Twitter")) {
		streamKeyLink = "https://www.pscp.tv/account/producer";
	} else if (serviceName.startsWith("YouStreamer")) {
		streamKeyLink = "https://app.youstreamer.com/stream/";
	}

	if (QString(streamKeyLink).isNull()) {
		ui->getStreamKeyButton->hide();
	} else {
		ui->getStreamKeyButton->setTargetUrl(QUrl(streamKeyLink));
		ui->getStreamKeyButton->show();
	}
}

void OBSBasicSettings::LoadServices(bool showAll)
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

static inline bool is_auth_service(const std::string &service)
{
	return Auth::AuthType(service) != Auth::Type::None;
}

void OBSBasicSettings::on_service_currentIndexChanged(int)
{
	bool showMore = ui->service->currentData().toInt() ==
			(int)ListOpt::ShowAll;
	if (showMore)
		return;

	std::string service = QT_TO_UTF8(ui->service->currentText());
	bool custom = IsCustomService();

	ui->disconnectAccount->setVisible(false);
	ui->bandwidthTestEnable->setVisible(false);
	ui->twitchAddonDropdown->setVisible(false);
	ui->twitchAddonLabel->setVisible(false);
	ui->mixerAddonDropdown->setVisible(false);
	ui->mixerAddonLabel->setVisible(false);

#ifdef BROWSER_AVAILABLE
	if (cef) {
		if (lastService != service.c_str()) {
			QString key = ui->key->text();
			bool can_auth = is_auth_service(service);
			int page = can_auth && (!loading || key.isEmpty())
					   ? (int)Section::Connect
					   : (int)Section::StreamKey;

			ui->streamStackWidget->setCurrentIndex(page);
			ui->streamKeyWidget->setVisible(true);
			ui->streamKeyLabel->setVisible(true);
			ui->connectAccount2->setVisible(can_auth);
			if (serviceAuths.find(selectedServiceID) != 
			    serviceAuths.end()) {
				    serviceAuths[selectedServiceID].reset();
			}
		}
	} else {
		ui->connectAccount2->setVisible(false);
	}
#else
	ui->connectAccount2->setVisible(false);
#endif

	ui->useAuth->setVisible(custom);
	ui->authUsernameLabel->setVisible(custom);
	ui->authUsername->setVisible(custom);
	ui->authPwLabel->setVisible(custom);
	ui->authPwWidget->setVisible(custom);

	if (custom) {
		ui->streamkeyPageLayout->insertRow(1, ui->serverLabel,
						   ui->serverStackedWidget);

		ui->serverStackedWidget->setCurrentIndex(1);
		ui->serverStackedWidget->setVisible(true);
		ui->serverLabel->setVisible(true);
		on_useAuth_toggled();
	} else {
		ui->serverStackedWidget->setCurrentIndex(0);
	}

#ifdef BROWSER_AVAILABLE
	if (serviceAuths.find(selectedServiceID) != serviceAuths.end()) {
		std::map<int, std::shared_ptr<Auth>> auths = main->GetAuths();

		if (auths.find(selectedServiceID) != auths.end() &&
		    service.find(auths[selectedServiceID]->service()) != std::string::npos) {
			serviceAuths[selectedServiceID] = auths[selectedServiceID];
			OnAuthConnected();
		}
	}
#endif
}

void OBSBasicSettings::UpdateServerList()
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

void OBSBasicSettings::on_show_clicked()
{
	if (ui->key->echoMode() == QLineEdit::Password) {
		ui->key->setEchoMode(QLineEdit::Normal);
		ui->show->setText(QTStr("Hide"));
	} else {
		ui->key->setEchoMode(QLineEdit::Password);
		ui->show->setText(QTStr("Show"));
	}
}

void OBSBasicSettings::on_authPwShow_clicked()
{
	if (ui->authPw->echoMode() == QLineEdit::Password) {
		ui->authPw->setEchoMode(QLineEdit::Normal);
		ui->authPwShow->setText(QTStr("Hide"));
	} else {
		ui->authPw->setEchoMode(QLineEdit::Password);
		ui->authPwShow->setText(QTStr("Show"));
	}
}

OBSService OBSBasicSettings::SpawnTempService()
{
	bool custom = IsCustomService();
	const char *service_id = custom ? "rtmp_custom" : "rtmp_common";

	OBSData settings = obs_data_create();
	obs_data_release(settings);

	if (!custom) {
		obs_data_set_string(settings, "service",
				    QT_TO_UTF8(ui->service->currentText()));
		obs_data_set_string(
			settings, "server",
			QT_TO_UTF8(ui->server->currentData().toString()));
	} else {
		obs_data_set_string(settings, "server",
				    QT_TO_UTF8(ui->customServer->text()));
	}
	obs_data_set_string(settings, "key", QT_TO_UTF8(ui->key->text()));

	OBSService newService = obs_service_create(service_id, "temp_service",
						   settings, nullptr);
	obs_service_release(newService);

	return newService;
}

void OBSBasicSettings::OnOAuthStreamKeyConnected()
{
#ifdef BROWSER_AVAILABLE
	OAuthStreamKey *a = 
		reinterpret_cast<OAuthStreamKey *>(
			serviceAuths.at(selectedServiceID).get());

	if (a) {
		bool validKey = !a->key().empty();

		if (validKey)
			ui->key->setText(QT_UTF8(a->key().c_str()));

		ui->streamKeyWidget->setVisible(false);
		ui->streamKeyLabel->setVisible(false);
		ui->connectAccount2->setVisible(false);
		ui->disconnectAccount->setVisible(true);

		if (strcmp(a->service(), "Twitch") == 0) {
			ui->bandwidthTestEnable->setVisible(true);
			ui->twitchAddonLabel->setVisible(true);
			ui->twitchAddonDropdown->setVisible(true);
		}
		if (strcmp(a->service(), "Mixer") == 0) {
			ui->mixerAddonLabel->setVisible(true);
			ui->mixerAddonDropdown->setVisible(true);
		}
	}

	ui->streamStackWidget->setCurrentIndex((int)Section::StreamKey);
#endif
}

void OBSBasicSettings::OnAuthConnected()
{
	std::string service = QT_TO_UTF8(ui->service->currentText());
	Auth::Type type = Auth::AuthType(service);

	if (type == Auth::Type::OAuth_StreamKey) {
		OnOAuthStreamKeyConnected();
	}

	if (!loading) {
		stream1Changed = true;
		EnableApplyButton(true);
	}
}

void OBSBasicSettings::on_connectAccount_clicked()
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

void OBSBasicSettings::on_disconnectAccount_clicked()
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
	ui->bandwidthTestEnable->setVisible(false);
	ui->twitchAddonDropdown->setVisible(false);
	ui->twitchAddonLabel->setVisible(false);
	ui->key->setText("");
}

void OBSBasicSettings::on_useStreamKey_clicked()
{
	ui->streamStackWidget->setCurrentIndex((int)Section::StreamKey);
}

void OBSBasicSettings::on_useAuth_toggled()
{
	if (!IsCustomService())
		return;

	bool use_auth = ui->useAuth->isChecked();

	ui->authUsernameLabel->setVisible(use_auth);
	ui->authUsername->setVisible(use_auth);
	ui->authPwLabel->setVisible(use_auth);
	ui->authPwWidget->setVisible(use_auth);
}

void OBSBasicSettings::AddEmptyServiceSetting(int id, bool isDefault) {
	OBSData data = obs_data_create();

	char serviceName[64];
	if (isDefault)
		sprintf(serviceName, "Default Stream");
	else 
		sprintf(serviceName, "New Stream %d", id);
 
	obs_data_set_int(data, "id", (long long)id);
	obs_data_set_string(data, "name", serviceName);
	obs_data_set_string(data, "type", "rtmp_common");

	serviceSettings.Add(data);

	PopulateStreamSettingsForm(id);
	selectedServiceID = id;

	ui->servicesList->AddNewItem(serviceName, id);
}

void OBSBasicSettings::AddService() {
	std::lock_guard<std::mutex> lock(streamMutex);

	if (ui->servicesList->count() != 0)
		SaveStreamSettingsChanges(selectedServiceID);

	std::map<int, OBSData> settings = serviceSettings.GetSettings();
	int newID = GetNewSettingID(settings);

	AddEmptyServiceSetting(newID, false);
	
	stream1Changed = true;
	EnableApplyButton(true);
}

void OBSBasicSettings::RemoveService(int serviceID) {
	std::lock_guard<std::mutex> lock(streamMutex);

	int newSelectedID = -1;

	if (ui->servicesList->count() != 0)
	        newSelectedID = ui->servicesList->currentItem()->
			 			data(Qt::UserRole).toInt();

	serviceSettings.Remove(serviceID);

	if (newSelectedID != -1) {
		PopulateStreamSettingsForm(newSelectedID);
		selectedServiceID = newSelectedID;
	}
	else {
		QMessageBox emptyNotice(this);
		emptyNotice.setIcon(QMessageBox::Warning);
		emptyNotice.setWindowModality(Qt::WindowModal);
		emptyNotice.setWindowTitle("Notice");
		emptyNotice.setText("You have removed all saved services.");
		emptyNotice.exec();

		std::map<int, OBSData> settings;
		int defaultServiceID = GetNewSettingID(settings);
		AddEmptyServiceSetting(defaultServiceID, true);
	}

	stream1Changed = true;
	EnableApplyButton(true);
}

void OBSBasicSettings::DisplayServiceSettings(int serviceID) {
	std::lock_guard<std::mutex> lock(streamMutex);
	SaveStreamSettingsChanges(selectedServiceID);
	PopulateStreamSettingsForm(serviceID);
	selectedServiceID = serviceID;
}

void OBSBasicSettings::PopulateStreamSettingsForm(int id) {	
	OBSData settings = serviceSettings.GetSettings(id);
	
	loading = true;

	const char *type = obs_data_get_string(settings, "type");
	const char *name = obs_data_get_string(settings, "name");
	const char *service = obs_data_get_string(settings, "service");
	const char *server = obs_data_get_string(settings, "server");
	const char *key = obs_data_get_string(settings, "key");
	int outputID = obs_data_get_int(settings, "output_id");
	lastService = service;
	
	int outputIndex = ui->streamOutputComboBox->findData(outputID);
	ui->streamOutputComboBox->setCurrentIndex(outputIndex);

	QString serviceName = name;
	ui->serviceNameInput->setText(name);

	if (strcmp(type, "rtmp_custom") == 0) {
		ui->service->setCurrentIndex(0);
		ui->customServer->setText(server);

		bool use_auth = obs_data_get_bool(settings, "use_auth");
		const char *username =
			obs_data_get_string(settings, "username");
		const char *password =
			obs_data_get_string(settings, "password");
		ui->authUsername->setText(QT_UTF8(username));
		ui->authPw->setText(QT_UTF8(password));
		ui->useAuth->setChecked(use_auth);
	} else {
		int idx = ui->service->findText(service);
		if (idx == -1) {
			if (service && *service)
				ui->service->insertItem(1, service);
			idx = 1;
		}
		ui->service->setCurrentIndex(idx);

		bool bw_test = obs_data_get_bool(settings, "bwtest");
		ui->bandwidthTestEnable->setChecked(bw_test);

		idx = config_get_int(main->Config(), "Twitch", "AddonChoice");
		ui->twitchAddonDropdown->setCurrentIndex(idx);

		idx = config_get_int(main->Config(), "Mixer", "AddonChoice");
		ui->mixerAddonDropdown->setCurrentIndex(idx);
	}

	UpdateServerList();

	if (strcmp(type, "rtmp_common") == 0) {
		int idx = ui->server->findData(server);
		if (idx == -1) {
			if (server && *server)
				ui->server->insertItem(0, server, server);
			idx = 0;
		}
		ui->server->setCurrentIndex(idx);
	}

	ui->key->setText(key);

	lastService.clear();
	on_service_currentIndexChanged(0);

	obs_data_release(settings);

	UpdateKeyLink();

	bool streamActive = obs_frontend_streaming_active();
	ui->streamPage->setEnabled(!streamActive);

	loading = false;
}

OBSData OBSBasicSettings::GetStreamFormChanges() {
	OBSData settings = obs_data_create();
	
	bool customServer = IsCustomService();
	
	char service_type[32];
	if (customServer)
		sprintf(service_type, "rtmp_custom");
	else 
		sprintf(service_type, "rtmp_common");
	
	obs_data_set_string(settings, "type", service_type);
	obs_data_set_string(settings, "name",
			QT_TO_UTF8(ui->serviceNameInput->text()));

	if (!customServer) {
		obs_data_set_string(settings, "service",
					QT_TO_UTF8(ui->service->currentText()));
		obs_data_set_string(
			settings, "server",
			QT_TO_UTF8(ui->server->currentData().toString()));
	} else {
		obs_data_set_string(settings, "server",
					QT_TO_UTF8(ui->customServer->text()));
		obs_data_set_bool(settings, "use_auth",
				ui->useAuth->isChecked());
		if (ui->useAuth->isChecked()) {
			obs_data_set_string(
				settings, "username",
				QT_TO_UTF8(ui->authUsername->text()));
			obs_data_set_string(settings, "password",
						QT_TO_UTF8(ui->authPw->text()));
		}
	}

	int outputID = ui->streamOutputComboBox->currentData().toInt();

	obs_data_set_int(settings, "output_id", outputID);

	obs_data_set_bool(settings, "bwtest",
			ui->bandwidthTestEnable->isChecked());

	bool accountConnected = 
		serviceAuths.find(selectedServiceID) != serviceAuths.end();

	if (accountConnected) {
		QComboBox *addOns = nullptr;

		if (strcmp(serviceAuths.at(selectedServiceID)->service(),
			   "Twitch") == 0)
			addOns = ui->twitchAddonDropdown;
		else if (strcmp(serviceAuths.at(selectedServiceID)->service(),
			        "Mixer") == 0)
			addOns = ui->mixerAddonDropdown;

		int currentChoice =
			obs_data_get_int(settings, "AddonChoice");
		int newChoice = addOns->currentIndex();
		obs_data_set_int(settings, "AddonChoice", newChoice);

		if (currentChoice != newChoice)
			forceAuthReload = true;
	}

	obs_data_set_string(settings, "key", QT_TO_UTF8(ui->key->text()));
	obs_data_set_bool(settings, "connectedAccount", accountConnected);

	return settings;
}

void OBSBasicSettings::SaveStreamSettingsChanges(int selectedServiceID) {
	OBSData formChanges = GetStreamFormChanges();

	obs_data_set_obj(formChanges, 
		"hotkey-data",
		obs_data_get_obj(
			serviceSettings.GetSettings(selectedServiceID),
			"hotkey-data"));
	obs_data_set_int(formChanges, "id", selectedServiceID);

	serviceSettings.SetSetting(selectedServiceID, formChanges);
}