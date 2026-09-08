#include "Paths.hpp"

#include <Windows.h>
#include <KnownFolders.h>
#include <ShlObj.h>

#include <chrono>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <vector>

namespace km {
namespace {

std::filesystem::path modulePath() {
    std::vector<wchar_t> buffer(512);
    for (;;) {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            return std::filesystem::current_path();
        }
        if (length < buffer.size() - 1) {
            return std::filesystem::path(std::wstring(buffer.data(), length));
        }
        buffer.resize(buffer.size() * 2);
    }
}

} // namespace

std::filesystem::path executableDirectory() {
    const auto path = modulePath();
    return path.has_filename() ? path.parent_path() : path;
}

std::filesystem::path dataDirectory() {
    PWSTR rawPath = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &rawPath))) {
        std::filesystem::path result(rawPath);
        CoTaskMemFree(rawPath);
        result /= L"KeyboardManager";
        std::error_code error;
        std::filesystem::create_directories(result, error);
        return result;
    }

    auto fallback = executableDirectory() / L"Data";
    std::error_code error;
    std::filesystem::create_directories(fallback, error);
    return fallback;
}

std::filesystem::path soundsDirectory() {
    return executableDirectory() / L"Sounds";
}

std::string toUtf8(const std::wstring_view value) {
    if (value.empty()) {
        return {};
    }
    const int length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                                            static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (length <= 0) {
        return {};
    }
    std::string result(static_cast<std::size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
                        result.data(), length, nullptr, nullptr);
    return result;
}

std::wstring fromUtf8(const std::string_view value) {
    if (value.empty()) {
        return {};
    }
    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                            static_cast<int>(value.size()), nullptr, 0);
    if (length <= 0) {
        return {};
    }
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
                        result.data(), length);
    return result;
}

void logMessage(const std::wstring_view message) noexcept {
    try {
        static std::mutex mutex;
        std::scoped_lock lock(mutex);

        const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::tm localTime{};
        localtime_s(&localTime, &now);

        std::ofstream stream(dataDirectory() / L"KeyboardManager.log", std::ios::app | std::ios::binary);
        if (!stream) {
            return;
        }
        stream << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S") << "  " << toUtf8(message) << '\n';
    } catch (...) {
        // Logging must never turn a recoverable bind failure into an application crash.
    }
}

} // namespace km
