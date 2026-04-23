@echo off
echo Setting up pay_test database...

set PGPASSWORD=123456

psql -U test -d postgres -c "CREATE DATABASE pay_test;"

psql -U test -d pay_test -f sql\001_init_pay_tables.sql
psql -U test -d pay_test -f sql\002_add_indexes.sql

echo Database setup complete!
