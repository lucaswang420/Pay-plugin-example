---
name: db-reset
description: Reset the pay_test development database by dropping and recreating all tables from SQL scripts.
---

# Database Reset

Reset the `pay_test` development database by dropping and recreating all tables from
the SQL scripts in `sql/`.

## Usage

- User invokes: `/db-reset`
- Requires: PostgreSQL running (local or Docker), `psql` available on PATH

## Quick Reset (Recommended)

### Docker PostgreSQL (default)
```powershell
cd examples/pay-server

# Drop existing tables, recreate from SQL scripts
docker exec -i pay_postgres psql -U postgres -d pay_test < sql/000_drop_pay_tables.sql
docker exec -i pay_postgres psql -U postgres -d pay_test < sql/001_init_pay_tables.sql
docker exec -i pay_postgres psql -U postgres -d pay_test < sql/002_add_indexes.sql
docker exec -i pay_postgres psql -U postgres -d pay_test < sql/003_refund_unique_constraint.sql
docker exec -i pay_postgres psql -U postgres -d pay_test < sql/004_ledger_fk.sql

# Verify
docker exec -i pay_postgres psql -U postgres -d pay_test -c "\dt"
```

### Local PostgreSQL
```powershell
cd examples/pay-server
$env:PGPASSWORD="postgres"

# Drop existing tables, recreate from SQL scripts
Get-ChildItem "sql\*.sql" | ForEach-Object {
    psql -h localhost -U postgres -d pay_test -f $_.FullName
}

# Verify
psql -h localhost -U postgres -d pay_test -c "\dt"
```

## SQL Migration Files

All SQL scripts live in `sql/` as a flat directory (no `migrations/` or `seed/` subdirectories):

| File | Purpose |
|------|---------|
| `000_drop_pay_tables.sql` | Drop all existing tables (safe for re-run) |
| `001_init_pay_tables.sql` | Create core tables: `pay_payment`, `pay_refund`, `pay_callback`, `pay_idempotency`, `pay_ledger` |
| `002_add_indexes.sql` | Performance indexes on frequently queried columns |
| `003_refund_unique_constraint.sql` | Enforce refund uniqueness |
| `004_ledger_fk.sql` | Foreign key constraints on ledger table |

**Apply order**: Always run sequentially by numeric prefix (`000` → `001` → `002` → `003` → `004`).

## Database Customisation

| Parameter | Default (Docker) | Default (Local) |
|-----------|-----------------|-----------------|
| Host | localhost | localhost |
| Port | 5432 | 5432 |
| Database | pay_test | pay_test |
| User | postgres | postgres |
| Password | postgres | postgres |

Defaults come from `examples/pay-server/docker-compose.yml`. For local setups, adjust credentials via environment variables.

## ORM Model Regeneration

After resetting the database structure, regenerate ORM models to ensure they match the
schema:

```powershell
cd libs/drogon-pay/src
drogon_ctl create model models
```

For details, see the `/orm-gen` skill.

## Verification

After reset, verify the schema is intact:

```powershell
# List all tables in pay_test
docker exec -i pay_postgres psql -U postgres -d pay_test -c "\dt"

# Expected output:
#  pay_callback
#  pay_idempotency
#  pay_ledger
#  pay_payment
#  pay_refund
```

Run a smoke test to confirm everything works:
```powershell
# From the repository root (DROGON_TEST binary, runs the full suite)
build\windows-msvc\tests\Release\PayBackendTests.exe
```

## Troubleshooting

| Symptom | Fix |
|---------|-----|
| `psql: FATAL: role "postgres" does not exist` | Verify PostgreSQL user; check docker-compose.yml `POSTGRES_USER` |
| `psql: FATAL: database "pay_test" does not exist` | Create database: `docker exec pay_postgres createdb -U postgres pay_test` |
| `drogon_ctl: command not found` | Install drogon_ctl or run from Drogon install directory |
| Tables already exist | Run `000_drop_pay_tables.sql` first (uses `DROP TABLE IF EXISTS`) |
