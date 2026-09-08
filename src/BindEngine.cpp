#include "BindEngine.hpp"

#include <Windows.h>

#include <algorithm>

namespace km {
namespace {

std::uint8_t modifierForVirtualKey(const std::uint16_t virtualKey) noexcept {
    switch (virtualKey) {
    case VK_CONTROL:
    case VK_LCONTROL:
    case VK_RCONTROL:
        return ModifierCtrl;
    case VK_SHIFT:
    case VK_LSHIFT:
    case VK_RSHIFT:
        return ModifierShift;
    case VK_MENU:
    case VK_LMENU:
    case VK_RMENU:
        return ModifierAlt;
    case VK_LWIN:
    case VK_RWIN:
        return ModifierWin;
    default:
        return ModifierNone;
    }
}

} // namespace

BindEngine::BindEngine(Settings& settings, ActionExecutor& executor)
    : settings_(settings), executor_(executor) {}

void BindEngine::handle(const RawKeyEvent& event) noexcept {
    if (!event.keyDown) {
        const auto device = pressed_.find(event.device);
        if (device != pressed_.end()) {
            device->second.erase(event.key);
            if (device->second.empty()) {
                pressed_.erase(device);
            }
        }
        return;
    }
    auto& deviceKeys = pressed_[event.device];
    if (deviceKeys.contains(event.key)) {
        return;
    }
    deviceKeys.insert_or_assign(event.key, event.virtualKey);
    lastPressedKey_ = event.key;

    const std::uint8_t modifiers = currentModifiers(0, nullptr);
    if (event.virtualKey == 'P' && modifiers == (ModifierCtrl | ModifierAlt) && !rightAltPressed()) {
        paused_ = !paused_;
        return;
    }
    if (paused_) {
        return;
    }
    if (altGrPressed()) {
        return;
    }

    Profile* profile = activeProfile();
    if (profile == nullptr) {
        return;
    }
    const std::uint8_t triggerModifiers = currentModifiers(event.device, &event.key);
    const auto binding = std::find_if(profile->bindings.begin(), profile->bindings.end(),
                                      [&event, triggerModifiers](const Binding& candidate) {
                                          return candidate.key == event.key &&
                                                 candidate.modifiers == triggerModifiers;
                                      });
    if (binding == profile->bindings.end()) {
        return;
    }
    for (const auto& action : binding->actions) {
        executor_.execute(action);
    }
}

void BindEngine::resetDevice(const std::uintptr_t device) noexcept {
    pressed_.erase(device);
}

bool BindEngine::paused() const noexcept {
    return paused_;
}

std::optional<KeyCode> BindEngine::lastPressedKey() const noexcept {
    return lastPressedKey_;
}

void BindEngine::setPaused(const bool paused) noexcept {
    paused_ = paused;
}

std::uint8_t BindEngine::currentModifiers(const std::uintptr_t excludedDevice,
                                          const KeyCode* excludedKey) const noexcept {
    std::uint8_t modifiers = ModifierNone;
    for (const auto& [device, keys] : pressed_) {
        for (const auto& [key, virtualKey] : keys) {
            if (excludedKey != nullptr && device == excludedDevice && key == *excludedKey) {
                continue;
            }
            modifiers = static_cast<std::uint8_t>(modifiers | modifierForVirtualKey(virtualKey));
        }
    }
    return modifiers;
}

bool BindEngine::rightAltPressed() const noexcept {
    for (const auto& [device, keys] : pressed_) {
        static_cast<void>(device);
        if (std::any_of(keys.begin(), keys.end(), [](const auto& item) {
                return item.second == VK_RMENU;
            })) {
            return true;
        }
    }
    return false;
}

bool BindEngine::altGrPressed() const noexcept {
    bool leftCtrl = false;
    bool rightCtrl = false;
    bool leftAlt = false;
    bool rightAlt = false;
    for (const auto& [device, keys] : pressed_) {
        static_cast<void>(device);
        for (const auto& [key, virtualKey] : keys) {
            static_cast<void>(key);
            leftCtrl = leftCtrl || virtualKey == VK_LCONTROL;
            rightCtrl = rightCtrl || virtualKey == VK_RCONTROL;
            leftAlt = leftAlt || virtualKey == VK_LMENU;
            rightAlt = rightAlt || virtualKey == VK_RMENU;
        }
    }
    // Windows exposes AltGr as synthetic Left Ctrl + physical Right Alt.
    return leftCtrl && rightAlt && !rightCtrl && !leftAlt;
}

Profile* BindEngine::activeProfile() noexcept {
    const auto profile = std::find_if(settings_.profiles.begin(), settings_.profiles.end(),
                                      [this](const Profile& candidate) {
                                          return candidate.name == settings_.activeProfile;
                                      });
    return profile == settings_.profiles.end() ? nullptr : &*profile;
}

} // namespace km
