/*---------------------------------------------------------------
The container is used to store settings and their order in the 
keyed-settings container. It uses a map to quickly add/remove settings
and a vector to store the order.
-----------------------------------------------------------------*/
#pragma once

#include <map>
#include <vector>
#include <obs.hpp>

class SettingsListContainer {

public:
        SettingsListContainer() {}
        SettingsListContainer(std::vector<OBSData> settings);
        
        std::map<int, OBSData> GetSettings() const { return settings; }
        OBSData GetSettings(int id) const;
        int GetIdAtIndex(int index) const {
               return orderedIDList[index];
        }
        std::vector<int> GetOrder() const;
        int GetCount() const;

        const SettingsListContainer& SetSetting(int settingID, OBSData newSetting);
        const SettingsListContainer& SetOrder(const std::vector<int>& newOrder);

        int Add(const OBSData& setting);
        void Remove(int settingID);

private:
        std::vector<int> orderedIDList;
        std::map<int, OBSData> settings;
};