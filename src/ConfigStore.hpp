#pragma once

#include "Types.hpp"

#include <filesystem>
#include <string_view>

namespace km {

class ConfigStore final {
public:
    ConfigStore();
    explicit ConfigStore(std::filesystem::path path);

    [[nodiscard]] Settings load() const;
    [[nodiscard]] bool save(const Settings& settings) const noexcept;

private:
    void report(std::wstring_view message) const noexcept;

    std::filesystem::path path_;
    bool diagnosticsEnabled_{true};
    mutable bool saveEnabled_{true};
};

} // namespace km
