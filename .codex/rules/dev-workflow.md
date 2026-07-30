---
description: Dev workflow entry points — prefer scripts\build.sh / scripts\build.bat; skills as explainers
globs:
  - "examples/pay-server/**"
  - "PayFrontend/**"
---

Prefer the project's unified wrappers `scripts\build.bat` (Windows) for any backend task — they encode the same path the CI
uses. The `/build-and-test`, `/orm-gen`, `/db-reset`, `/docker-integration-test` skills are
detailed explainers; call the wrapper first, drop into a skill when you need
the steps spelled out.

## Backend

| Step | Run |
|------|-----|
| Rebuild database | `/db-reset` skill (drops + recreates the PG DB via `psql`) |
| Regenerate ORM models | Use `/orm-gen` skill — calls `drogon_ctl create model` against `model.json` |
| Build | `examples\pay-server\scripts\build.bat` (`-debug` for Debug) |
| Run server | `examples/pay-server/scripts/run_server.bat` |
| Unit / integration tests | `build/windows-msvc/tests/Release/PayBackendTests.exe`. By label: `PayBackendTests.exe
| Full cycle | Build → test → verify |

## Frontend (run inside the respective dir, e.g. `cd PayFrontend`)

| Step | Admin (`PayFrontend`) / User (`PayFrontend`) |
|------|----|
| Install (once) | `npm install` |
| Run dev server | `npm run dev` (admin → `localhost:5174/admin/`; user → `localhost:5173`) |
| Unit tests | `npm run test:unit` |
| E2E tests | `npx playwright test` |
| Production build | `npm run build` |

## Full stack (Docker)

`cd examples/pay-server && docker-compose up -d` (backend API :5566, PostgreSQL :5432, Redis :6379) /
`docker-compose down`. Service names: `payserver`, `postgres`, `redis`. Use the `/docker-integration-test` skill for guided integration checks.

Standard build/run/test flags also live in `README.md` "Quick Start".
