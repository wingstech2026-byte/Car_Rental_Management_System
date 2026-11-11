// =============================================================================
// api/data.js - shared online database for the Car Rental Management System
// -----------------------------------------------------------------------------
// A Vercel serverless function that stores ALL data of the agency as one text
// "bundle" (the format of the C++ class DataFileFormat) in an Upstash Redis
// database, so several users - in browsers or in the Windows program - work
// with the same data.
//
//   GET  /api/data          -> the bundle (header X-CRMS-Version)
//   PUT  /api/data          -> stores a new bundle
//
// Security:
//   * Every request must log in with HTTP Basic authentication (user name and
//     password of an account of the system). The password is checked against
//     the salted SHA-256 hash stored in the bundle's users.txt section, with
//     exactly the same algorithm as the C++ class User.
//   * A Clerk may save rentals and returns but may not change user accounts.
//   * Optimistic concurrency: a save must send the version it loaded
//     (X-CRMS-Version). If somebody else saved in the meantime, the save is
//     refused with 409 Conflict instead of silently overwriting their work.
//
// Configuration: connect an Upstash Redis database to the Vercel project
// (Storage tab). Vercel then sets KV_REST_API_URL / KV_REST_API_TOKEN (or
// UPSTASH_REDIS_REST_URL / UPSTASH_REDIS_REST_TOKEN) automatically.
// =============================================================================
'use strict';

const crypto = require('crypto');

const VERSION_KEY = 'crms:version';
const BUNDLE_KEY = 'crms:bundle';
const HASH_ITERATIONS = 1000;
const MAX_BUNDLE_BYTES = 2 * 1024 * 1024;

const SAVE_SCRIPT = [
  "local current = redis.call('GET', KEYS[1]) or '0'",
  'if current ~= ARGV[1] then return -1 end',
  "redis.call('SET', KEYS[2], ARGV[2])",
  "redis.call('SET', KEYS[1], ARGV[3])",
  'return 1',
].join('\n');

function databaseConfig() {
  const url = process.env.KV_REST_API_URL || process.env.UPSTASH_REDIS_REST_URL;
  const token = process.env.KV_REST_API_TOKEN || process.env.UPSTASH_REDIS_REST_TOKEN;
  return url && token ? { url, token } : null;
}

async function redis(config, command) {
  const response = await fetch(config.url, {
    method: 'POST',
    headers: { Authorization: 'Bearer ' + config.token, 'Content-Type': 'application/json' },
    body: JSON.stringify(command),
  });
  const json = await response.json();
  if (!response.ok || json.error) {
    throw new Error('Database error: ' + (json.error || response.status));
  }
  return json.result;
}

// ---- Same password hashing as the C++ class User ------------------------------
function hashPassword(salt, password) {
  const sha = (text) => crypto.createHash('sha256').update(text, 'utf8').digest('hex');
  let hash = sha(salt + ':' + password);
  for (let round = 1; round < HASH_ITERATIONS; round += 1) {
    hash = sha(salt + ':' + hash);
  }
  return hash;
}

// Returns the lines of one section of a bundle ("--- users.txt").
function bundleSection(bundle, name) {
  const lines = String(bundle || '').split(/\r?\n/);
  const result = [];
  let inside = false;
  for (const line of lines) {
    if (line.startsWith('--- ')) {
      inside = line.slice(4).trim() === name;
      continue;
    }
    if (inside && line.trim() !== '') result.push(line.trim());
  }
  return result;
}

function parseUsers(bundle) {
  const users = new Map();
  for (const line of bundleSection(bundle, 'users.txt')) {
    const [username, role, salt, hash] = line.split('|').map((part) => (part || '').trim());
    if (username && salt && hash) users.set(username.toLowerCase(), { role, salt, hash });
  }
  return users;
}

function readCredentials(req) {
  const header = req.headers.authorization || '';
  if (!header.startsWith('Basic ')) return null;
  const decoded = Buffer.from(header.slice(6), 'base64').toString('utf8');
  const colon = decoded.indexOf(':');
  if (colon < 0) return null;
  return { username: decoded.slice(0, colon).trim().toLowerCase(), password: decoded.slice(colon + 1) };
}

// Checks the credentials against the accounts in a bundle. Returns the account or null.
function authenticate(credentials, bundle) {
  if (!credentials) return null;
  const user = parseUsers(bundle).get(credentials.username);
  if (!user) return null;
  const expected = Buffer.from(user.hash, 'utf8');
  const actual = Buffer.from(hashPassword(user.salt, credentials.password), 'utf8');
  return expected.length === actual.length && crypto.timingSafeEqual(expected, actual) ? user : null;
}

function send(res, status, body, headers = {}) {
  res.statusCode = status;
  res.setHeader('Cache-Control', 'no-store');
  for (const [key, value] of Object.entries(headers)) res.setHeader(key, value);
  if (typeof body === 'string') {
    res.setHeader('Content-Type', 'text/plain; charset=utf-8');
    res.end(body);
  } else {
    res.setHeader('Content-Type', 'application/json; charset=utf-8');
    res.end(JSON.stringify(body));
  }
}

async function readBody(req) {
  if (typeof req.body === 'string') return req.body;
  if (Buffer.isBuffer(req.body)) return req.body.toString('utf8');
  const chunks = [];
  let size = 0;
  for await (const chunk of req) {
    size += chunk.length;
    if (size > MAX_BUNDLE_BYTES) throw new Error('The data is too large.');
    chunks.push(chunk);
  }
  return Buffer.concat(chunks).toString('utf8');
}

module.exports = async function handler(req, res) {
  const config = databaseConfig();
  if (!config) {
    send(res, 503, { error: 'The shared online database is not configured.' });
    return;
  }

  try {
    const credentials = readCredentials(req);
    const [storedVersion, storedBundle] = await Promise.all([
      redis(config, ['GET', VERSION_KEY]),
      redis(config, ['GET', BUNDLE_KEY]),
    ]);
    const version = storedVersion ? String(storedVersion) : '0';
    const empty = !storedBundle;

    if (req.method === 'GET') {
      if (empty) {
        // Nothing stored yet: the first user starts with the default data and saves it.
        send(res, 200, '', { 'X-CRMS-Version': '0' });
        return;
      }
      if (!authenticate(credentials, storedBundle)) {
        send(res, 401, { error: 'Incorrect user name or password.' }, { 'WWW-Authenticate': 'Basic realm="CRMS"' });
        return;
      }
      send(res, 200, storedBundle, { 'X-CRMS-Version': version });
      return;
    }

    if (req.method === 'PUT') {
      const bundle = await readBody(req);
      if (!bundle.startsWith('CRMS-BUNDLE 1')) {
        send(res, 400, { error: 'The data is not a valid bundle.' });
        return;
      }
      if (Buffer.byteLength(bundle, 'utf8') > MAX_BUNDLE_BYTES) {
        send(res, 413, { error: 'The data is too large.' });
        return;
      }

      // Who is saving? Checked against the stored accounts (or, for the very
      // first save, against the accounts in the new data).
      const user = authenticate(credentials, empty ? bundle : storedBundle);
      if (!user) {
        send(res, 401, { error: 'Incorrect user name or password.' });
        return;
      }
      if (user.role !== 'Manager' && !empty &&
          bundleSection(bundle, 'users.txt').join('\n') !== bundleSection(storedBundle, 'users.txt').join('\n')) {
        send(res, 403, { error: 'Access denied: only a Manager may change user accounts.' });
        return;
      }

      const expected = String(req.headers['x-crms-version'] || '0');
      const next = String(Number(expected) + 1);
      const result = await redis(config, ['EVAL', SAVE_SCRIPT, '2', VERSION_KEY, BUNDLE_KEY, expected, bundle, next]);
      if (Number(result) !== 1) {
        send(res, 409, { error: 'The shared data was changed by another user. Reload and repeat your changes.' });
        return;
      }
      send(res, 200, { version: next }, { 'X-CRMS-Version': next });
      return;
    }

    res.setHeader('Allow', 'GET, PUT');
    send(res, 405, { error: 'Method not allowed.' });
  } catch (error) {
    send(res, 500, { error: String(error && error.message ? error.message : error) });
  }
};
