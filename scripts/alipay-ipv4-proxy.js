// Local IPv4-forcing forward proxy for the Alipay sandbox gateway.
//
// Why: hosts without IPv6 egress hit a 21s TCP SYN timeout against the sandbox
// domain, because it publishes AAAA records and trantor's resolver prefers
// IPv6 (no Happy Eyeballs fallback). This proxy forwards over IPv4 only.
//
// Usage:
//   node scripts/alipay-ipv4-proxy.js
//   # then point the gateway at it, e.g. in pay-server .env:
//   #   ALIPAY_SANDBOX_GATEWAY_URL=http://127.0.0.1:18443/gateway.do
//
// Dev/verification helper — not part of the product; do not use in production.
const http = require('http')
const https = require('https')

const TARGET = 'openapi-sandbox.dl.alipaydev.com'
const PORT = 18443

http
  .createServer((req, res) => {
    const chunks = []
    console.log(`[${new Date().toISOString()}] ${req.method} ${req.url} from ${req.socket.remoteAddress}`)
    req.on('data', (c) => chunks.push(c))
    req.on('end', () => {
      const body = Buffer.concat(chunks)
      const headers = { ...req.headers, host: TARGET, 'content-length': body.length }
      delete headers.connection
      const upstream = https.request(
        { host: TARGET, family: 4, port: 443, path: req.url, method: req.method, headers },
        (ur) => {
          const h = { ...ur.headers }
          delete h['transfer-encoding']
          delete h['content-length']
          delete h.connection
          res.writeHead(ur.statusCode, h)
          ur.pipe(res)
        }
      )
      upstream.on('error', (e) => {
        res.writeHead(502, { 'content-type': 'text/plain' })
        res.end('proxy error: ' + e.message)
      })
      upstream.end(body)
    })
  })
  .listen(PORT, '127.0.0.1', () => {
    console.log(`alipay ipv4 proxy: http://127.0.0.1:${PORT} -> https://${TARGET} (family=4)`)
  })
