#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace km {

enum class ExtendedFlag : std::uint8_t {
    None = 0,
    E0 = 1,
    E1 = 2,
};

struct KeyCode {
    std::uint16_t scanCode{};
    ExtendedFlag extended{};

    bool operator==(const KeyCode&) const = default;
};

struct KeyCodeHash {
    std::size_t operator()(const KeyCode& key) const noexcept {
        return (static_cast<std::size_t>(key.scanCode) << 2U) |
               static_cast<std::size_t>(key.extended);
    }
};

enum Modifier : std::uint8_t {
    ModifierNone = 0,
    ModifierCtrl = 1 << 0,
    ModifierShift = 1 << 1,
    ModifierAlt = 1 << 2,
    ModifierWin = 1 << 3,
};

enum class ActionType : std::uint8_t {
    PlaySound,
    LaunchFile,
    SendKey,
};

struct Action {
    ActionType type{ActionType::PlaySound};
    std::wstring path;
    std::uint16_t virtualKey{};
};

struct Binding {
    KeyCode key;
    std::uint8_t modifiers{ModifierNone};
    std::vector<Action> actions;
};

struct Profile {
    std::string name;
    std::vector<Binding> bindings;
};

struct Settings {
    int keyboardSize{};
    float masterVolume{0.8F};
    std::string outputDeviceId;
    std::string outputDeviceName;
    std::string activeProfile{"Default"};
    std::vector<Profile> profiles{{"Default", {}}};
};

} // namespace km
