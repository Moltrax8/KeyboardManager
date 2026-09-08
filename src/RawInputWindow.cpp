#include "RawInputWindow.hpp"

#include "BindEngine.hpp"
#include "Paths.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace km {
namespace {

constexpr wchar_t WindowClassName[] = L"KeyboardManager.RawInput";

} // namespace

RawInputWindow::~RawInputWindow() {
    if (window_ != nullptr) {
        RAWINPUTDEVICE device{0x01, 0x06, RIDEV_REMOVE, nullptr};
        RegisterRawInputDevices(&device, 1, sizeof(device));
        DestroyWindow(window_);
    }
    if (instance_ != nullptr) {
        UnregisterClassW(WindowClassName, instance_);
    }
}

bool RawInputWindow::create(HINSTANCE instance, BindEngine& engine) noexcept {
    instance_ = instance;
    engine_ = &engine;

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = windowProcedure;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = WindowClassName;
    if (RegisterClassExW(&windowClass) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        logMessage(L"Could not register the Raw Input window class.");
        return false;
    }

    window_ = CreateWindowExW(0, WindowClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE,
                              nullptr, instance, this);
    if (window_ == nullptr) {
        logMessage(L"Could not create the Raw Input message window.");
        return false;
    }

    RAWINPUTDEVICE device{};
    device.usUsagePage = 0x01;
    device.usUsage = 0x06;
    device.dwFlags = RIDEV_INPUTSINK | RIDEV_DEVNOTIFY;
    device.hwndTarget = window_;
    if (!RegisterRawInputDevices(&device, 1, sizeof(device))) {
        logMessage(L"RegisterRawInputDevices failed.");
        return false;
    }
    return true;
}

LRESULT CALLBACK RawInputWindow::windowProcedure(const HWND window, const UINT message,
                                                  const WPARAM wParam, const LPARAM lParam) {
    auto* self = reinterpret_cast<RawInputWindow*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<RawInputWindow*>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (message == WM_INPUT && self != nullptr) {
        self->processInput(reinterpret_cast<HRAWINPUT>(lParam));
    } else if (message == WM_INPUT_DEVICE_CHANGE && wParam == GIDC_REMOVAL && self != nullptr) {
        // A removed keyboard cannot deliver its pending KeyUp events; clear debounce state.
        self->engine_->resetDevice(reinterpret_cast<std::uintptr_t>(reinterpret_cast<HANDLE>(lParam)));
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

void RawInputWindow::processInput(const HRAWINPUT input) noexcept {
    UINT size = 0;
    if (GetRawInputData(input, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER)) != 0 || size == 0) {
        return;
    }
    std::vector<std::byte> storage(size);
    if (GetRawInputData(input, RID_INPUT, storage.data(), &size, sizeof(RAWINPUTHEADER)) != size) {
        return;
    }
    const auto* raw = reinterpret_cast<const RAWINPUT*>(storage.data());
    if (raw->header.dwType != RIM_TYPEKEYBOARD || raw->data.keyboard.VKey == 0xFF ||
        raw->data.keyboard.MakeCode == 0) {
        return;
    }

    const RAWKEYBOARD& keyboard = raw->data.keyboard;
    ExtendedFlag extended = ExtendedFlag::None;
    if ((keyboard.Flags & RI_KEY_E0) != 0) {
        extended = ExtendedFlag::E0;
    } else if ((keyboard.Flags & RI_KEY_E1) != 0) {
        extended = ExtendedFlag::E1;
    }
    std::uint16_t virtualKey = keyboard.VKey;
    if (virtualKey == VK_CONTROL) {
        virtualKey = extended == ExtendedFlag::E0 ? VK_RCONTROL : VK_LCONTROL;
    } else if (virtualKey == VK_MENU) {
        virtualKey = extended == ExtendedFlag::E0 ? VK_RMENU : VK_LMENU;
    } else if (virtualKey == VK_SHIFT) {
        virtualKey = static_cast<std::uint16_t>(MapVirtualKeyW(keyboard.MakeCode, MAPVK_VSC_TO_VK_EX));
    }
    engine_->handle({{keyboard.MakeCode, extended}, virtualKey,
                     reinterpret_cast<std::uintptr_t>(raw->header.hDevice),
                     (keyboard.Flags & RI_KEY_BREAK) == 0});
}

} // namespace km
