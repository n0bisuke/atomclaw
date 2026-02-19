#pragma once

#include "esp_err.h"
#include <stddef.h>

/**
 * Set ATOMS3 built-in RGB LED color.
 * Input JSON:
 *   {"r":255,"g":0,"b":0}
 * or
 *   {"preset":"red"}  // red|green|blue|white|off
 */
esp_err_t tool_set_atom_led_execute(const char *input_json, char *output, size_t output_size);

