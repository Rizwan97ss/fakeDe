#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace fakede {

// Cryptographic file hashes - reproducibility/chain-of-custody identifiers, not
// similarity/perceptual hashes. SHA-256 via OpenSSL (already linked in for Drogon's
// TLS support); BLAKE3 via the reference C implementation (vcpkg port) as a faster,
// modern alternative some forensic tooling now prefers.
struct FileHashes {
    std::string sha256Hex;
    std::string blake3Hex;
};

FileHashes computeFileHashes(const std::vector<uint8_t>& bytes);

} // namespace fakede
