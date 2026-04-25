# Alipay Signature Debugging

## Current Status
- Error: `isv.invalid-signature`
- Signature encoding: Base64 (correct for RSA2)
- sign_type excluded from signature calculation
- URL path fixed
- Parameter order consistent

## Test Data
```
app_id: 9021000162676379
method: alipay.trade.create
charset: utf-8
version: 1.0
sign_type: RSA2
timestamp: 2026-04-24 20:25:19
nonce: <uuid>
biz_content: {"buyer_id":"2088102146225135","out_trade_no":"TEST-xxx","subject":"Test payment","total_amount":"100.00"}
```

## Signature String (without URL encoding)
```
app_id=9021000162676379&biz_content={"buyer_id":"2088102146225135","out_trade_no":"TEST-xxx","subject":"Test payment","total_amount":"100.00"}&charset=utf-8&method=alipay.trade.create&nonce=<uuid>&timestamp=2026-04-24 20:25:19&version=1.0
```

## Next Steps
1. Verify private key format
2. Test with Alipay's online signature tool
3. Check if there's a charset encoding issue
4. Verify RSA signature algorithm (SHA256withRSA)
