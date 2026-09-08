#include "App.hpp"

#include "Paths.hpp"

#include <Windows.h>
#include <shellapi.h>

#include <string_view>

namespace {

class UniqueHandle final {
public:
    explicit UniqueHandle(HANDLE handle) : handle_(handle) {}
    ~UniqueHandle() {
        if (handle_ != nullptr) {
            CloseHandle(handle_);
        }
    }
    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;

    [[nodiscard]] HANDLE get() const noexcept {
        return handle_;
    }

private:
    HANDLE handle_{};
};

bool hasArgument(const wchar_t* expected) {
    int count = 0;
    LPWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    if (arguments == nullptr) {
        return false;
    }
    bool found = false;
    for (int index = 1; index < count; ++index) {
        if (_wcsicmp(arguments[index], expected) == 0) {
            found = true;
            break;
        }
    }
    LocalFree(arguments);
    return found;
}

bool signalExisting(const UINT message) {
    if (HWND window = FindWindowW(km::MainWindowClass, nullptr)) {
        DWORD processId = 0;
        GetWindowThreadProcessId(window, &processId);
        UniqueHandle process(OpenProcess(SYNCHRONIZE, FALSE, processId));
        DWORD_PTR ignored = 0;
        if (SendMessageTimeoutW(window, message, 0, 0, SMTO_ABORTIFHUNG, 3000, &ignored) == 0) {
            return false;
        }
        if (message == km::WindowMessageShutdown) {
            return process.get() != nullptr && WaitForSingleObject(process.get(), 10000) == WAIT_OBJECT_0;
        }
    }
    return true;
}

} // namespace

int WINAPI wWinMain(const HINSTANCE instance, HINSTANCE, PWSTR, int) {
    if (hasArgument(L"/shutdown")) {
        return signalExisting(km::WindowMessageShutdown) ? 0 : 2;
    }

    UniqueHandle mutex(CreateMutexW(nullptr, FALSE, km::InstanceMutexName));
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        static_cast<void>(signalExisting(km::WindowMessageRestore));
        return 0;
    }

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    int result = 1;
    {
        km::App app(instance);
        if (!app.initialize()) {
            km::logMessage(L"Application initialization failed.");
        } else {
            result = app.run();
        }
    }
    if (SUCCEEDED(comResult)) {
        CoUninitialize();
    }
    return result;
}
