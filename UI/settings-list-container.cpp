#include "settings-list-container.hpp"

SettingsListContainer::SettingsListContainer(std::vector<OBSData> settingsList) {
    
    for (unsigned i = 0; i < settingsList.size(); i++) {
        add(settingsList[i]);
    }
}
OBSData SettingsListContainer::getSettings(int id) const {
    return settings.at(id);
}

std::vector<int> SettingsListContainer::getOrder() const {
    return orderedIDList;
}

int SettingsListContainer::getCount() const {
    return orderedIDList.size(); 
}

const SettingsListContainer& SettingsListContainer::setSettings(int settingID, OBSData newSetting) {
    settings[settingID] = newSetting; 
    return *this;
}

const SettingsListContainer& SettingsListContainer::setOrder(std::vector<int> newOrder) {
    orderedIDList = newOrder; 
    return *this;
}

int SettingsListContainer::add(OBSData setting) {

    int id = obs_data_get_int(setting, "id");

    if (settings.find(id) == settings.end()) {
        settings.insert({id, setting});
        orderedIDList.push_back(id);
    }

    return id;
}

void SettingsListContainer::remove(int settingID) {
    
    auto pos = settings.find(settingID);

    if (pos != settings.end()) {
        settings.erase(pos);

        for (auto i = orderedIDList.begin(); i != orderedIDList.end(); i++) {
            if (*i == settingID) {
                orderedIDList.erase(i);
                break;
            }
        }
    }
}