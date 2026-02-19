#include "tools/tool_set_atom_led.h"

#include "rgb/rgb.h"
#include "cJSON.h"
#include "esp_err.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

static void apply_preset(const char *preset, int *r, int *g, int *b, bool *ok)
{
    if (!preset) return;
    if (strcmp(preset, "off") == 0) {
        *r = *g = *b = 0; *ok = true; return;
    }
    if (strcmp(preset, "red") == 0) {
        *r = 255; *g = 0; *b = 0; *ok = true; return;
    }
    if (strcmp(preset, "green") == 0) {
        *r = 0; *g = 255; *b = 0; *ok = true; return;
    }
    if (strcmp(preset, "blue") == 0) {
        *r = 0; *g = 0; *b = 255; *ok = true; return;
    }
    if (strcmp(preset, "white") == 0) {
        *r = 255; *g = 255; *b = 255; *ok = true; return;
    }
}

esp_err_t tool_set_atom_led_execute(const char *input_json, char *output, size_t output_size)
{
    int r = -1, g = -1, b = -1;
    bool valid = false;

    cJSON *root = cJSON_Parse(input_json ? input_json : "{}");
    if (!root) {
        snprintf(output, output_size, "Error: invalid JSON");
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *preset = cJSON_GetObjectItem(root, "preset");
    if (preset && cJSON_IsString(preset) && preset->valuestring) {
        apply_preset(preset->valuestring, &r, &g, &b, &valid);
        if (!valid) {
            cJSON_Delete(root);
            snprintf(output, output_size, "Error: unknown preset");
            return ESP_ERR_INVALID_ARG;
        }
    } else {
        cJSON *jr = cJSON_GetObjectItem(root, "r");
        cJSON *jg = cJSON_GetObjectItem(root, "g");
        cJSON *jb = cJSON_GetObjectItem(root, "b");
        if (cJSON_IsNumber(jr) && cJSON_IsNumber(jg) && cJSON_IsNumber(jb)) {
            r = jr->valueint;
            g = jg->valueint;
            b = jb->valueint;
            valid = true;
        }
    }
    cJSON_Delete(root);

    if (!valid || r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) {
        snprintf(output, output_size,
                 "Error: provide preset or r/g/b in 0..255");
        return ESP_ERR_INVALID_ARG;
    }

    rgb_set((uint8_t)r, (uint8_t)g, (uint8_t)b);
    snprintf(output, output_size, "OK: LED set to r=%d g=%d b=%d", r, g, b);
    return ESP_OK;
}
