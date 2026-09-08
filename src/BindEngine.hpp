#pragma once

#include "ActionExecutor.hpp"
#include "Types.hpp"

#include <cstdint>
#include <optional>
#include <unordered_map>

namespace km {

struct RawKeyEvent {
    KeyCode key;
    std::uint16_t virtualKey{};
    std::uintptr_t device{};
    bool keyDown{};
};

class BindEngine final {
public:
    BindEngine(Settings& settings, ActionExecutor& executor);

    void handle(const RawKeyEvent& event) noexcept;
    void resetDevice(std::uintptr_t device) noexcept;
    [[nodiscard]] bool paused() const noexcept;
    [[nodiscard]] std::optional<KeyCode> lastPressedKey() const noexcept;
    void setPaused(bool paused) noexcept;

private:
    [[nodiscard]] std::uint8_t currentModifiers(std::uintptr_t excludedDevice,
                                                const KeyCode* excludedKey) const noexcept;
    [[nodiscard]] bool rightAltPressed() const noexcept;
    [[nodiscard]] bool altGrPressed() const noexcept;
    [[nodiscard]] Profile* activeProfile() noexcept;

    Settings& settings_;
    ActionExecutor& executor_;
    using DeviceKeys = std::unordered_map<KeyCode, std::uint16_t, KeyCodeHash>;
    std::unordered_map<std::uintptr_t, DeviceKeys> pressed_;
    std::optional<KeyCode> lastPressedKey_;
    bool paused_{};
};

} // namespace km
