#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace km {

std::filesystem::path executableDirectory();
std::filesystem::path dataDirectory();
std::filesystem::path soundsDirectory();
std::string toUtf8(std::wstring_view value);
std::wstring fromUtf8(std::string_view value);
void logMessage(std::wstring_view message) noexcept;

} // namespace km
