#include "ActionExecutor.hpp"

#include "AudioEngine.hpp"
#include "Paths.hpp"

#include <Windows.h>
#include <shellapi.h>

#include <filesystem>

namespace km {

WindowsActionExecutor::WindowsActionExecutor(AudioEngine& audio) noexcept : audio_(audio) {}

void WindowsActionExecutor::execute(const Action& action) noexcept {
    try {
        if (action.type == ActionType::PlaySound) {
            const auto path = std::filesystem::path(action.path);
            static_cast<void>(audio_.play(path.is_absolute() ? path : soundsDirectory() / path));
            return;
        }
        if (action.type == ActionType::LaunchFile) {
            const std::filesystem::path path(action.path);
            std::error_code error;
            if (!std::filesystem::exists(path, error)) {
                logMessage(L"Launch target is missing: " + path.wstring());
                return;
            }
            SHELLEXECUTEINFOW info{};
            info.cbSize = sizeof(info);
            info.fMask = SEE_MASK_FLAG_NO_UI | SEE_MASK_ASYNCOK;
            info.lpVerb = L"open";
            info.lpFile = path.c_str();
            info.nShow = SW_SHOWNORMAL;
            if (!ShellExecuteExW(&info)) {
                logMessage(L"Launch action failed: " + path.wstring());
            }
            return;
        }
        if (action.type == ActionType::SendKey && action.virtualKey != 0) {
            INPUT inputs[2]{};
            inputs[0].type = INPUT_KEYBOARD;
            inputs[0].ki.wVk = action.virtualKey;
            inputs[0].ki.dwFlags = KEYEVENTF_EXTENDEDKEY;
            inputs[1] = inputs[0];
            inputs[1].ki.dwFlags = KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP;
            if (SendInput(2, inputs, sizeof(INPUT)) != 2) {
                logMessage(L"SendInput action failed.");
            }
        }
    } catch (...) {
        // Input dispatch is noexcept; allocation or path failures must not terminate the process.
        logMessage(L"Action execution failed.");
    }
}

} // namespace km
