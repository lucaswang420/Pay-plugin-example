# Pay Plugin Configuration Guide

## Configuration File Location

Configuration is loaded from `config.json` in the build directory.

## Configuration Structure

```json
{
  "app": {
    "threads_num": 16,
    "enable_session": true,
    "session_timeout": 1200
  },
  "listeners": [
    {
      "address": "0.0.0.0",
      "port": 8080,
      "https": {
        "cert": "./certs/server.crt",
        "key": "./certs/server.key",
        "conf": "./certs/openssl.cnf"
      }
    }
  ],
  "plugins": [
    {
      "name": "PayPlugin",
      "dependencies": [],
      "config": {
        "wechat": {
          "appid": "wx1234567890abcdef",
          "mchid": "1234567890",
          "api_v3_key": "your_api_v3_key_here",
          "cert_path": "./certs/apiclient_cert.pem",
          "key_path": "./certs/apiclient_key.pem",
          "serial_no": "serial_number_here",
          "notify_url": "https://yourdomain.com/api/callback",
          "base_url": "https://api.mch.weixin.qq.com"
        },
        "idempotency": {
          "ttl": 604800,
          "use_redis": true
        },
        "database": {
          "host": "localhost",
          "port": 5432,
          "dbname": "pay_test",
          "user": "postgres",
          "password": "postgres"
        },
        "redis": {
          "host": "localhost",
          "port": 6379,
          "database": 0,
          "password": ""
        }
      }
    }
  ]
}
```

## Configuration Parameters

### WeChat Pay Configuration

| Parameter | Description | Required | Default |
|-----------|-------------|----------|---------|
| `appid` | WeChat AppID | Yes | - |
| `mchid` | Merchant ID | Yes | - |
| `api_v3_key` | API v3 Key | Yes | - |
| `cert_path` | Certificate path | Yes | - |
| `key_path` | Private key path | Yes | - |
| `serial_no` | Certificate serial number | Yes | - |
| `notify_url` | Payment callback URL | Yes | - |
| `base_url` | WeChat API base URL | No | https://api.mch.weixin.qq.com |

### Idempotency Configuration

| Parameter | Description | Required | Default |
|-----------|-------------|----------|---------|
| `ttl` | Idempotency record TTL (seconds) | No | 604800 (7 days) |
| `use_redis` | Enable Redis caching | No | true |

### Database Configuration

| Parameter | Description | Required | Default |
|-----------|-------------|----------|---------|
| `host` | Database host | No | localhost |
| `port` | Database port | No | 5432 |
| `dbname` | Database name | No | pay_test |
| `user` | Database user | No | postgres |
| `password` | Database password | No | empty |

### Redis Configuration

| Parameter | Description | Required | Default |
|-----------|-------------|----------|---------|
| `host` | Redis host | No | localhost |
| `port` | Redis port | No | 6379 |
| `database` | Redis database number | No | 0 |
| `password` | Redis password | No | empty |

## Environment Variables

For sensitive data, use environment variables:

```json
{
  "wechat": {
    "api_v3_key": "${WECHAT_API_KEY}",
    "cert_path": "${WECHAT_CERT_PATH}"
  }
}
```

Set environment variables:
```bash
export WECHAT_API_KEY="your_key_here"
export WECHAT_CERT_PATH="/path/to/cert.pem"
```

## Validation

Configuration is validated on startup:
- Required parameters must be present
- Certificate files must exist
- Database connection must succeed
- Redis connection must succeed

If validation fails:
1. Check log output for specific error
2. Verify file paths are correct
3. Ensure database is running
4. Confirm Redis is running
