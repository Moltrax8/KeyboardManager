#include "ConfigStore.hpp"

#include <Windows.h>

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace {

class TemporaryDirectory final {
public:
    TemporaryDirectory() {
        const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                (L"KeyboardManagerTests-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                 std::to_wstring(suffix));
        std::filesystem::create_directories(path_);
    }

    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const noexcept {
        return path_;
    }

private:
    std::filesystem::path path_;
};

void require(const bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void writeFile(const std::filesystem::path& path, const std::string& content) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(stream), "could not create test configuration");
    stream << content;
    require(static_cast<bool>(stream), "could not write test configuration");
}

void testRoundTrip() {
    TemporaryDirectory temporary;
    const auto path = temporary.path() / L"config.json";
    km::ConfigStore store(path);

    km::Settings expected;
    expected.keyboardSize = 2;
    expected.masterVolume = 0.42F;
    expected.outputDeviceId = "stable-device-id";
    expected.outputDeviceName = "Speakers";
    expected.activeProfile = "Games";
    expected.profiles = {{"Games",
                          {{{0x1EU, km::ExtendedFlag::E0},
                            static_cast<std::uint8_t>(km::ModifierCtrl | km::ModifierShift),
                            {{km::ActionType::PlaySound, L"alerts\\\u00FC.mp3", 0},
                             {km::ActionType::LaunchFile, L"C:\\Tools\\helper.exe", 0},
                             {km::ActionType::SendKey, {}, VK_MEDIA_PLAY_PAUSE}}}}}};

    require(store.save(expected), "save failed");
    require(std::filesystem::is_regular_file(path), "config.json was not created");
    require(!std::filesystem::exists(path.wstring() + L".tmp"), "temporary file was left behind");

    const km::Settings actual = store.load();
    require(actual.keyboardSize == expected.keyboardSize, "keyboard size did not round-trip");
    require(std::abs(actual.masterVolume - expected.masterVolume) < 0.001F,
            "master volume did not round-trip");
    require(actual.outputDeviceId == expected.outputDeviceId, "output device ID did not round-trip");
    require(actual.outputDeviceName == expected.outputDeviceName, "output device name did not round-trip");
    require(actual.activeProfile == expected.activeProfile, "active profile did not round-trip");
    require(actual.profiles.size() == 1 && actual.profiles.front().bindings.size() == 1,
            "profile bindings did not round-trip");

    const auto& binding = actual.profiles.front().bindings.front();
    require(binding.key == expected.profiles.front().bindings.front().key,
            "physical key identity did not round-trip");
    require(binding.modifiers == expected.profiles.front().bindings.front().modifiers,
            "modifiers did not round-trip");
    require(binding.actions.size() == 3, "action chain did not round-trip");
    require(binding.actions[0].type == km::ActionType::PlaySound &&
                binding.actions[0].path == L"alerts\\\u00FC.mp3",
            "sound action did not round-trip");
    require(binding.actions[1].type == km::ActionType::LaunchFile &&
                binding.actions[1].path == L"C:\\Tools\\helper.exe",
            "launch action did not round-trip");
    require(binding.actions[2].type == km::ActionType::SendKey &&
                binding.actions[2].virtualKey == VK_MEDIA_PLAY_PAUSE,
            "media-key action did not round-trip");
}

void testInvalidValuesAreSanitized() {
    TemporaryDirectory temporary;
    const auto path = temporary.path() / L"config.json";
    writeFile(path, R"json({
        "keyboardSize": 99,
        "masterVolume": -5,
        "activeProfile": "Missing",
        "profiles": [
            {"name": "Valid", "bindings": [
                {"scanCode": 30, "extended": 0, "modifiers": 16,
                 "actions": [{"type": "sound", "path": "ignored.mp3"}]},
                {"scanCode": 31, "extended": 1, "modifiers": 2,
                 "actions": [{"type": "key", "virtualKey": 179}]}
            ]},
            {"name": "Valid", "bindings": []},
            {"name": "", "bindings": []}
        ]
    })json");

    const km::Settings settings = km::ConfigStore(path).load();
    require(settings.keyboardSize == 2, "keyboard size was not clamped");
    require(settings.masterVolume == 0.0F, "master volume was not clamped");
    require(settings.activeProfile == "Valid", "missing active profile did not fall back");
    require(settings.profiles.size() == 1, "invalid or duplicate profiles were retained");
    require(settings.profiles.front().bindings.size() == 1, "invalid binding was retained");
    require(settings.profiles.front().bindings.front().actions.size() == 1,
            "valid media action was removed");
}

void testCorruptConfigurationIsPreserved() {
    TemporaryDirectory temporary;
    const auto path = temporary.path() / L"config.json";
    const auto corruptPath = std::filesystem::path(path.wstring() + L".corrupt");
    writeFile(path, "{not valid json");

    km::ConfigStore store(path);
    const km::Settings settings = store.load();
    require(settings.activeProfile == "Default" && settings.profiles.size() == 1,
            "corrupt configuration did not fall back to defaults");
    require(!std::filesystem::exists(path), "corrupt source file was not moved");
    require(std::filesystem::is_regular_file(corruptPath), "corrupt configuration was not preserved");
    require(store.save(settings), "saving remained disabled after successful preservation");
}

void testUnknownActionsAreNotReinterpreted() {
    TemporaryDirectory temporary;
    const auto path = temporary.path() / L"config.json";
    writeFile(path, R"json({
        "version": 1,
        "activeProfile": "Default",
        "profiles": [{"name": "Default", "bindings": [
            {"scanCode": 30, "extended": 0, "modifiers": 0,
             "actions": [{"type": "future-action", "path": "must-not-become-a-sound"}]}
        ]}]
    })json");

    const km::Settings settings = km::ConfigStore(path).load();
    require(settings.profiles.size() == 1 && settings.profiles.front().bindings.empty(),
            "unknown action was reinterpreted as a supported action");
}

void testNewerConfigurationIsNotOverwritten() {
    TemporaryDirectory temporary;
    const auto path = temporary.path() / L"config.json";
    constexpr char FutureConfig[] = R"json({"version": 2, "future": "preserve-me"})json";
    writeFile(path, FutureConfig);

    km::ConfigStore store(path);
    const km::Settings settings = store.load();
    require(settings.activeProfile == "Default", "newer configuration did not use safe defaults");
    require(!store.save(settings), "saving was not disabled for a newer configuration");

    std::ifstream stream(path, std::ios::binary);
    const std::string preserved((std::istreambuf_iterator<char>(stream)),
                                std::istreambuf_iterator<char>());
    require(preserved == FutureConfig, "newer configuration was modified");
}

} // namespace

void runBindEngineTests();

int main() {
    try {
        runBindEngineTests();
        testRoundTrip();
        testInvalidValuesAreSanitized();
        testCorruptConfigurationIsPreserved();
        testUnknownActionsAreNotReinterpreted();
        testNewerConfigurationIsNotOverwritten();
        std::cout << "All core tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
}
