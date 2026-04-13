# Pay Plugin Project

## Project Overview

This is a Drogon-based payment plugin with Service architecture.

## Build Rules

**CRITICAL:** This project has specific build requirements that MUST be followed:

### Windows Build
- **Required:** Use `scripts/build.bat` for compilation
- **Build Type:** MUST be Release mode
- **Dependency Management:** Uses Conan for third-party libraries
- **Drogon Version:** Local Drogon is built as Release, so this project must also be Release

### Why This Matters
- Drogon framework is locally compiled in Release mode
- Mixing Debug/Release between Drogon and this plugin causes linker errors
- Conan manages all third-party dependencies automatically
- build.bat handles all necessary configuration

### Compilation Command
```bash
cd PayBackend
..\scripts\build.bat
```

**NEVER use:**
- `cmake --build build` directly
- Debug mode builds
- Visual Studio's build button (unless configured for Release)

## Architecture

- Service-based architecture (PaymentService, RefundService, CallbackService, etc.)
- Async callback pattern
- Idempotency support
- PostgreSQL + Redis

## Testing

- Framework: Google Test
- Test executable: `build/Release/test_payplugin.exe`
- Run from: `PayBackend/` directory

## Current Work

Phase 3 of Option B implementation: Migrating remaining tests to new Service API
