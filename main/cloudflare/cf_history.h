#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * cf_history.h
 *
 * AtomClaw: Cloudflare Workers KV history client
 *
 * Cloudflare Worker は純粋なストレージ。AI処理はしない。
 * 要約生成が必要なときは Worker から needs_summarize フラグが返り、
 * ESP32 が自分の llm_proxy で要約を生成して /update_summary に送る。
 *
 * Provides:
 *   - Summary fetch:    GET  /summary?user_id=...
 *   - History save:     POST /save  (fire-and-forget, async)
 *   - Summary update:   POST /update_summary  (ESP32が生成した要約を保存)
 *
 * On failure: logs a warning and continues — no retry, no crash.
 */

/**
 * Initialize the Cloudflare history module.
 * Loads Worker URL and auth token from NVS or atom_secrets.h.
 */
esp_err_t cf_history_init(void);

/**
 * Returns true if a Cloudflare Worker URL is configured.
 *
 * Use this to branch between "CF enabled" and "local-only" mode at runtime.
 * When false, all cf_* calls are no-ops and the agent falls back to
 * the local PSRAM ring buffer only (last 2 exchanges).
 */
bool cf_history_is_configured(void);

/**
 * Result of cf_get_summary().
 */
typedef struct {
    bool needs_summarize;   /**< true = Workerが「ESP32で要約して」と要求している */
    int  history_count;     /**< クラウドに保存済みの累計ターン数 */
} cf_summary_result_t;

/**
 * Fetch the conversation summary for a user.
 *
 * @param user_id   Discord user ID string.
 * @param buf       Output buffer for the summary text.
 * @param buf_size  Size of buf.
 * @param result    Optional: populated with needs_summarize and history_count.
 * @return ESP_OK on success, ESP_FAIL on network error (buf will be empty).
 */
esp_err_t cf_get_summary(const char *user_id, char *buf, size_t buf_size,
                         cf_summary_result_t *result);

/**
 * Asynchronously save a conversation turn.
 *
 * Creates a short-lived FreeRTOS task that posts to /save and exits.
 * Caller is NOT blocked; failure is silently ignored.
 *
 * @param user_id    Discord user ID.
 * @param role       "user" or "assistant".
 * @param content    Message text.
 * @param timestamp  Unix timestamp (seconds). Pass 0 to use current time.
 */
void cf_save_async(const char *user_id, const char *role,
                   const char *content, uint32_t timestamp);

/**
 * Push an ESP32-generated summary to Cloudflare KV.
 *
 * AIはESP32側で動く。Workerは保存するだけ。
 *
 * @param user_id  Discord user ID.
 * @param summary  ESP32のLLMが生成した要約テキスト。
 * @return ESP_OK on success.
 */
esp_err_t cf_update_summary(const char *user_id, const char *summary);
