#include "auth-base.hpp"
#include "window-basic-main.hpp"

#include <vector>
#include <map>

struct AuthInfo {
	Auth::Def def;
	Auth::create_cb create;
};

static std::vector<AuthInfo> authDefs;

void Auth::RegisterAuth(const Def &d, create_cb create)
{
	AuthInfo info = {d, create};
	authDefs.push_back(info);
}

std::shared_ptr<Auth> Auth::Create(const std::string &service, int id)
{
	for (auto &a : authDefs) {
		if (service.find(a.def.service) != std::string::npos) {
			return a.create(id);
		}
	}

	return nullptr;
}

Auth::Type Auth::AuthType(const std::string &service)
{
	for (auto &a : authDefs) {
		if (service.find(a.def.service) != std::string::npos) {
			return a.def.type;
		}
	}

	return Type::None;
}

void Auth::Load()
{
	OBSBasic *main = OBSBasic::Get();
	std::map<int, std::string> names;
	std::map<int, std::shared_ptr<Auth>> auths;
	const char *type = config_get_string(main->Config(), "Auth", "Type");
	
	std::string types = "";
	
	if (type && std::strlen(type) != 0)
		types = type;

	Auth::ParseAuthTypes(types, names);

	for (auto &item : names) {
		std::shared_ptr<Auth> auth = Create(item.second, item.first);
		if (auth) {
			if (auth->LoadInternal())
				auth->LoadUI();
			auths.insert({item.first, auth});
		}
	}
	main->SetAuths(auths);
}

void Auth::Save()
{
	OBSBasic *main = OBSBasic::Get();
	std::map<int, std::shared_ptr<Auth>> auths = main->GetAuths();

	if (auths.size() == 0) {
		if (config_has_user_value(main->Config(), "Auth", "Type")) {
			config_remove_value(main->Config(), "Auth", "Type");
			config_save_safe(main->Config(), "tmp", nullptr);
		}
		return;
	}

	std::string types = "";
	for (auto it = auths.begin(); it != auths.end(); it++) {
		Auth *auth = it->second.get();
		if (it != auths.begin())
			types += ",";
		types += auth->authName();
		auth->SaveInternal();
	}

	config_set_string(main->Config(), "Auth", "Type", types.c_str());
	config_save_safe(main->Config(), "tmp", nullptr);
}

const char *Auth::authName() const {
	std::string name = def.service;
	name += "_service#";
	name += std::to_string(id);
	return name.c_str();
}

void Auth::ParseAuthTypes(const std::string types, 
			  std::map<int, std::string>& services) {
	if (types.size() == 0)
		return;
	
	int index = 0;
	while (index < (int)types.size()) {
		int commaPos = types.find(',', index);
		std::string service;
		if (commaPos == std::string::npos) {
			service = types.substr(index, commaPos);
			index = types.size();
		}
		else {
			service = types.substr(index, commaPos - index);
			index = commaPos + 1;
		}

		std::string name = service.substr(0, service.find("_service#"));
		int id = 0;
		if (service.size() > name.size() + std::strlen("_service#"))
			id = std::stoi(service.substr(service.find("_service#") +
				       std::strlen("_service#")));

		services.insert({id, name});
	}
}

void Auth::ConfigStreamAuths() {
	OBSBasic *main = OBSBasic::Get();
	std::vector<OBSService> services = main->GetServices();
	std::map<int, std::shared_ptr<Auth>> auths = main->GetAuths();

	for (auto &service : services) {
		OBSData settings = obs_service_get_settings(service);
		int id = obs_data_get_int(settings, "id");
		bool bwtest = obs_data_get_bool(settings, "bwtest");

		if (obs_data_get_bool(settings, "connectedAccount")) {
			Auth *auth = auths.at(id).get();
			if(auth->key().empty())
				continue;
			if (bwtest && strcmp(auth->service(), "Twitch") == 0)
				obs_data_set_string(settings, "key",
						(auth->key() + "?bandwidthtest=true").c_str());
			else
				obs_data_set_string(settings, "key", auth->key().c_str());

			obs_service_update(service, settings);
		}
	}
}