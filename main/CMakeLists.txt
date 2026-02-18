# Shared sources (both MimiClaw and AtomClaw)
set(SHARED_SRCS
    "bus/message_bus.c"
    "wifi/wifi_manager.c"
    "llm/llm_proxy.c"
    "memory/memory_store.c"
    "cli/serial_cli.c"
    "proxy/http_proxy.c"
    "tools/tool_registry.c"
    "tools/tool_web_search.c"
    "tools/tool_get_time.c"
    "tools/tool_files.c"
    "rgb/rgb.c"
)

# MimiClaw-specific sources
set(MIMI_SRCS
    "mimi.c"
    "display/display.c"
    "display/Vernon_ST7789T/Vernon_ST7789T.c"
    "buttons/multi_button.c"
    "buttons/button_driver.c"
    "imu/I2C_Driver.c"
    "imu/QMI8658.c"
    "imu/imu_manager.c"
    "ui/config_screen.c"
    "telegram/telegram_bot.c"
    "agent/agent_loop.c"
    "agent/context_builder.c"
    "memory/session_mgr.c"
    "gateway/ws_server.c"
    "ota/ota_manager.c"
)

# AtomClaw-specific sources
set(ATOM_SRCS
    "atom_main.c"
    "agent/atom_context.c"
    "memory/atom_session.c"
    "discord/discord_server.c"
    "cloudflare/cf_history.c"
)

# Shared dependencies
set(SHARED_DEPS
    nvs_flash esp_wifi esp_netif esp_http_client esp_http_server
    esp_event json spiffs console vfs esp-tls
    driver esp_timer led_strip
)

# MimiClaw-specific dependencies
set(MIMI_DEPS
    esp_https_ota app_update esp_lcd qrcode
)

# AtomClaw-specific dependencies (PSA crypto for Ed25519)
set(ATOM_DEPS
    mbedtls
)

if(CONFIG_DEVICE_ATOMCLAW)
    # ── AtomClaw build ──────────────────────────────────────────
    message(STATUS "Building AtomClaw (8MB Flash / Discord / Cloudflare)")

    idf_component_register(
        SRCS
            ${SHARED_SRCS}
            ${ATOM_SRCS}
        INCLUDE_DIRS
            "."
        REQUIRES
            ${SHARED_DEPS}
            ${ATOM_DEPS}
    )
else()
    # ── MimiClaw build (default) ────────────────────────────────
    message(STATUS "Building MimiClaw (16MB Flash / Telegram / WebSocket)")

    idf_component_register(
        SRCS
            ${SHARED_SRCS}
            ${MIMI_SRCS}
        INCLUDE_DIRS
            "."
        EMBED_FILES
            "../assets/banner_320x172.rgb565"
        REQUIRES
            ${SHARED_DEPS}
            ${MIMI_DEPS}
    )
endif()
