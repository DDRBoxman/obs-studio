#include "settings-list-container.hpp"

SettingsListContainer::SettingsListContainer(std::vector<OBSData> settingsList) {
        for (unsigned i = 0; i < settingsList.size(); i++) {
                Add(settingsList[i]);
        }
}
OBSData SettingsListContainer::GetSettings(int id) const {
        return settings.at(id);
}

std::vector<int> SettingsListContainer::GetOrder() const {
        return orderedIDList;
}

int SettingsListContainer::GetCount() const {
        return orderedIDList.size(); 
}

const SettingsListContainer& SettingsListContainer::SetSetting(int settingID, OBSData newSetting) {
        settings[settingID] = newSetting; 
        return *this;
}

const SettingsListContainer& SettingsListContainer::SetOrder(const std::vector<int>& newOrder) {
        orderedIDList = newOrder; 
        return *this;
}

int SettingsListContainer::Add(const OBSData& setting) {
        int id = obs_data_get_int(setting, "id");

        if (settings.find(id) == settings.end()) {
                settings.insert({id, setting});
                orderedIDList.push_back(id);
        }

        return id;
}

void SettingsListContainer::Remove(int settingID) {
        bool elementFound = false;

        for (auto i = orderedIDList.begin(); i != orderedIDList.end(); i++) {
                if (*i == settingID) {
                        orderedIDList.erase(i);
                        elementFound = true;
                        break;
                }
        }

        if (elementFound)
                settings.erase(settingID);
}