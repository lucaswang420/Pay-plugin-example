@echo off
setlocal

REM SQL migrations live at the repository root (script is in examples/pay-server/scripts/)
cd /d "%~dp0..\..\.."
echo Setting up pay_test database...

set PGPASSWORD=123456

echo Dropping existing database...
psql -U test -d postgres -c "DROP DATABASE IF EXISTS pay_test;" >nul 2>&1
if %errorlevel% neq 0 (
    echo Error: Failed to drop database
    endlocal
    exit /b 1
)

echo Creating new database...
psql -U test -d postgres -c "CREATE DATABASE pay_test;" >nul 2>&1
if %errorlevel% neq 0 (
    echo Error: Failed to create database
    endlocal
    exit /b 1
)

echo Applying Dropping tables...
psql -U test -d pay_test -f sql/000_drop_pay_tables.sql >nul 2>&1
if %errorlevel% neq 0 (
    echo Error: Failed to apply OAuth2 core schema
    endlocal
    exit /b 1
)

echo Applying Pay core schema...
psql -U test -d pay_test -f sql/001_init_pay_tables.sql >nul 2>&1
if %errorlevel% neq 0 (
    echo Error: Failed to apply OAuth2 core schema
    endlocal
    exit /b 1
)

echo Applying add indexes...
psql -U test -d pay_test -f sql/002_add_indexes.sql >nul 2>&1
if %errorlevel% neq 0 (
    echo Error: Failed to apply indexes
    endlocal
    exit /b 1
)

echo Applying refund unique constraint...
psql -U test -d pay_test -f sql/003_refund_unique_constraint.sql >nul 2>&1
if %errorlevel% neq 0 (
    echo Error: Failed to apply refund unique constraint
    endlocal
    exit /b 1
)

echo Applying ledger foreign keys...
psql -U test -d pay_test -f sql/004_ledger_fk.sql >nul 2>&1
if %errorlevel% neq 0 (
    echo Error: Failed to apply ledger foreign keys
    endlocal
    exit /b 1
)

echo Database setup complete!
endlocal
exit /b 0
