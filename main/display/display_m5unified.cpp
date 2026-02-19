#include "display/display.h"

#include <M5Unified.h>
#include "esp_log.h"

static const char *TAG = "display";
static bool s_ready = false;

static void set_display_font(bool title)
{
    M5.Display.setFont(&fonts::lgfxJapanGothic_12);
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(title ? CYAN : WHITE, BLACK);
}

extern "C" esp_err_t display_init(void)
{
    auto cfg = M5.config();
    M5.begin(cfg);

    M5.Display.setRotation(0);
    M5.Display.fillScreen(BLACK);
    set_display_font(true);
    M5.Display.setCursor(0, 0);
    M5.Display.print("AtomClaw");

    s_ready = true;
    ESP_LOGI(TAG, "M5Unified display ready: %dx%d",
             M5.Display.width(), M5.Display.height());
    return ESP_OK;
}

extern "C" bool display_is_ready(void)
{
    return s_ready;
}

extern "C" void display_show_banner(void)
{
    if (!s_ready) return;
    M5.Display.fillScreen(BLACK);
    set_display_font(true);
    M5.Display.setCursor(4, 4);
    M5.Display.print("AtomClaw");
    set_display_font(false);
    M5.Display.setCursor(4, 30);
    M5.Display.print("Display Ready");
}

extern "C" esp_err_t display_show_text(const char *title, const char *text)
{
    if (!s_ready) return ESP_ERR_INVALID_STATE;
    if (!text || !text[0]) return ESP_ERR_INVALID_ARG;
    if (!title) title = "AtomClaw";

    M5.Display.fillScreen(BLACK);
    set_display_font(true);
    M5.Display.setCursor(4, 4);
    M5.Display.print(title);

    set_display_font(false);
    M5.Display.setCursor(4, 30);
    M5.Display.print(text);
    return ESP_OK;
}

extern "C" void display_set_backlight_percent(uint8_t percent)
{
    if (!s_ready) return;
    uint8_t p = percent > 100 ? 100 : percent;
    M5.Display.setBrightness((uint8_t)((p * 255) / 100));
}

extern "C" uint8_t display_get_backlight_percent(void)
{
    return 100;
}

extern "C" void display_cycle_backlight(void)
{
    /* Not used in AtomClaw path. */
}

extern "C" bool display_get_banner_center_rgb(uint8_t *r, uint8_t *g, uint8_t *b)
{
    if (!r || !g || !b) return false;
    *r = 0;
    *g = 0;
    *b = 0;
    return true;
}

extern "C" void display_show_config_screen(const char *qr_text, const char *ip_text,
                                            const char **lines, size_t line_count, size_t scroll,
                                            size_t selected, int selected_offset_px)
{
    (void)qr_text;
    (void)ip_text;
    (void)lines;
    (void)line_count;
    (void)scroll;
    (void)selected;
    (void)selected_offset_px;
    if (!s_ready) return;
    M5.Display.fillScreen(BLACK);
    set_display_font(false);
    M5.Display.setCursor(4, 4);
    M5.Display.print("Config");
}
