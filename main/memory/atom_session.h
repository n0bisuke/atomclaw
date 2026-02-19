#pragma once

#include "esp_err.h"
#include <stddef.h>

/**
 * atom_session.h
 *
 * AtomClaw: In-RAM ring buffer session manager.
 *
 * Stores the last ATOM_SESSION_MAX_EXCHANGES (default 3) conversation
 * exchanges (user + assistant pairs) per Discord user in RAM.
 * Prefers PSRAM and falls back to internal RAM when PSRAM is unavailable.
 *
 * - No SPIFFS writes (Flash preservation)
 * - Thread-safe via FreeRTOS mutex
 * - Keyed by Discord user_id (string)
 * - Supports up to ATOM_SESSION_MAX_USERS simultaneous users
 */

#define ATOM_SESSION_MAX_USERS  8

/**
 * Initialize the session module (allocates RAM buffers, creates mutex).
 */
esp_err_t atom_session_init(void);

/**
 * Append a message to the ring buffer for the given user.
 *
 * @param user_id  Discord user ID.
 * @param role     "user" or "assistant".
 * @param content  Message text (will be truncated to ATOM_SESSION_MSG_MAX_LEN).
 */
esp_err_t atom_session_append(const char *user_id,
                              const char *role,
                              const char *content);

/**
 * Serialize recent messages for the given user as a JSON array.
 *
 * Returns a JSON messages array:
 *   [{"role":"user","content":"..."},{"role":"assistant","content":"..."},...]
 *
 * @param user_id   Discord user ID.
 * @param buf       Output buffer.
 * @param buf_size  Size of buf.
 * @param max_msgs  Maximum number of messages to return (0 = return all stored).
 *                  Use 4 (= 2 exchanges) for CF-less local-only mode.
 */
esp_err_t atom_session_get_history_json(const char *user_id,
                                        char *buf, size_t buf_size,
                                        int max_msgs);

/**
 * Clear the session history for the given user.
 */
esp_err_t atom_session_clear(const char *user_id);
