#pragma once

// Ferriot - Observe/Notify state (RFC 7641 / OMA LWM2M)

#include "types.hpp"
#include "resource.hpp"
#include "transport/coap_client.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace lwm2m {

// One active Observe relationship (a server observing a resource/instance/object).
struct ObserveState {
    ObjectPath path;
    std::vector<uint8_t> token;
    transport::SessionHandle session = 0;
    uint16_t short_server_id = 0;
    uint32_t seq = 1;   // ++ before each notification, so the first notification is 2
    transport::ContentFormat content_format = transport::ContentFormat::TlvLwm2m;

    // Change detection: a resource compares its ResourceValue; an instance/object
    // compares a hash of its serialized TLV.
    std::optional<ResourceValue> last_value;
    std::optional<uint64_t> last_hash;

    // pmin/pmax timing (0 pmax = no maximum period)
    std::chrono::steady_clock::time_point last_notify{};
    std::chrono::seconds pmin{0};
    std::chrono::seconds pmax{0};
};

// A server may observe the same path twice or over two sessions, so the identity
// of an observation is (session, token), not the path.
struct ObserveKey {
    transport::SessionHandle session;
    std::vector<uint8_t> token;

    bool operator==(const ObserveKey& other) const noexcept {
        return session == other.session && token == other.token;
    }
};

struct ObserveKeyHash {
    std::size_t operator()(const ObserveKey& k) const noexcept {
        std::size_t h = std::hash<transport::SessionHandle>{}(k.session);
        for (uint8_t b : k.token) {
            h = h * 31 + b;
        }
        return h;
    }
};

// FNV-1a hash of a byte buffer, used to detect changes in instance/object TLV.
[[nodiscard]] inline uint64_t fnv1a(const std::vector<uint8_t>& data) noexcept {
    uint64_t h = 1469598103934665603ULL;
    for (uint8_t b : data) {
        h ^= b;
        h *= 1099511628211ULL;
    }
    return h;
}

} // namespace lwm2m
