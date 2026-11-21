---
id: "005"
title: "Refactor ConfigManager::save_to_file to emit valid JSON with hardened permissions"
type: "refactor"
status: "in-progress"
created_at: "2026-11-17"
updated_at: "2026-11-17"
---

# [Refactor] Refactor ConfigManager::save_to_file to emit valid JSON with hardened permissions

## Description
`ConfigManager::save_to_file` builds JSON by concatenating raw values into string
literals. Any `"`, `\`, newline, or control character in `api_key`, `base_url`, or
`model` produces a corrupt config file that the loader cannot read back. The file is
also created under the process umask (`0664`), leaving the plaintext API key
group-readable. The function additionally returns `true` on a failed stream write.

## Acceptance Criteria
- [ ] Serialize through `json::JsonWriter` so values are properly escaped
- [ ] Emit one top-level key per line so the existing line-based `load_from_file` still parses it
- [ ] Restrict the config file to `0600` (owner read/write only)
- [ ] Detect stream write failure and return `false` instead of silently succeeding
- [ ] Keep the function lock-free (all 8 callers already hold the non-recursive `mutex_`)
- [ ] Add a regression test proving escaping and permission hardening
- [ ] Full test suite green (zero mock theater)

## Context / Notes
- Target files: `src/core/config.cpp`, `tests/test_json.cpp`
- Dependencies: existing `json::JsonWriter`, `json::JsonValue`, `json::JsonObject`
- `JsonObject` is a `std::map`, so keys serialize alphabetically: `api_key`, `base_url`, `max_tokens`, `model`, `server_port`, `temperature`
- Known limitation left untouched: `load_from_file` is a naive colon splitter, so escaped values survive as JSON but not as an exact naive-loader round-trip
