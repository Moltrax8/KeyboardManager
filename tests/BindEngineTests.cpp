#include "BindEngine.hpp"

#include <Windows.h>

#include <array>
#include <cstddef>
#include <stdexcept>
#include <string>

namespace {

class RecordingExecutor final : public km::ActionExecutor {
public:
    void execute(const km::Action& action) noexcept override {
        if (count < actions.size()) {
            actions[count++] = &action;
        }
    }

    std::array<const km::Action*, 16> actions{};
    std::size_t count{};
};

void require(const bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

constexpr km::KeyCode CtrlKey{0x1D, km::ExtendedFlag::None};
constexpr km::KeyCode ShiftKey{0x2A, km::ExtendedFlag::None};
constexpr km::KeyCode RightAltKey{0x38, km::ExtendedFlag::E0};
constexpr km::KeyCode TriggerKey{0x1E, km::ExtendedFlag::None};

km::Settings testSettings() {
    km::Settings settings;
    settings.profiles.front().bindings = {
        {TriggerKey, km::ModifierNone, {{km::ActionType::PlaySound, L"plain.mp3", 0}}},
        {TriggerKey, km::ModifierCtrl, {{km::ActionType::PlaySound, L"ctrl.mp3", 0}}},
    };
    return settings;
}

void keyDown(km::BindEngine& engine, const km::KeyCode key, const std::uint16_t virtualKey,
             const std::uintptr_t device = 1) {
    engine.handle({key, virtualKey, device, true});
}

void keyUp(km::BindEngine& engine, const km::KeyCode key, const std::uint16_t virtualKey,
           const std::uintptr_t device = 1) {
    engine.handle({key, virtualKey, device, false});
}

void testInitialKeyDownAndRepeat() {
    auto settings = testSettings();
    RecordingExecutor executor;
    km::BindEngine engine(settings, executor);

    keyDown(engine, TriggerKey, 'A');
    keyDown(engine, TriggerKey, 'A');
    require(executor.count == 1 && executor.actions[0]->path == L"plain.mp3",
            "initial KeyDown debounce failed");

    keyUp(engine, TriggerKey, 'A');
    keyDown(engine, TriggerKey, 'A');
    require(executor.count == 2, "KeyUp did not permit a later trigger");
}

void testExactModifiers() {
    auto settings = testSettings();
    RecordingExecutor executor;
    km::BindEngine engine(settings, executor);

    keyDown(engine, CtrlKey, VK_LCONTROL);
    keyDown(engine, TriggerKey, 'A');
    require(executor.count == 1 && executor.actions[0]->path == L"ctrl.mp3",
            "exact Ctrl binding did not execute");

    keyUp(engine, TriggerKey, 'A');
    keyDown(engine, ShiftKey, VK_LSHIFT);
    keyDown(engine, TriggerKey, 'A');
    require(executor.count == 1, "binding executed with an extra modifier");
}

void testAltGrDoesNotTriggerCtrlAltBindings() {
    auto settings = testSettings();
    settings.profiles.front().bindings.push_back(
        {TriggerKey, static_cast<std::uint8_t>(km::ModifierCtrl | km::ModifierAlt),
         {{km::ActionType::PlaySound, L"ctrl-alt.mp3", 0}}});
    RecordingExecutor executor;
    km::BindEngine engine(settings, executor);

    keyDown(engine, CtrlKey, VK_LCONTROL, 1);
    keyDown(engine, RightAltKey, VK_RMENU, 2);
    keyDown(engine, TriggerKey, 'A', 1);
    require(executor.count == 0, "AltGr triggered a Ctrl+Alt binding");
}

void testDeviceResetClearsDebounceState() {
    auto settings = testSettings();
    RecordingExecutor executor;
    km::BindEngine engine(settings, executor);

    keyDown(engine, TriggerKey, 'A', 7);
    engine.resetDevice(7);
    keyDown(engine, TriggerKey, 'A', 7);
    require(executor.count == 2, "device removal did not clear pressed keys");
}

void testPauseChord() {
    auto settings = testSettings();
    RecordingExecutor executor;
    km::BindEngine engine(settings, executor);
    constexpr km::KeyCode AltKey{0x38, km::ExtendedFlag::None};
    constexpr km::KeyCode PKey{0x19, km::ExtendedFlag::None};

    keyDown(engine, CtrlKey, VK_LCONTROL);
    keyDown(engine, AltKey, VK_LMENU);
    keyDown(engine, PKey, 'P');
    require(engine.paused(), "global pause chord did not pause binds");

    keyUp(engine, PKey, 'P');
    keyUp(engine, AltKey, VK_LMENU);
    keyUp(engine, CtrlKey, VK_LCONTROL);
    keyDown(engine, TriggerKey, 'A');
    require(executor.count == 0, "paused engine executed an action");
}

} // namespace

void runBindEngineTests() {
    testInitialKeyDownAndRepeat();
    testExactModifiers();
    testAltGrDoesNotTriggerCtrlAltBindings();
    testDeviceResetClearsDebounceState();
    testPauseChord();
}
