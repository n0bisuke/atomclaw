#pragma once

#include "esp_err.h"

/**
 * discord_server.h
 *
 * AtomClaw: Discord Interaction HTTP server
 *
 * - Runs an HTTP server on ATOM_DISCORD_HTTP_PORT (default 80)
 * - Handles POST /interactions from Discord
 * - Performs Ed25519 signature verification
 * - Sends immediate deferred response {"type":5}
 * - Pushes message to inbound bus for agent processing
 * - discord_follow_up() sends the final response via Discord's webhook API
 *
 * Deployment: expose via Cloudflare Tunnel / ngrok so Discord can reach the ESP32.
 */

/**
 * Initialize the Discord server.
 * Loads Discord app ID and public key from NVS or atom_secrets.h.
 */
esp_err_t discord_server_init(void);

/**
 * Start the HTTP server.
 * Must be called after WiFi is connected.
 */
esp_err_t discord_server_start(void);

/**
 * Stop the HTTP server.
 */
esp_err_t discord_server_stop(void);

/**
 * Send (or update) the follow-up message for a deferred interaction.
 *
 * @param interaction_token  The token from the original interaction payload.
 * @param text               The response text to send (max 2000 chars).
 * @return ESP_OK on success.
 */
esp_err_t discord_follow_up(const char *interaction_token, const char *text);
