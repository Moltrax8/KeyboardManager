#pragma once

#include "Types.hpp"

#include <string>
#include <vector>

namespace km {

struct KeySpec {
    KeyCode key;
    float width{1.0F};
    bool gap{};
};

using KeyboardLayout = std::vector<std::vector<KeySpec>>;

[[nodiscard]] const KeyboardLayout& keyboardLayout(int size);
[[nodiscard]] std::string localizedKeyName(const KeyCode& key);

} // namespace km
