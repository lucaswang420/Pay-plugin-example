---
name: build-and-test
description: Build the PayBackend C++ project and run unit/integration tests.
---

# Build & Test

Build the PayBackend C++ project and run unit/integration tests.

## Build

The one true build script is `PayBackend/scripts/build.bat`. It handles Conan dependency
installation, CMake configuration, compilation, and copies `config.json` + `certs/`
to the build output directory. Supports `-debug` / `-release` flags (Release is default).

```powershell
# Standard Release build (output: build/Release/PayServer.exe)
cd PayBackend
scripts\build.bat

# Debug build (output: build/Debug/PayServer.exe)
scripts\build.bat -debug
```

### Manual CMake Build (if needed)

Only use when you need a custom CMake configuration that differs from `build.bat`:

```powershell
cd PayBackend\build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DCMAKE_POLICY_DEFAULT_CMP0091=NEW
cmake --build . --config Release
```

Environment uses **C++17** (`set(CMAKE_CXX_STANDARD 17)`) per `CMakeLists.txt`.

## Run Server

```powershell
cd PayBackend
build\Release\PayServer.exe -c build\Release\config.json
```

The build script copies `config.json` and `certs/` to `build/Release/` automatically.

## Test

### Unit & Integration Tests

The test executable (`test_payplugin.exe`) is built alongside the main project:

```powershell
cd PayBackend

# Run all tests
build\Release\test_payplugin.exe

# Filter by test suite
build\Release\test_payplugin.exe --gtest_filter="*Payment*"
build\Release\test_payplugin.exe --gtest_filter="*Refund*"
build\Release\test_payplugin.exe --gtest_filter="*Idempotency*"

# List all available tests
build\Release\test_payplugin.exe --gtest_list_tests

# Verbose output
build\Release\test_payplugin.exe --output-on-failure -V
```

### Docker Integration Tests

Use the `/docker-integration-test` skill for end-to-end tests in a containerized
environment (PostgreSQL + Redis + PayServer).

```powershell
cd PayBackend
docker-compose up -d
python .claude/skills/docker-integration-test/scripts/pay_e2e_test.py
```

## Pre-Commit Verification

The `settings.json` pre-commit hook runs `test_payplugin.exe` before allowing
a `git commit`. If tests fail, the commit is blocked.

## Error Recovery

### Build failures
| Symptom | Fix |
|---------|-----|
| `Error: Conan install failed` | Run `conan install .. --build=missing -s build_type=Release` manually |
| `Error: CMake configuration failed` | Delete `build/` directory and rebuild |
| `fatal error C1083` (missing header) | Check Conan packages are installed; verify `orm_compat.h` exists in `models/` |

### Test failures
| Symptom | Fix |
|---------|-----|
| Database connection errors | Verify PostgreSQL/Redis are running (`docker ps`) |
| `test_payplugin.exe` not found | Rebuild first: `scripts\build.bat` |
| Port conflict (5566) | Kill existing process: `taskkill /F /IM PayServer.exe` |

## Project Context

- **Framework**: Drogon C++17
- **C++ Standard**: C++17
- **Build System**: CMake + Conan
- **Testing**: Google Test (via Drogon test framework)
- **Test Executable**: `test_payplugin.exe`
- **Architecture**: Controller → Service → Plugin → Model (SOA)
