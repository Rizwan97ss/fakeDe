#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace fakede {

std::string base64Encode(const std::vector<uint8_t>& bytes);

} // namespace fakede
