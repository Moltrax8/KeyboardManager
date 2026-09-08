#include "ConfigStore.hpp"

#include "Paths.hpp"

#include <Windows.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <unordered_set>
#include <utility>

namespace km {
namespace {

using Json = nlohmann::json;

const char* actionTypeName(const ActionType type) {
    switch (type) {
    case ActionType::PlaySound:
        return "sound";
    case ActionType::LaunchFile:
        return "launch";
    case ActionType::SendKey:
        return "key";
    }
    return "sound";
}

ActionType parseActionType(const std::string& value) {
    if (value == "launch") {
        return ActionType::LaunchFile;
    }
    if (value == "key") {
        return ActionType::SendKey;
    }
    return ActionType::PlaySound;
}

Json serialize(const Settings& settings) {
    Json root{
        {"version", 1},
        {"keyboardSize", settings.keyboardSize},
        {"masterVolume", settings.masterVolume},
        {"outputDeviceId", settings.outputDeviceId},
        {"outputDevice", settings.outputDeviceName},
        {"activeProfile", settings.activeProfile},
        {"profiles", Json::array()},
    };

    for (const auto& profile : settings.profiles) {
        Json profileJson{{"name", profile.name}, {"bindings", Json::array()}};
        for (const auto& binding : profile.bindings) {
            Json bindingJson{
                {"scanCode", binding.key.scanCode},
                {"extended", static_cast<std::uint8_t>(binding.key.extended)},
                {"modifiers", binding.modifiers},
                {"actions", Json::array()},
            };
            for (const auto& action : binding.actions) {
                bindingJson["actions"].push_back({
                    {"type", actionTypeName(action.type)},
                    {"path", toUtf8(action.path)},
                    {"virtualKey", action.virtualKey},
                });
            }
            profileJson["bindings"].push_back(std::move(bindingJson));
        }
        root["profiles"].push_back(std::move(profileJson));
    }
    return root;
}

Settings deserialize(const Json& root) {
    Settings settings;
    settings.keyboardSize = std::clamp(root.value("keyboardSize", 0), 0, 2);
    settings.masterVolume = std::clamp(root.value("masterVolume", 0.8F), 0.0F, 1.0F);
    settings.outputDeviceId = root.value("outputDeviceId", std::string{});
    settings.outputDeviceName = root.value("outputDevice", std::string{});
    settings.activeProfile = root.value("activeProfile", std::string{"Default"});
    settings.profiles.clear();

    std::unordered_set<std::string> names;
    const auto profiles = root.find("profiles");
    if (profiles != root.end() && profiles->is_array()) {
        for (const auto& profileJson : *profiles) {
            if (!profileJson.is_object()) {
                continue;
            }
            Profile profile;
            profile.name = profileJson.value("name", std::string{});
            if (profile.name.empty() || profile.name.size() > 64 || !names.insert(profile.name).second) {
                continue;
            }

            const auto bindings = profileJson.find("bindings");
            if (bindings != profileJson.end() && bindings->is_array()) {
                for (const auto& bindingJson : *bindings) {
                    if (!bindingJson.is_object()) {
                        continue;
                    }
                    const auto scanCode = bindingJson.value("scanCode", 0U);
                    const auto extended = bindingJson.value("extended", 0U);
                    const auto modifiers = bindingJson.value("modifiers", 0U);
                    if (scanCode == 0 || scanCode > 0xFFFFU || extended > 2U || modifiers > 0x0FU) {
                        continue;
                    }

                    Binding binding{{static_cast<std::uint16_t>(scanCode),
                                     static_cast<ExtendedFlag>(extended)},
                                    static_cast<std::uint8_t>(modifiers), {}};
                    const auto actions = bindingJson.find("actions");
                    if (actions != bindingJson.end() && actions->is_array()) {
                        for (const auto& actionJson : *actions) {
                            if (!actionJson.is_object()) {
                                continue;
                            }
                            Action action;
                            action.type = parseActionType(actionJson.value("type", std::string{"sound"}));
                            action.path = fromUtf8(actionJson.value("path", std::string{}));
                            action.virtualKey = static_cast<std::uint16_t>(
                                std::min(actionJson.value("virtualKey", 0U), 0xFFFFU));
                            if ((action.type == ActionType::SendKey && action.virtualKey != 0) ||
                                (action.type != ActionType::SendKey && !action.path.empty())) {
                                binding.actions.push_back(std::move(action));
                            }
                        }
                    }
                    if (!binding.actions.empty()) {
                        profile.bindings.push_back(std::move(binding));
                    }
                }
            }
            settings.profiles.push_back(std::move(profile));
        }
    }

    if (settings.profiles.empty()) {
        settings.profiles.push_back({"Default", {}});
    }
    const auto active = std::find_if(settings.profiles.begin(), settings.profiles.end(),
                                     [&settings](const Profile& profile) {
                                         return profile.name == settings.activeProfile;
                                     });
    if (active == settings.profiles.end()) {
        settings.activeProfile = settings.profiles.front().name;
    }
    return settings;
}

} // namespace

ConfigStore::ConfigStore() : path_(dataDirectory() / L"config.json") {}

ConfigStore::ConfigStore(std::filesystem::path path)
    : path_(std::move(path)), diagnosticsEnabled_(false) {}

void ConfigStore::report(const std::wstring_view message) const noexcept {
    if (diagnosticsEnabled_) {
        logMessage(message);
    }
}

Settings ConfigStore::load() const {
    std::ifstream stream(path_, std::ios::binary);
    if (!stream) {
        return {};
    }
    try {
        Json root;
        stream >> root;
        return deserialize(root);
    } catch (const std::exception& error) {
        report(L"Configuration load failed: " + fromUtf8(error.what()));
        stream.close();
        const auto corruptPath = path_.wstring() + L".corrupt";
        if (!MoveFileExW(path_.c_str(), corruptPath.c_str(), MOVEFILE_REPLACE_EXISTING)) {
            report(L"The invalid configuration could not be preserved as config.json.corrupt.");
            saveEnabled_ = false;
        }
        return {};
    }
}

bool ConfigStore::save(const Settings& settings) const noexcept {
    if (!saveEnabled_) {
        return false;
    }
    try {
        const auto temporary = path_.wstring() + L".tmp";
        {
            std::ofstream stream(std::filesystem::path(temporary), std::ios::binary | std::ios::trunc);
            if (!stream) {
                report(L"Could not open the temporary configuration file.");
                return false;
            }
            stream << serialize(settings).dump(2) << '\n';
            stream.flush();
            if (!stream) {
                report(L"Could not write the temporary configuration file.");
                return false;
            }
        }
        if (!MoveFileExW(temporary.c_str(), path_.c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            DeleteFileW(temporary.c_str());
            report(L"Could not atomically replace the configuration file.");
            return false;
        }
        return true;
    } catch (const std::exception& error) {
        report(L"Configuration save failed: " + fromUtf8(error.what()));
        return false;
    } catch (...) {
        report(L"Configuration save failed with an unknown error.");
        return false;
    }
}

} // namespace km
