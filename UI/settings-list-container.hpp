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
    SettingsListContainer() { ; }
    SettingsListContainer(std::vector<OBSData> settings);
    
    OBSData getSettings(int id) const;
    std::vector<int> getOrder() const;
    int getCount() const;

    const SettingsListContainer& setSettings(int settingID, OBSData newSetting);
    const SettingsListContainer& setOrder(std::vector<int> newOrder);

    int add(OBSData setting);
    void remove(int settingID);

private:
    std::vector<int> orderedIDList;
    std::map<int, OBSData> settings;
};