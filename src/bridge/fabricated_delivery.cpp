// A fabricated object's delivery, checked before the native reads it (see
// fabricated_delivery.hpp).
#include "bridge/fabricated_delivery.hpp"

#include "monocypher-ed25519.h"

#include <cstring>
#include <vector>

#ifndef SCRAPERX_FAB_PUBLIC_KEY
#error "SCRAPERX_FAB_PUBLIC_KEY (tools/fab/dev_public_key.hex) must be defined by the build"
#endif

namespace scraperx::bridge {

namespace {

constexpr std::uint8_t nibble(const char c) noexcept {
    return static_cast<std::uint8_t>(c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : 0xFF);
}

constexpr PublicKey parse_key(const char (&hex)[65]) noexcept {
    PublicKey key{};
    for (std::size_t i = 0; i < key.size(); ++i) {
        key[i] = static_cast<std::uint8_t>(nibble(hex[2 * i]) << 4U | nibble(hex[2 * i + 1]));
    }
    return key;
}

constexpr bool well_formed(const char (&hex)[65]) noexcept {
    for (std::size_t i = 0; i < 64; ++i) {
        if (nibble(hex[i]) > 15) {
            return false;
        }
    }
    return hex[64] == '\0';
}

constexpr char kKeyHex[65] = SCRAPERX_FAB_PUBLIC_KEY;
static_assert(well_formed(kKeyHex), "the game's public key is 64 lowercase hex digits");
constexpr PublicKey kKey = parse_key(kKeyHex);

} // namespace

const PublicKey &game_public_key() noexcept {
    return kKey;
}

DeliveryCheck check_delivery(const std::uint8_t *hull, const std::size_t hull_size, const std::uint8_t *signature,
                             const std::size_t signature_size, const std::uint8_t *model, const std::size_t model_size,
                             const PublicKey &key) {
    if (hull == nullptr || signature == nullptr || model == nullptr || hull_size < kModelHashOffset + 64 ||
        hull_size > kMaxHullBytes || signature_size != kSignatureBytes || model_size == 0 ||
        model_size > kMaxModelBytes) {
        return DeliveryCheck::Size;
    }
    const std::size_t prefix = sizeof(kSignedPrefix) - 1;
    std::vector<std::uint8_t> message(prefix + hull_size);
    std::memcpy(message.data(), kSignedPrefix, prefix);
    std::memcpy(message.data() + prefix, hull, hull_size);
    if (crypto_ed25519_check(signature, key.data(), message.data(), message.size()) != 0) {
        return DeliveryCheck::Signature;
    }
    std::uint8_t digest[64] = {};
    crypto_sha512(digest, model, model_size);
    if (std::memcmp(digest, hull + kModelHashOffset, sizeof(digest)) != 0) {
        return DeliveryCheck::Model;
    }
    return DeliveryCheck::Ok;
}

const char *delivery_check_name(const DeliveryCheck check) noexcept {
    switch (check) {
    case DeliveryCheck::Ok: return "ok";
    case DeliveryCheck::Size: return "size";
    case DeliveryCheck::Signature: return "signature";
    case DeliveryCheck::Model: return "model";
    }
    return "unknown";
}

} // namespace scraperx::bridge
