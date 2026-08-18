# Ferriot

[![CI](https://github.com/mikelngelous/ferriot/actions/workflows/ci.yml/badge.svg)](https://github.com/mikelngelous/ferriot/actions/workflows/ci.yml)
[![Version](https://img.shields.io/badge/version-0.1.0-blue.svg)](https://github.com/mikelngelous/ferriot/releases)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)]()
[![License](https://img.shields.io/badge/license-Apache_2.0-blue.svg)](LICENSE)

A modern C++17 LWM2M (Lightweight M2M) client for Linux edge and gateways.

Built directly on libcoap with a type-safe object model, RAII, and `Result<T>` error
handling. The public API lives under the `lwm2m::` namespace.

## Why Ferriot

The LWM2M client ecosystem is mostly C libraries built for kilobyte-RAM
microcontrollers (Wakaama, Anjay, IOWA, Zephyr), plus a Java server (Leshan). Their C
callback APIs fit a Linux gateway with C++17 and megabytes of RAM poorly. Ferriot
targets that gap:

- **Native C++17 stack** — built directly on libcoap, not a wrapper over Wakaama's C
  core like the other C++ options.
- **Permissive** — Apache 2.0, free for commercial use.
- **Edge/gateway first** — designed for Linux from the start.

## Footprint

Built natively on an NXP FRDM-i.MX93 (Cortex-A55, aarch64) and registered against a
Leshan server over the network. NoSec, one server, idle after registration:

| Metric | Value |
|--------|-------|
| Example client binary (stripped, libcoap linked static) | 517 KB |
| Resident memory (RSS) while registered | ~1.8 MB |

The 119 unit tests build and pass on the same board. OpenSSL and libcurl are shared
system libraries; libcoap is bundled statically.

![Ferriot on an i.MX93 board, registered in Eclipse Leshan](docs/images/leshan-imx93.png)

*Ferriot running on an NXP FRDM-i.MX93, registered against the public Eclipse Leshan
server. The server has read the Device object over CoAP — manufacturer and model
report as Ferriot.*

## Architecture

```
┌──────────────────────────────────────────────────────────────┐
│                       Application                             │
├──────────────────────────────────────────────────────────────┤
│                      lwm2m::Client                            │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐        │
│  │ Security │ │  Server  │ │  Device  │ │   FOTA   │  ...   │
│  │   (0)    │ │   (1)    │ │   (3)    │ │   (5)    │        │
│  └──────────┘ └──────────┘ └──────────┘ └──────────┘        │
├──────────────────────────────────────────────────────────────┤
│              TLV Codec    │    Result<T>                      │
├──────────────────────────────────────────────────────────────┤
│                     CoAP Transport                            │
│                       (libcoap)                               │
└──────────────────────────────────────────────────────────────┘
```

## Features

- **Strong Types** - Compile-time safety with `ObjectId`, `InstanceId`, `ResourceId`
- **Result<T>** - Rust-inspired error handling, no exceptions in hot paths
- **TLV Codec** - OMA LWM2M spec compliant encoder/decoder
- **CoAP Transport** - Built on libcoap v4.3.4
- **Auto-Reconnection** - Exponential backoff (2s-300s) on server failure
- **Thread-Safe** - `std::recursive_mutex` for concurrent access
- **FOTA Support** - HTTP firmware downloads with async operations

## LWM2M Objects

| ID | Object | Status | Description |
|----|--------|--------|-------------|
| 0 | Security | NoSec | Server credentials |
| 1 | Server | Complete | Registration lifetime, binding |
| 3 | Device | Complete | Manufacturer, model, serial |
| 4 | Connectivity | Complete | Network bearer, signal strength |
| 5 | Firmware Update | Complete | HTTP FOTA downloads |
| 6 | Location | Complete | GPS coordinates |

## Quick Start

```cpp
#include "lwm2m/client.hpp"

int main() {
    // Configure client
    lwm2m::ClientConfig config;
    config.endpoint_name = "my-device-1";
    config.lifetime = "86400";

    lwm2m::Client client(config);

    // Configure device info
    lwm2m::objects::DeviceConfig device_config;
    device_config.manufacturer = "MyCompany";
    device_config.model_number = "Model-X";
    client.device() = lwm2m::objects::DeviceObject(device_config);

    // Add server
    lwm2m::objects::SecurityInstance security;
    security.server_uri = "coap://localhost:5683";
    security.short_server_id = 1;
    client.security().add_server(security);

    lwm2m::objects::ServerInstance server;
    server.short_server_id = 1;
    server.lifetime = 86400;
    client.server().add_server(server);

    // Start and register
    client.start();
    client.register_with_server(1);

    // Run until stopped
    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds{1});
    }

    client.stop();
    return 0;
}
```

## Build

```bash
mkdir build && cd build
cmake -DLWM2M_BUILD_TESTS=ON ..
make -j$(nproc)
ctest --output-on-failure
```

### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `LWM2M_BUILD_TESTS` | ON | Build unit tests |
| `LWM2M_BUILD_EXAMPLES` | ON | Build example client |
| `LWM2M_ENABLE_SANITIZERS` | OFF | Enable ASAN/UBSAN |

## Cross-Compilation (ARM64)

```bash
# Source OpenEmbedded SDK
unset LD_LIBRARY_PATH
source /path/to/oecore-x86_64/environment-setup-aarch64-oe-linux

# Build
mkdir build-arm64 && cd build-arm64
cmake -DLWM2M_BUILD_TESTS=OFF -DLWM2M_BUILD_EXAMPLES=OFF ..
make -j$(nproc)
```

## Requirements

- C++17 compiler (GCC 8+, Clang 7+)
- CMake 3.12+
- OpenSSL 1.1+

## Dependencies

Automatically fetched via CMake FetchContent:

| Dependency | Version | Purpose |
|------------|---------|---------|
| libcoap | v4.3.4 | CoAP/DTLS transport |
| GoogleTest | v1.14.0 | Unit testing |
| jsoncpp | latest | Integration tests |

## License

Apache License 2.0 - see [LICENSE](LICENSE) and [NOTICE](NOTICE) for details.
