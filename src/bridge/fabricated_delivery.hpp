#pragma once

// Live fabrication's delivery as the game receives it (03_EXECUTION/
// LIVE_FABRICATION/DESIGN.md, "Delivered"): the hull file, its Ed25519
// signature and the fitted GLB, checked here before the native simulation
// reads a byte of them. Every size bounded first; then the signature, over
// kSignedPrefix and the hull file, against the game's public key; then the
// GLB against the SHA-512 the signed hull file names for it. A delivery that
// fails any check is refused, never repaired. The native still bounds every
// field it takes (sim/fabricated.hpp).
//
// The public key is tools/fab/dev_public_key.hex, compiled in. Its private
// key is made by tools/fab/fab.py keygen, outside every repository.
//
// Plain C++ (Monocypher's Ed25519 and SHA-512), so the native suite checks it
// as the bridge runs it.

#include <array>
#include <cstddef>
#include <cstdint>

namespace scraperx::bridge {

enum class DeliveryCheck : std::uint8_t {
    Ok = 0,
    Size = 1,        // a part empty, or longer than a delivery may be
    Signature = 2,   // the signature is not the game's key's, over this hull file
    Model = 3,       // the GLB is not the one the signed hull file names
};

// The bounds on what is received (the hull file's own is the native's).
constexpr std::size_t kSignatureBytes = 64;
constexpr std::size_t kMaxHullBytes = 3208;
constexpr std::size_t kMaxModelBytes = 2U * 1024U * 1024U;
// Where the hull file names its GLB: SHA-512, after its 40-byte head.
constexpr std::size_t kModelHashOffset = 40;
// Signed ahead of the hull file (tools/fab/fab.py SIGNED_PREFIX).
constexpr char kSignedPrefix[] = "ScraperX fabricated object v1\n";

using PublicKey = std::array<std::uint8_t, 32>;

// The game's public key (tools/fab/dev_public_key.hex).
[[nodiscard]] const PublicKey &game_public_key() noexcept;

[[nodiscard]] DeliveryCheck check_delivery(const std::uint8_t *hull, std::size_t hull_size,
                                           const std::uint8_t *signature, std::size_t signature_size,
                                           const std::uint8_t *model, std::size_t model_size,
                                           const PublicKey &key = game_public_key());

[[nodiscard]] const char *delivery_check_name(DeliveryCheck check) noexcept;

} // namespace scraperx::bridge
