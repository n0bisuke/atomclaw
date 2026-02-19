#pragma once

/* AtomClaw Global Configuration
 * ESP32-S3 8MB Flash / 8MB PSRAM - Discord + Cloudflare hybrid
 */

/* Build-time secrets (highest priority) */
#if __has_include("atom_secrets.h")
#include "atom_secrets.h"
#endif

/* WiFi */
#ifndef ATOM_SECRET_WIFI_SSID
#define ATOM_SECRET_WIFI_SSID           ""
#endif
#ifndef ATOM_SECRET_WIFI_PASS
#define ATOM_SECRET_WIFI_PASS           ""
#endif

/* LLM API */
#ifndef ATOM_SECRET_API_KEY
#define ATOM_SECRET_API_KEY             ""
#endif
#ifndef ATOM_SECRET_MODEL
#define ATOM_SECRET_MODEL               ""
#endif
#ifndef ATOM_SECRET_MODEL_PROVIDER
#define ATOM_SECRET_MODEL_PROVIDER      "anthropic"
#endif

/* Discord - application credentials */
#ifndef ATOM_SECRET_DISCORD_APP_ID
#define ATOM_SECRET_DISCORD_APP_ID      ""
#endif
/* Discord public key for Ed25519 signature verification (hex string, 64 chars) */
#ifndef ATOM_SECRET_DISCORD_PUBLIC_KEY
#define ATOM_SECRET_DISCORD_PUBLIC_KEY  ""
#endif

/* Cloudflare Worker */
/* Base URL of the Cloudflare Worker, e.g. "https://atomclaw.yourname.workers.dev" */
#ifndef ATOM_SECRET_CF_WORKER_URL
#define ATOM_SECRET_CF_WORKER_URL       ""
#endif
/* Optional auth token for the CF Worker */
#ifndef ATOM_SECRET_CF_AUTH_TOKEN
#define ATOM_SECRET_CF_AUTH_TOKEN       ""
#endif

/* Brave Search (optional) */
#ifndef ATOM_SECRET_SEARCH_KEY
#define ATOM_SECRET_SEARCH_KEY          ""
#endif

/* Proxy (optional, for LLM / search API access) */
#ifndef ATOM_SECRET_PROXY_HOST
#define ATOM_SECRET_PROXY_HOST          ""
#endif
#ifndef ATOM_SECRET_PROXY_PORT
#define ATOM_SECRET_PROXY_PORT          ""
#endif

/* ── WiFi ── */
/* 0: always use build-time secrets from atom_secrets.h, 1: allow NVS override from wifi_set */
#define ATOM_WIFI_USE_NVS              0
/* 1: use wifi_diag-like direct connect path in atom_main (bypass wifi_manager for initial connect) */
#define ATOM_WIFI_USE_DIAG_CONNECT     1
/* 1: boot like wifi_diag (do not start subsystems unless WiFi is connected) */
#define ATOM_WIFI_STRICT_DIAG_BOOT     0
#define ATOM_WIFI_MAX_RETRY             10
#define ATOM_WIFI_RETRY_BASE_MS         1000
#define ATOM_WIFI_RETRY_MAX_MS          30000

/* ── Discord HTTP server ── */
#define ATOM_DISCORD_HTTP_PORT          80
#define ATOM_DISCORD_INTERACTION_PATH   "/interactions"
/* Discord API base */
#define ATOM_DISCORD_API_BASE           "https://discord.com/api/v10"
/* Max Discord response length (Discord limit: 2000 chars) */
#define ATOM_DISCORD_MAX_RESP_LEN       1900
/* Deferred response timeout: must respond within 3 seconds */
#define ATOM_DISCORD_DEFER_TIMEOUT_MS   2500

/* ── Agent Loop ── */
#define ATOM_AGENT_STACK                (16 * 1024)
#define ATOM_AGENT_PRIO                 6
#define ATOM_AGENT_CORE                 1
#define ATOM_AGENT_MAX_TOOL_ITER        5
#define ATOM_MAX_TOOL_CALLS             4
/* Max LLM send tokens target */
#define ATOM_LLM_MAX_TOKENS             2048

/* ── LLM ── */
#define ATOM_LLM_DEFAULT_MODEL          "claude-haiku-4-5"
#define ATOM_LLM_PROVIDER_DEFAULT       "anthropic"
#define ATOM_LLM_API_URL                "https://api.anthropic.com/v1/messages"
#define ATOM_OPENAI_API_URL             "https://api.openai.com/v1/chat/completions"
#define ATOM_LLM_API_VERSION            "2023-06-01"
#define ATOM_LLM_STREAM_BUF_SIZE        (24 * 1024)

/* ── Message Bus ── */
#define ATOM_BUS_QUEUE_LEN              4
#define ATOM_OUTBOUND_STACK             (8 * 1024)
#define ATOM_OUTBOUND_PRIO              5
#define ATOM_OUTBOUND_CORE              0

/* ── SPIFFS (minimal use) ── */
#define ATOM_SPIFFS_BASE                "/spiffs"
#define ATOM_SPIFFS_CONFIG_DIR          "/spiffs/config"
#define ATOM_SPIFFS_MEMORY_DIR          "/spiffs/memory"
#define ATOM_SOUL_FILE                  "/spiffs/config/SOUL.md"
#define ATOM_USER_FILE                  "/spiffs/config/USER.md"
#define ATOM_MEMORY_FILE                "/spiffs/memory/MEMORY.md"
/* Max 4KB for MEMORY.md */
#define ATOM_MEMORY_MAX_BYTES           4096
/* System prompt buffer size */
#define ATOM_CONTEXT_BUF_SIZE           (12 * 1024)

/* ── Session Ring Buffer ── */
/* 3 exchanges = 6 messages (user + assistant per exchange) */
#define ATOM_SESSION_MAX_EXCHANGES      3
#define ATOM_SESSION_MAX_MSGS           (ATOM_SESSION_MAX_EXCHANGES * 2)
/* Max chars per stored message */
#define ATOM_SESSION_MSG_MAX_LEN        512

/* ── Cloudflare History ── */
#define ATOM_CF_SUMMARY_PATH            "/summary"
#define ATOM_CF_SAVE_PATH               "/save"
#define ATOM_CF_TIMEOUT_MS              5000
/* Max summary size */
#define ATOM_CF_SUMMARY_MAX_LEN         2048

/* ── Serial CLI ── */
#define ATOM_CLI_STACK                  (4 * 1024)
#define ATOM_CLI_PRIO                   3
#define ATOM_CLI_CORE                   0

/* ── NVS Namespaces ── */
#define ATOM_NVS_WIFI                   "wifi_config"
#define ATOM_NVS_LLM                    "llm_config"
#define ATOM_NVS_DISCORD                "discord_cfg"
#define ATOM_NVS_CF                     "cf_config"
#define ATOM_NVS_PROXY                  "proxy_config"
#define ATOM_NVS_SEARCH                 "search_config"

/* ── NVS Keys ── */
#define ATOM_NVS_KEY_SSID               "ssid"
#define ATOM_NVS_KEY_PASS               "password"
#define ATOM_NVS_KEY_API_KEY            "api_key"
#define ATOM_NVS_KEY_MODEL              "model"
#define ATOM_NVS_KEY_PROVIDER           "provider"
#define ATOM_NVS_KEY_DISCORD_APP_ID     "app_id"
#define ATOM_NVS_KEY_DISCORD_PUB_KEY    "pub_key"
#define ATOM_NVS_KEY_CF_URL             "worker_url"
#define ATOM_NVS_KEY_CF_TOKEN           "auth_token"
#define ATOM_NVS_KEY_PROXY_HOST         "host"
#define ATOM_NVS_KEY_PROXY_PORT         "port"

/* ── Channel identifier ── */
#define ATOM_CHAN_DISCORD                "discord"
