// =============================================================================
// api/status.js - tells the web page whether the shared online database is set up.
//   GET /api/status -> {"configured": true|false}
// =============================================================================
'use strict';

module.exports = function handler(req, res) {
  const configured = Boolean((process.env.KV_REST_API_URL || process.env.UPSTASH_REDIS_REST_URL) &&
    (process.env.KV_REST_API_TOKEN || process.env.UPSTASH_REDIS_REST_TOKEN));
  res.statusCode = 200;
  res.setHeader('Content-Type', 'application/json; charset=utf-8');
  res.setHeader('Cache-Control', 'no-store');
  res.end(JSON.stringify({ configured }));
};
