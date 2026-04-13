# Alipay Sandbox Quick Start Guide

## Overview

Alipay Sandbox is a FREE testing environment for Alipay payments that requires NO business license - perfect for individual developers!

## Benefits

- Individual account registration (no business license)
- Completely free
- Full API support
- Simulates real payment flow
- No real money required

## Quick Setup (5 minutes)

### 1. Register Alipay Developer Account

Visit: https://open.alipay.com/

- Sign up with your personal Alipay account
- Go to "Console" after login

### 2. Access Sandbox Environment

In the left menu, navigate to:

```
R&D Services -> Sandbox Environment
```

Or visit: https://openhome.alipay.com/platform/appDaily.htm

### 3. Get Sandbox Parameters

**Copy these values from the sandbox page:**

```
APPID: 2021000000000000 (example)
Gateway URL: https://openapi.alipaydev.com/gateway.do
```

**Sandbox Accounts (for testing):**

Seller (Merchant):
- Email: sandbox_seller@example.com
- UID: 2088102146225135
- Password: 111111

Buyer:
- Email: sandbox_buyer@example.com
- UID: 2088102146225135
- Password: 111111
- Balance: 999999.00 CNY

### 4. Generate Keys

**Option A: Use Alipay Key Tool (Recommended for Windows/Mac)**

1. Download "RSA Signature Tool" from sandbox page
2. Run the tool
3. Select "RSA2 (2048 bit)"
4. Click "Generate Private Key" and "Generate Public Key"
5. Save both keys

**Option B: Use OpenSSL (Linux)**

```bash
mkdir -p PayBackend/certs/alipay
cd PayBackend/certs/alipay

# Generate private key
openssl genrsa -out app_private_key.pem 2048

# Extract public key
openssl rsa -in app_private_key.pem -pubout -out app_public_key.pem

# Convert to PKCS#8 format
openssl pkcs8 -topk8 -inform PEM -in app_private_key.pem -outform PEM -nocrypt -out app_private_key_pkcs8.pem

# Rename
mv app_private_key_pkcs8.pem app_private_key.pem
```

### 5. Upload Public Key to Alipay

1. In the sandbox page, find "Set Application Public Key"
2. Paste your public key content (including BEGIN/END tags)
3. Click "Save"
4. Click "View Alipay Public Key"
5. Copy Alipay's public key

### 6. Save Alipay Public Key

Create file: `PayBackend/certs/alipay/alipay_public_key.pem`

Paste the Alipay public key content.

## Configuration

### Update config.json

Add to `PayBackend/config.json`:

```json
{
  "plugins": [
    {
      "name": "PayPlugin",
      "dependencies": [],
      "config": {
        "payment_provider": "alipay_sandbox",
        "alipay_sandbox": {
          "app_id": "YOUR_SANDBOX_APPID",
          "seller_id": "YOUR_SANDBOX_SELLER_EMAIL_OR_UID",
          "gateway_url": "https://openapi.alipaydev.com/gateway.do",
          "private_key_path": "./certs/alipay/app_private_key.pem",
          "alipay_public_key_path": "./certs/alipay/alipay_public_key.pem"
        }
      }
    }
  ]
}
```

**Replace placeholders:**

- `YOUR_SANDBOX_APPID` - Your sandbox APPID from step 3
- `YOUR_SANDBOX_SELLER_EMAIL_OR_UID` - Your sandbox seller email or UID

### Verify Files

Check that these files exist:

```bash
ls -la PayBackend/certs/alipay/
# Expected:
# app_private_key.pem
# app_public_key.pem
# alipay_public_key.pem
```

## Testing

### 1. Set API Key

```bash
export PAY_API_KEYS=test-dev-key
```

### 2. Start Server

```bash
cd PayBackend
./build/Release/PayServer.exe
```

### 3. Create Payment

```bash
curl -X POST http://localhost:5566/pay/create \
  -H "Content-Type: application/json" \
  -H "X-Api-Key: test-dev-key" \
  -d '{
    "user_id": "10001",
    "amount": "0.01",
    "currency": "CNY",
    "title": "Sandbox Test"
  }'
```

### 4. Complete Payment with Sandbox Buyer

1. Login to Alipay Sandbox with buyer account
2. Scan the QR code returned from step 3
3. Enter password: 111111
4. Payment completed!

### 5. Query Order

```bash
curl "http://localhost:5566/pay/query?order_no=ORDER_NO" \
  -H "X-Api-Key: test-dev-key"
```

## Key Differences from Production

| Feature | Sandbox | Production |
|---------|---------|------------|
| Registration | Personal account OK | Business license required |
| Cost | Free | 0.6% per transaction |
| Gateway | openapi.alipaydev.com | openapi.alipay.com |
| Money | Virtual (999999 CNY) | Real money |
| Accounts | Sandbox accounts only | Real user accounts |

## When Ready for Production

1. Apply for Alipay merchant account with business license
2. Replace `gateway_url` with: `https://openapi.alipay.com/gateway.do`
3. Update `app_id` and `seller_id` with production values
4. No code changes needed!

## Troubleshooting

### "Signature verification failed"

- Ensure public key uploaded to sandbox matches your private key
- Check Alipay public key is correctly saved

### "App does not exist"

- Verify you're using sandbox APPID (starts with 2021)
- Ensure using sandbox gateway URL

### "Buyer account cannot login"

- Make sure you're on sandbox environment page
- Use buyer account credentials, not seller

## Resources

- [Alipay Open Platform](https://open.alipay.com/)
- [Sandbox Environment](https://openhome.alipay.com/platform/appDaily.htm)
- [API Documentation](https://opendocs.alipay.com/open/02ivbs)

---

**Last Updated:** 2026-04-13  
**Status:** ✅ Ready for individual developers
