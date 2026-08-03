# API Key Configuration Guide

## Overview

The Pay Plugin uses API keys for authentication. API keys should **never** be hardcoded in configuration files that are committed to version control.

## Security Best Practices

✅ **DO:**
- Store API keys in environment variables
- Use `.env` files for local development (never commit)
- Use secret management systems in production (HashiCorp Vault, AWS Secrets Manager, etc.)
- Rotate API keys regularly
- Use different keys for different environments
- Apply principle of least privilege to key scopes

❌ **DON'T:**
- Hardcode API keys in `config.json`
- Commit API keys to version control
- Share API keys in plain text
- Use production keys in development
- Use the same key across all environments

## Configuration Methods

### Method 1: Environment Variable (Recommended for Production)

Set the `PAY_API_KEYS` environment variable with comma-separated values:

```bash
export PAY_API_KEYS=key1,key2,key3
build\windows-msvc\examples\pay-server\Release\PayServer.exe
```

Or use a single key with `PAY_API_KEY`:

```bash
export PAY_API_KEY=your-single-api-key
build\windows-msvc\examples\pay-server\Release\PayServer.exe
```

### Method 2: .env File (Development Only)

1. Copy the example file:
```bash
cp .env.example .env
```

2. Edit `.env` with your actual keys:
```env
PAY_API_KEYS=test-dev-key,performance-test-key,admin-test-key
```

3. Source the file before running:
```bash
source .env
build\windows-msvc\examples\pay-server\Release\PayServer.exe
```

**WARNING:** Never commit `.env` to version control! It's already in `.gitignore`.

### Method 3: Secret Management Systems (Production)

#### Docker Secrets
```yaml
# docker-compose.yml
version: '3.8'
services:
  pay-server:
    image: pay-server:latest
    secrets:
      - api_keys
    environment:
      - PAY_API_KEYS_FILE=/run/secrets/api_keys

secrets:
  api_keys:
    file: ./secrets/api_keys.txt
```

#### Kubernetes Secrets
```yaml
apiVersion: v1
kind: Secret
metadata:
  name: pay-api-keys
type: Opaque
stringData:
  keys: "key1,key2,key3"
---
apiVersion: apps/v1
kind: Deployment
spec:
  template:
    spec:
      containers:
      - name: pay-server
        env:
        - name: PAY_API_KEYS
          valueFrom:
            secretKeyRef:
              name: pay-api-keys
              key: keys
```

#### AWS Secrets Manager
```bash
aws secretsmanager create-secret \
  --name prod/pay/api-keys \
  --secret-string "key1,key2,key3"

# Application retrieves using AWS SDK
```

#### HashiCorp Vault
```bash
vault kv put secret/pay/api-keys \
  keys="key1,key2,key3"
```

## API Key Scopes

Configure scopes in `config.json` to control what each key can do:

```json
{
  "custom_config": {
    "pay": {
      "api_key_scopes": {
        "test-dev-key": ["read", "write", "admin", "order_query", "refund_query", "refund"],
        "performance-test-key": ["read", "write", "order_query", "refund_query"],
        "admin-test-key": ["read", "write", "admin", "order_query", "refund_query", "refund", "reconcile"],
        "prod-api-key": ["read", "order_query"]
      },
      "api_key_default_scopes": ["read"]
    }
  }
}
```

### Available Scopes

Scopes are resolved from the request path relative to the configured
`base_path` (default `/api/pay`). The enforced scopes are:

| Scope | Endpoint it gates | Description |
|-------|-------------------|-------------|
| `order_query` | `GET {base}/query` | Query payment orders |
| `refund` | `POST {base}/refund` | Create refunds |
| `refund_query` | `GET {base}/refund/query` | Query refund status |
| `reconcile` | `GET {base}/reconcile/summary` | Access reconciliation data |

Note: routes that resolve to no scope (e.g. create payment, QR create, the
metrics endpoints) are not scope-gated — only a valid API key is required.
Other scope strings (such as `read`) may appear in a key's scope list but are
not matched by any route and have no effect.

## Testing API Keys

### Test with curl:

```bash
# Using X-Api-Key header
curl -H "X-Api-Key: test-dev-key" \
     http://localhost:5566/api/pay/query?order_no=test_123

# Using Authorization Bearer header
curl -H "Authorization: Bearer test-dev-key" \
     http://localhost:5566/api/pay/query?order_no=test_123
```

### Test with multiple keys:

```bash
# Test each key has appropriate scopes
for key in "test-dev-key" "performance-test-key" "admin-test-key"; do
  echo "Testing key: $key"
  curl -s -H "X-Api-Key: $key" \
       -H "X-Api-Key: test-dev-key" \
       http://localhost:5566/api/pay/query?order_no=test_123
done
```

## Key Rotation

To rotate API keys without downtime:

1. Add new key to environment variables
2. Update clients to use new key
3. Monitor for successful migration
4. Remove old key from environment variables
5. Restart server

Example:
```bash
# Phase 1: Add new key
export PAY_API_KEYS="old-key,new-key"

# Phase 2: After clients updated (e.g., 24 hours later)
export PAY_API_KEYS="new-key"

# Phase 3: Restart service
systemctl restart pay-server
```

## Troubleshooting

### "api key not configured"
**Cause:** No API keys in environment variables or config.json

**Solution:** Set `PAY_API_KEYS` or `PAY_API_KEY` environment variable

### "missing api key"
**Cause:** Request doesn't include `X-Api-Key` or `Authorization` header

**Solution:** Add API key to request headers

### "invalid api key"
**Cause:** Provided API key is not in the allowed list

**Solution:** Check environment variables and ensure key is present

### "api key scope not allowed"
**Cause:** API key doesn't have permission for this endpoint

**Solution:** Check `api_key_scopes` in config.json and verify key has required scope

## Production Checklist

- [ ] API keys stored in environment variables
- [ ] No hardcoded keys in config.json
- [ ] `.env` file in `.gitignore`
- [ ] Different keys for dev/staging/prod
- [ ] Key scopes follow least privilege principle
- [ ] Key rotation procedure documented
- [ ] Secret management system configured (if applicable)
- [ ] Monitoring for unauthorized access attempts
- [ ] Incident response plan for compromised keys
