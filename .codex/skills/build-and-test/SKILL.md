---
name: build-and-test
description: Build the pay-server example host C++ project and run unit/integration tests.
---

# Build & Test

Build the pay-server example host C++ project and run unit/integration tests.

## Build

The one true build script is `examples/pay-server/scripts/build.bat`. It handles Conan dependency
installation, CMake configuration, compilation, and copies `config.json` + `certs/`
to the build output directory. Supports `-debug` / `-release` flags (Release is default).

```powershell
# Standard Release build (output: build/windows-msvc/examples/pay-server/Release/PayServer.exe)
cd examples/pay-server
scripts\build.bat

# Debug build (output: build/Debug/PayServer.exe)
scripts\build.bat -debug
```

### Manual CMake Build (if needed)

Only use when you need a custom CMake configuration that differs from `build.bat`:

```powershell
cd build\windows-msvc
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DCMAKE_POLICY_DEFAULT_CMP0091=NEW
cmake --build . --config Release
```

Environment uses **C++17** (`set(CMAKE_CXX_STANDARD 17)`) per `CMakeLists.txt`.

## Run Server

```powershell
cd examples/pay-server
build\windows-msvc\examples\pay-server\Release\PayServer.exe -c build\Release\config.json
```

The build script copies `config.json` and `certs/` to `build/windows-msvc/examples/pay-server/Release/` automatically.

## Test

### Unit & Integration Tests

The test executable (`PayBackendTests.exe`) is built alongside the main project:

```powershell
cd examples/pay-server

# Run all tests
build\windows-msvc\tests\Release\PayBackendTests.exe

# Filter by test suite
build\windows-msvc\tests\Release\PayBackendTests.exe
build\windows-msvc\tests\Release\PayBackendTests.exe
build\windows-msvc\tests\Release\PayBackendTests.exe

# List all available tests
build\windows-msvc\tests\Release\PayBackendTests.exe

# Verbose output
build\windows-msvc\tests\Release\PayBackendTests.exe --output-on-failure -V
```

### Docker Integration Tests

Use the `/docker-integration-test` skill for end-to-end tests in a containerized
environment (PostgreSQL + Redis + PayServer).

```powershell
cd examples/pay-server
docker-compose up -d
python .claude/skills/docker-integration-test/scripts/pay_e2e_test.py
```

## Pre-Commit Verification

The `settings.json` pre-commit hook runs `PayBackendTests.exe` before allowing
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
| `PayBackendTests.exe` not found | Rebuild first: `scripts\build.bat` |
| Port conflict (5566) | Kill existing process: `taskkill /F /IM PayServer.exe` |

## Project Context

- **Framework**: Drogon C++17
- **C++ Standard**: C++17
- **Build System**: CMake + Conan
- **Testing**: Drogon DROGON_TEST framework
- **Test Executable**: `PayBackendTests.exe`
- **Architecture**: Controller → Service → Plugin → Model (SOA)
