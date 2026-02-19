#include "tools/tool_display_text.h"

#include "display/display.h"
#include "cJSON.h"

#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

static int utf8_decode_one(const unsigned char *s, size_t len, uint32_t *cp, size_t *consumed)
{
    if (!s || len == 0 || !cp || !consumed) return 0;

    unsigned char c0 = s[0];
    if (c0 < 0x80) {
        *cp = c0;
        *consumed = 1;
        return 1;
    }
    if ((c0 & 0xE0) == 0xC0 && len >= 2) {
        unsigned char c1 = s[1];
        if ((c1 & 0xC0) != 0x80) return 0;
        *cp = ((uint32_t)(c0 & 0x1F) << 6) | (uint32_t)(c1 & 0x3F);
        *consumed = 2;
        return 1;
    }
    if ((c0 & 0xF0) == 0xE0 && len >= 3) {
        unsigned char c1 = s[1], c2 = s[2];
        if ((c1 & 0xC0) != 0x80 || (c2 & 0xC0) != 0x80) return 0;
        *cp = ((uint32_t)(c0 & 0x0F) << 12) | ((uint32_t)(c1 & 0x3F) << 6) | (uint32_t)(c2 & 0x3F);
        *consumed = 3;
        return 1;
    }
    if ((c0 & 0xF8) == 0xF0 && len >= 4) {
        unsigned char c1 = s[1], c2 = s[2], c3 = s[3];
        if ((c1 & 0xC0) != 0x80 || (c2 & 0xC0) != 0x80 || (c3 & 0xC0) != 0x80) return 0;
        *cp = ((uint32_t)(c0 & 0x07) << 18) | ((uint32_t)(c1 & 0x3F) << 12) |
              ((uint32_t)(c2 & 0x3F) << 6) | (uint32_t)(c3 & 0x3F);
        *consumed = 4;
        return 1;
    }
    return 0;
}

static size_t utf8_encode_one(uint32_t cp, char *out, size_t out_size)
{
    if (!out || out_size == 0) return 0;
    if (cp < 0x80) {
        if (out_size < 1) return 0;
        out[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800) {
        if (out_size < 2) return 0;
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        if (out_size < 3) return 0;
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    if (out_size < 4) return 0;
    out[0] = (char)(0xF0 | (cp >> 18));
    out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}

static int is_display_safe_codepoint(uint32_t cp)
{
    if (cp == '\n') return 1;
    if (cp >= 0x20 && cp <= 0x7E) return 1;                    /* ASCII */
    if (cp >= 0x3040 && cp <= 0x309F) return 1;                /* Hiragana */
    if (cp >= 0x30A0 && cp <= 0x30FF) return 1;                /* Katakana */
    if (cp >= 0x4E00 && cp <= 0x9FFF) return 1;                /* CJK */
    if (cp == 0x3001 || cp == 0x3002 || cp == 0x30FC) return 1;/* 、 。 ー */
    return 0;
}

static int is_quote_or_markdown(uint32_t cp)
{
    return (cp == '"' || cp == '`' || cp == '*' || cp == '#'
            || cp == 0x2018 || cp == 0x2019 || cp == 0x201C || cp == 0x201D
            || cp == 0x300C || cp == 0x300D || cp == 0x300E || cp == 0x300F);
}

static void sanitize_for_display(const char *in, char *out, size_t out_size)
{
    if (!in || !out || out_size == 0) return;

    const unsigned char *p = (const unsigned char *)in;
    size_t i = 0, n = strlen(in), j = 0;
    while (i < n && j + 1 < out_size) {
        uint32_t cp = 0;
        size_t consumed = 0;
        if (!utf8_decode_one(p + i, n - i, &cp, &consumed)) {
            i++;
            continue;
        }
        i += consumed;

        if (is_quote_or_markdown(cp)) continue;
        if (cp == '\t') cp = ' ';
        if (!is_display_safe_codepoint(cp)) continue;

        size_t written = utf8_encode_one(cp, out + j, out_size - 1 - j);
        if (written == 0) break;
        j += written;
    }
    out[j] = '\0';
}

esp_err_t tool_display_text_execute(const char *input_json, char *output, size_t output_size)
{
    char title_buf[48] = "AtomClaw";
    char text_buf[192] = {0};

    cJSON *root = cJSON_Parse(input_json ? input_json : "{}");
    if (!root) {
        snprintf(output, output_size, "Error: invalid JSON");
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *title = cJSON_GetObjectItem(root, "title");
    cJSON *text = cJSON_GetObjectItem(root, "text");
    if (title && cJSON_IsString(title) && title->valuestring && title->valuestring[0]) {
        strlcpy(title_buf, title->valuestring, sizeof(title_buf));
    }
    if (text && cJSON_IsString(text) && text->valuestring) {
        strlcpy(text_buf, text->valuestring, sizeof(text_buf));
    }
    cJSON_Delete(root);

    if (!display_is_ready()) {
        snprintf(output, output_size, "Error: display not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    if (!text_buf[0]) {
        snprintf(output, output_size, "Error: text is required");
        return ESP_ERR_INVALID_ARG;
    }

    char sanitized[160];
    sanitize_for_display(text_buf, sanitized, sizeof(sanitized));
    if (!sanitized[0]) {
        snprintf(output, output_size, "Error: text became empty after sanitize");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = display_show_text(title_buf, sanitized);
    if (err != ESP_OK) {
        snprintf(output, output_size, "Error: display draw failed (%s)", esp_err_to_name(err));
        return err;
    }
    snprintf(output, output_size, "OK: text rendered");
    return ESP_OK;
}
