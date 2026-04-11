# Pay Plugin Deployment Guide

## Prerequisites

- Ubuntu 20.04+ or Windows Server
- PostgreSQL 12+
- Redis 6+
- CMake 3.5+
- Conan dependency manager
- Valid SSL certificate (for production)

## Build from Source

### 1. Clone Repository

```bash
git clone <repository-url>
cd Pay-plugin-example
```

### 2. Install Dependencies

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install -y cmake build-essential \
  libpq-dev redis-server libssl-dev \
  wget unzip

# Install Conan
pip3 install conan
```

**Windows:**
1. Install Visual Studio 2019 or later
2. Install CMake
3. Install Conan
4. Install PostgreSQL
5. Install Redis

### 3. Build Project

```bash
cd PayBackend/scripts
./build.bat
```

## Database Setup

### 1. Create Database

```sql
CREATE DATABASE pay_test;
```

### 2. Run Migrations

```bash
psql -U postgres -d pay_test -f sql/001_init_pay_tables.sql
psql -U postgres -d pay_test -f sql/002_add_indexes.sql
```

### 3. Create User

```sql
CREATE USER pay_user WITH PASSWORD 'secure_password';
GRANT ALL PRIVILEGES ON DATABASE pay_test TO pay_user;
```

## Redis Setup

### Ubuntu/Debian

```bash
sudo systemctl start redis-server
sudo systemctl enable redis-server
```

### Windows

Download and install Redis from https://redis.io/

## Configuration

### 1. Copy Configuration Template

```bash
cp config.example.json config.json
```

### 2. Update Configuration

Edit `config.json` with your settings:
- WeChat Pay credentials
- Database connection
- Redis connection
- SSL certificate paths

### 3. Validate Configuration

```bash
./PayServer --test-config
```

## Deployment

### Development Environment

```bash
cd PayBackend/build/Release
./PayServer
```

### Production Environment

#### Using Systemd (Linux)

Create `/etc/systemd/system/payserver.service`:

```ini
[Unit]
Description=Pay Server
After=network.target postgresql.service redis.service

[Service]
Type=simple
User=payuser
WorkingDirectory=/opt/payserver
ExecStart=/opt/payserver/PayServer
Restart=always
RestartSec=10
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
```

Enable and start:
```bash
sudo systemctl daemon-reload
sudo systemctl enable payserver
sudo systemctl start payserver
```

#### Using Docker

Create `Dockerfile`:

```dockerfile
FROM ubuntu:22.04

# Install dependencies
RUN apt-get update && apt-get install -y \
    cmake build-essential \
    libpq-dev redis-server \
    libssl-dev wget git

# Clone and build
WORKDIR /app
COPY . .
RUN mkdir build && cd build && \
    cmake .. && \
    make -j$(nproc)

# Expose port
EXPOSE 8080

CMD ["./PayServer"]
```

Build and run:
```bash
docker build -t payserver .
docker run -d -p 8080:8080 payserver
```

## Health Checks

### Check Server Status

```bash
curl http://localhost:8080/api/health
```

Expected response:
```json
{"status": "healthy", "timestamp": "..."}
```

### Check Database Connection

```bash
psql -U pay_user -d pay_test -c "SELECT 1;"
```

### Check Redis Connection

```bash
redis-cli ping
```

## Monitoring

### Logs

Logs are written to:
- Standard output/syslog (Linux)
- Console output (Windows)

### Metrics

Prometheus metrics are exposed at `/metrics` endpoint.

## Backup

### Database Backup

```bash
pg_dump -U pay_user pay_test > backup_$(date +%Y%m%d).sql
```

### Configuration Backup

```bash
cp config.json config.json.backup
```

## Troubleshooting

See [troubleshooting.md](troubleshooting.md) for common issues.
