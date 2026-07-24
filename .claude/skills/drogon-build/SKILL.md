---
name: drogon-build
description: Build Drogon C++ application (PayBackend) using the project's Conan + CMake + MSVC toolchain via scripts\build.bat.
---

# Drogon Build

Build the PayBackend C++ application using the project's Conan + CMake + MSVC
toolchain via `PayBackend/scripts/build.bat`.

## Quick Build

```powershell
cd PayBackend
scripts\build.bat
```

### Debug Build
```powershell
cd PayBackend
scripts\build.bat -debug
```

### What the build script does (step by step)

1. Kills any running `PayServer.exe` processes (avoids file-lock conflicts)
2. `cd PayBackend\build`
3. `conan install .. --build=missing -s build_type=Release` — resolves C++ dependencies
4. `cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DCMAKE_POLICY_DEFAULT_CMP0091=NEW`
5. `cmake --build . --config Release` — compiles the project
6. Copies `config.json`, `.env`, and `certs/` to `build/Release/`

### Build outputs

| Artifact | Path |
|----------|------|
| Server executable | `build/Release/PayServer.exe` |
| Test executable | `build/Release/test_payplugin.exe` |
| Config | `build/Release/config.json` |
| Certs | `build/Release/certs/` |

## Manual CMake (for custom configurations)

If you need a non-standard CMake setup (different generator, toolchain, etc.), bypass
`build.bat`:

```powershell
cd PayBackend\build

# Configure
cmake .. -G "Visual Studio 17 2022" -A x64 `
    -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake `
    -DCMAKE_POLICY_DEFAULT_CMP0091=NEW

# Build
cmake --build . --config Release

# Copy config
Copy-Item ..\config.json Release\ -Force
if (Test-Path "..\certs") { Copy-Item ..\certs Release\certs\ -Recurse -Force }
```

## Running After Build

```powershell
# Start the server
cd PayBackend
build\Release\PayServer.exe -c build\Release\config.json

# Or with logging
build\Release\PayServer.exe -c build\Release\config.json --log-level debug
```

## Build Troubleshooting

| Symptom | Fix |
|---------|-----|
| `error MSB1009: Project file does not exist` | Run `scripts\build.bat` from `PayBackend/` (not subdirectory) |
| `conan: command not found` | `pip install conan` or add Conan to PATH |
| `cmake: command not found` | Install CMake and add to PATH |
| `Error: Conan install failed` | `conan install .. --build=missing -s build_type=Release --output-folder .` |
| `fatal error C1083` (drogon headers) | Drogon not installed via Conan — check `conanfile.txt` |
| `LNK1104: cannot open file 'PayServer.exe'` | `taskkill /F /IM PayServer.exe` then retry |
| `D9035: option 'FI' has been deprecated` | Normal MSVC warning, safe to ignore |

## Project Structure

```
PayBackend/
├── build/                  # CMake build directory
├── certs/                  # TLS certificates (copied to build output)
├── config.json             # Server configuration (copied by build.bat)
├── controllers/            # HTTP request handlers
├── filters/                # Middleware/filters
├── models/                 # ORM generated models (DO NOT EDIT manually)
├── plugins/                # Third-party payment clients
├── scripts/
│   └── build.bat           # ← THE build script
├── services/               # Business logic layer
├── sql/                    # Database migrations
├── test/                   # Google Test test files
├── utils/                  # Utility functions
├── CMakeLists.txt          # CMake project definition (C++17)
├── docker-compose.yml      # Container orchestration
├── Dockerfile              # Container image definition
└── model.json              # ORM model definition for drogon_ctl
```

## Context

- **C++ Standard**: C++17 (set in `CMakeLists.txt`)
- **Build System**: CMake ≥ 3.5 + Conan 2.x
- **Compiler**: MSVC (Windows), GCC/Clang (Linux) — supports both
- **Framework**: Drogon C++ web framework
- **Dependencies**: Drogon, OpenSSL (via Conan)
- **ORM**: drogon_ctl generated models from PostgreSQL schema
