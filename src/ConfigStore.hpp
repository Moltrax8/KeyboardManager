#pragma once

#include "Types.hpp"

#include <filesystem>

namespace km {

class ConfigStore final {
public:
    ConfigStore();

    [[nodiscard]] Settings load() const;
    [[nodiscard]] bool save(const Settings& settings) const noexcept;

private:
    std::filesystem::path path_;
    mutable bool saveEnabled_{true};
};

} // namespace km
