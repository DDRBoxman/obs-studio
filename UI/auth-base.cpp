#include "auth-base.hpp"
#include "window-basic-main.hpp"

#include <vector>
#include <regex>
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
	const char *type = config_get_string(main->Config(), "Auth", "Type");

	std::string types = "";	
	if (type && std::strlen(type) != 0)
		types = type;

	std::map<int, std::string> names = Auth::ParseAuthTypes(types);
	std::map<int, std::shared_ptr<Auth>> auths;
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
			types.append(",");
		types.append(auth->Name());
		auth->SaveInternal();
	}

	config_set_string(main->Config(), "Auth", "Type", types.c_str());
	config_save_safe(main->Config(), "tmp", nullptr);
}

const char *Auth::Name() const {
	std::string name = def.service;
	name += "_service#";
	name += std::to_string(id);
	return name.c_str();
}

std::map<int, std::string> Auth::ParseAuthTypes(const std::string types) {
	std::map<int, std::string> services;
	if (types.size() == 0)
		return services;

	const std::regex comma(",");
	std::vector<std::string> auths(
	    std::sregex_token_iterator(types.begin(), types.end(), comma, -1),
	    std::sregex_token_iterator()
	);

	const std::regex sep("_service#");
	for (auto &auth : auths) {
		std::vector<std::string> info(
		    std::sregex_token_iterator(auth.begin(), auth.end(), sep, -1),
		    std::sregex_token_iterator()
		);
		if (info.size() > 0) {
			std::string name = info[0];
			int id = 0;
			if (info.size() == 2)
				id = std::stoi(info[1]);
			services.insert(std::pair<int, std::string>(id, name));
		}
	}

	return services;
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
		obs_data_release(settings);
	}
}