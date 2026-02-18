#pragma once

#include "esp_err.h"
#include <stddef.h>

/**
 * atom_context.h
 *
 * AtomClaw: Context builder for the agent loop.
 *
 * Assembles the system prompt from:
 *   1. Hardcoded AtomClaw identity
 *   2. SOUL.md      (personality)
 *   3. USER.md      (user profile)
 *   4. MEMORY.md    (long-term memory, max 4KB)
 *   5. CF summary   (cloud conversation summary, optional)
 *
 * The conversation messages array is built from:
 *   - Recent history JSON (from atom_session)
 *   - Current user message appended at the end
 */

/**
 * Build the system prompt.
 *
 * @param buf       Output buffer for the system prompt string.
 * @param size      Size of buf.
 * @param cf_summary  Cloudflare summary string (may be NULL or empty).
 * @return ESP_OK on success.
 */
esp_err_t atom_context_build_system(char *buf, size_t size, const char *cf_summary);

/**
 * Build the messages JSON array by appending the current user message
 * to the existing history JSON.
 *
 * @param history_json  JSON array string from atom_session_get_history_json().
 * @param user_message  Current user input.
 * @param buf           Output buffer for the combined messages JSON.
 * @param size          Size of buf.
 * @return ESP_OK on success.
 */
esp_err_t atom_context_build_messages(const char *history_json,
                                      const char *user_message,
                                      char *buf, size_t size);
