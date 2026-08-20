#include "FileHasher.h"

#include <blake3.h>
#include <openssl/evp.h>

#include <array>
#include <stdexcept>

namespace fakede {

namespace {

std::string toHex(const unsigned char* data, size_t len) {
    static constexpr char kHexDigits[] = "0123456789abcdef";
    std::string out(len * 2, '0');
    for (size_t i = 0; i < len; ++i) {
        out[i * 2] = kHexDigits[data[i] >> 4];
        out[i * 2 + 1] = kHexDigits[data[i] & 0x0F];
    }
    return out;
}

std::string sha256Hex(const std::vector<uint8_t>& bytes) {
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) throw std::runtime_error("failed to allocate EVP_MD_CTX for SHA-256");

    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digestLen = 0;
    const bool ok = EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) == 1 &&
                     EVP_DigestUpdate(ctx, bytes.data(), bytes.size()) == 1 &&
                     EVP_DigestFinal_ex(ctx, digest, &digestLen) == 1;
    EVP_MD_CTX_free(ctx);
    if (!ok) throw std::runtime_error("SHA-256 hashing failed");

    return toHex(digest, digestLen);
}

std::string blake3Hex(const std::vector<uint8_t>& bytes) {
    blake3_hasher hasher;
    blake3_hasher_init(&hasher);
    blake3_hasher_update(&hasher, bytes.data(), bytes.size());

    std::array<uint8_t, BLAKE3_OUT_LEN> output{};
    blake3_hasher_finalize(&hasher, output.data(), output.size());

    return toHex(output.data(), output.size());
}

} // namespace

FileHashes computeFileHashes(const std::vector<uint8_t>& bytes) {
    FileHashes hashes;
    hashes.sha256Hex = sha256Hex(bytes);
    hashes.blake3Hex = blake3Hex(bytes);
    return hashes;
}

} // namespace fakede
