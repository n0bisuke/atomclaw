#pragma once

#include "esp_err.h"
#include <stddef.h>

/**
 * Render text on the local display.
 * Input JSON:
 *   {"text":"hello"}
 * Optional:
 *   {"title":"Line","text":"hello"}
 */
esp_err_t tool_display_text_execute(const char *input_json, char *output, size_t output_size);

