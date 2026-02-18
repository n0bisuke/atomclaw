#include "atom_context.h"
#include "atom_config.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "atom_ctx";

/* ── Append a file's content to a buffer ────────────────────────────── */

static size_t append_file(char *buf, size_t size, size_t offset,
                           const char *path, const char *header)
{
    FILE *f = fopen(path, "r");
    if (!f) return offset;

    if (header && offset + 8 < size) {
        offset += snprintf(buf + offset, size - offset, "\n## %s\n\n", header);
    }

    size_t n = fread(buf + offset, 1, size - offset - 1, f);
    offset += n;
    if (offset < size) buf[offset] = '\0';
    fclose(f);
    return offset;
}

/* ── System prompt ───────────────────────────────────────────────────── */

esp_err_t atom_context_build_system(char *buf, size_t size, const char *cf_summary)
{
    size_t off = 0;

    off += snprintf(buf + off, size - off,
        "# AtomClaw\n\n"
        "You are AtomClaw, a compact AI assistant running on an ESP32-S3 (8MB Flash) device.\n"
        "You communicate through Discord slash commands.\n\n"
        "Be helpful, accurate, and concise — prefer short answers.\n"
        "Discord messages are limited to 2000 characters.\n\n"
        "## Available Tools\n"
        "- web_search: Search the web for current information.\n"
        "- get_current_time: Get the current date and time. "
          "Always use this tool when the user asks about time or date.\n\n"
        "Use tools when needed. Provide your final answer as plain text.\n\n"
        "## Memory\n"
        "Long-term memory is stored in /spiffs/memory/MEMORY.md.\n"
        "When you learn something important about the user, use write_file to persist it.\n"
        "Keep MEMORY.md under 4KB.\n\n");

    /* SOUL.md */
    off = append_file(buf, size, off, ATOM_SOUL_FILE, "Personality");

    /* USER.md */
    off = append_file(buf, size, off, ATOM_USER_FILE, "User Profile");

    /* MEMORY.md (max 4KB) */
    {
        FILE *f = fopen(ATOM_MEMORY_FILE, "r");
        if (f) {
            char mem[ATOM_MEMORY_MAX_BYTES];
            size_t n = fread(mem, 1, sizeof(mem) - 1, f);
            mem[n] = '\0';
            fclose(f);
            if (n > 0) {
                off += snprintf(buf + off, size - off,
                                "\n## Long-term Memory\n\n%s\n", mem);
            }
        }
    }

    /* Cloudflare summary (cloud conversation history) */
    if (cf_summary && cf_summary[0]) {
        off += snprintf(buf + off, size - off,
                        "\n## Conversation Summary (from cloud)\n\n%s\n", cf_summary);
    }

    ESP_LOGI(TAG, "System prompt: %d bytes", (int)off);
    return ESP_OK;
}

/* ── Messages array ──────────────────────────────────────────────────── */

esp_err_t atom_context_build_messages(const char *history_json,
                                      const char *user_message,
                                      char *buf, size_t size)
{
    cJSON *history = cJSON_Parse(history_json);
    if (!history) history = cJSON_CreateArray();

    cJSON *user_msg = cJSON_CreateObject();
    cJSON_AddStringToObject(user_msg, "role", "user");
    cJSON_AddStringToObject(user_msg, "content", user_message);
    cJSON_AddItemToArray(history, user_msg);

    char *json_str = cJSON_PrintUnformatted(history);
    cJSON_Delete(history);

    if (json_str) {
        strncpy(buf, json_str, size - 1);
        buf[size - 1] = '\0';
        free(json_str);
    } else {
        snprintf(buf, size, "[{\"role\":\"user\",\"content\":\"%s\"}]", user_message);
    }

    return ESP_OK;
}
