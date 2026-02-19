#include <stdio.h>
#include <string.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_spiffs.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "cJSON.h"

#include "atom_config.h"
#include "bus/message_bus.h"
#include "wifi/wifi_manager.h"
#include "llm/llm_proxy.h"
#include "memory/memory_store.h"
#include "memory/atom_session.h"
#include "agent/atom_context.h"
#include "cloudflare/cf_history.h"
#include "discord/discord_server.h"
#include "cli/serial_cli.h"
#include "proxy/http_proxy.h"
#include "tools/tool_registry.h"
#include "rgb/rgb.h"

static const char *TAG = "atomclaw";

#if ATOM_WIFI_USE_DIAG_CONNECT
#define DIAG_WIFI_CONNECTED_BIT  BIT0
#define DIAG_WIFI_FAIL_BIT       BIT1

static EventGroupHandle_t s_diag_wifi_event_group;
static char s_diag_ip_str[16] = "0.0.0.0";
static int s_diag_retry_count = 0;

static const char *diag_wifi_reason_to_str(wifi_err_reason_t reason)
{
    switch (reason) {
    case WIFI_REASON_AUTH_EXPIRE: return "AUTH_EXPIRE";
    case WIFI_REASON_AUTH_FAIL: return "AUTH_FAIL";
    case WIFI_REASON_ASSOC_EXPIRE: return "ASSOC_EXPIRE";
    case WIFI_REASON_ASSOC_FAIL: return "ASSOC_FAIL";
    case WIFI_REASON_HANDSHAKE_TIMEOUT: return "HANDSHAKE_TIMEOUT";
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT: return "4WAY_HANDSHAKE_TIMEOUT";
    case WIFI_REASON_NO_AP_FOUND: return "NO_AP_FOUND";
    case WIFI_REASON_BEACON_TIMEOUT: return "BEACON_TIMEOUT";
    case WIFI_REASON_CONNECTION_FAIL: return "CONNECTION_FAIL";
    default: return "UNKNOWN";
    }
}

static void diag_wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *disc = (wifi_event_sta_disconnected_t *)data;
        if (disc) {
            ESP_LOGW(TAG, "[diag-connect] Disconnected (reason=%d:%s)",
                     disc->reason, diag_wifi_reason_to_str(disc->reason));
        } else {
            ESP_LOGW(TAG, "[diag-connect] Disconnected (unknown)");
        }
        s_diag_retry_count++;
        ESP_LOGW(TAG, "[diag-connect] retry %d (immediate)", s_diag_retry_count);
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *ev = (ip_event_got_ip_t *)data;
        snprintf(s_diag_ip_str, sizeof(s_diag_ip_str), IPSTR, IP2STR(&ev->ip_info.ip));
        ESP_LOGI(TAG, "[diag-connect] Connected! IP=%s", s_diag_ip_str);
        xEventGroupSetBits(s_diag_wifi_event_group, DIAG_WIFI_CONNECTED_BIT);
    }
}

static esp_err_t wifi_connect_diag_style(uint32_t timeout_ms)
{
    if (ATOM_SECRET_WIFI_SSID[0] == '\0') {
        return ESP_ERR_NOT_FOUND;
    }

    s_diag_wifi_event_group = xEventGroupCreate();
    if (!s_diag_wifi_event_group) {
        return ESP_ERR_NO_MEM;
    }

    ESP_ERROR_CHECK(esp_netif_init());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &diag_wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &diag_wifi_event_handler, NULL, NULL));

    wifi_config_t wc = {0};
    strncpy((char *)wc.sta.ssid, ATOM_SECRET_WIFI_SSID, sizeof(wc.sta.ssid) - 1);
    strncpy((char *)wc.sta.password, ATOM_SECRET_WIFI_PASS, sizeof(wc.sta.password) - 1);
    wc.sta.threshold.authmode = WIFI_AUTH_OPEN;
    wc.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    wc.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    wc.sta.pmf_cfg.capable = true;
    wc.sta.pmf_cfg.required = false;

    ESP_LOGI(TAG, "[diag-connect] Connecting to SSID: %s (source=build, pass_len=%d)",
             wc.sta.ssid, (int)strlen((char *)wc.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
    ESP_ERROR_CHECK(esp_wifi_start());
    /* Hotspot/AP handshake stability: keep station fully awake during auth/assoc. */
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    TickType_t ticks = (timeout_ms == UINT32_MAX) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    EventBits_t bits = xEventGroupWaitBits(s_diag_wifi_event_group,
                                           DIAG_WIFI_CONNECTED_BIT | DIAG_WIFI_FAIL_BIT,
                                           pdFALSE, pdFALSE, ticks);
    if (bits & DIAG_WIFI_CONNECTED_BIT) {
        return ESP_OK;
    }
    return ESP_ERR_TIMEOUT;
}
#endif

static void *alloc_prefer_psram(size_t size, const char *name)
{
    void *p = heap_caps_calloc(1, size, MALLOC_CAP_SPIRAM);
    if (p) return p;

    p = calloc(1, size);
    if (p) {
        ESP_LOGW(TAG, "%s allocated in internal RAM (PSRAM unavailable)", name);
    }
    return p;
}

/* ── NVS init ────────────────────────────────────────────────────────── */

static esp_err_t init_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS truncated, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret;
}

/* ── SPIFFS init ─────────────────────────────────────────────────────── */

static esp_err_t init_spiffs(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path             = ATOM_SPIFFS_BASE,
        .partition_label       = NULL,
        .max_files             = 8,
        .format_if_mount_failed = true,
    };
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPIFFS mount failed: %s", esp_err_to_name(ret));
        return ret;
    }
    size_t total = 0, used = 0;
    esp_spiffs_info(NULL, &total, &used);
    ESP_LOGI(TAG, "SPIFFS: total=%dKB used=%dKB", (int)(total/1024), (int)(used/1024));
    return ESP_OK;
}

/* ── AtomClaw Agent Loop ─────────────────────────────────────────────── */

static void atom_agent_task(void *arg)
{
    ESP_LOGI(TAG, "AtomClaw agent started on core %d", xPortGetCoreID());

    /* Prefer PSRAM; fallback to internal RAM so ATOMS3 (no PSRAM) can still run. */
    char *system_prompt = alloc_prefer_psram(ATOM_CONTEXT_BUF_SIZE, "system_prompt");
    char *history_json  = alloc_prefer_psram(ATOM_LLM_STREAM_BUF_SIZE, "history_json");
    char *tool_output   = alloc_prefer_psram(8 * 1024, "tool_output");
    char *cf_summary    = alloc_prefer_psram(ATOM_CF_SUMMARY_MAX_LEN, "cf_summary");

    if (!system_prompt || !history_json || !tool_output || !cf_summary) {
        ESP_LOGE(TAG, "Agent buffer allocation failed");
        free(system_prompt);
        free(history_json);
        free(tool_output);
        free(cf_summary);
        vTaskDelete(NULL);
        return;
    }

    const char *tools_json = tool_registry_get_tools_json();

    while (1) {
        mimi_msg_t msg;
        esp_err_t err = message_bus_pop_inbound(&msg, portMAX_DELAY);
        if (err != ESP_OK) continue;

        ESP_LOGI(TAG, "AtomClaw processing from %s (user=%s)", msg.channel, msg.chat_id);

        /* 1. Check CF availability for this request */
        bool cf_ok = cf_history_is_configured()
                     && strcmp(msg.channel, ATOM_CHAN_DISCORD) == 0;

        /* 2. Fetch Cloudflare summary (CF mode only) */
        cf_summary[0] = '\0';
        cf_summary_result_t cf_res = {0};
        if (cf_ok) {
            cf_get_summary(msg.chat_id, cf_summary, ATOM_CF_SUMMARY_MAX_LEN, &cf_res);
        }

        /* 3. Build system prompt */
        atom_context_build_system(system_prompt, ATOM_CONTEXT_BUF_SIZE, cf_summary);

        /* 4. Load local ring buffer history.
         *    CF mode: all stored messages (up to ATOM_SESSION_MAX_MSGS).
         *    Local-only mode: last 2 exchanges (4 messages) to keep context short. */
        int max_msgs = cf_ok ? 0 : 4;
        atom_session_get_history_json(msg.chat_id, history_json,
                                      ATOM_LLM_STREAM_BUF_SIZE, max_msgs);

        /* 5. Build messages array */
        cJSON *messages = cJSON_Parse(history_json);
        if (!messages) messages = cJSON_CreateArray();

        cJSON *user_msg_j = cJSON_CreateObject();
        cJSON_AddStringToObject(user_msg_j, "role", "user");
        cJSON_AddStringToObject(user_msg_j, "content", msg.content);
        cJSON_AddItemToArray(messages, user_msg_j);

        /* 6. ReAct loop (max ATOM_AGENT_MAX_TOOL_ITER iterations) */
        char *final_text = NULL;
        int iteration = 0;

        while (iteration < ATOM_AGENT_MAX_TOOL_ITER) {
            llm_response_t resp;
            err = llm_chat_tools(system_prompt, messages, tools_json, &resp);

            if (err != ESP_OK) {
                ESP_LOGE(TAG, "LLM error: %s", esp_err_to_name(err));
                break;
            }

            if (!resp.tool_use) {
                if (resp.text && resp.text_len > 0) {
                    final_text = strdup(resp.text);
                }
                llm_response_free(&resp);
                break;
            }

            /* Execute tools */
            cJSON *asst_msg = cJSON_CreateObject();
            cJSON_AddStringToObject(asst_msg, "role", "assistant");
            cJSON *asst_content = cJSON_CreateArray();
            if (resp.text && resp.text_len > 0) {
                cJSON *tb = cJSON_CreateObject();
                cJSON_AddStringToObject(tb, "type", "text");
                cJSON_AddStringToObject(tb, "text", resp.text);
                cJSON_AddItemToArray(asst_content, tb);
            }
            for (int i = 0; i < resp.call_count; i++) {
                const llm_tool_call_t *call = &resp.calls[i];
                cJSON *ub = cJSON_CreateObject();
                cJSON_AddStringToObject(ub, "type", "tool_use");
                cJSON_AddStringToObject(ub, "id",   call->id);
                cJSON_AddStringToObject(ub, "name", call->name);
                cJSON *inp = cJSON_Parse(call->input);
                cJSON_AddItemToObject(ub, "input", inp ? inp : cJSON_CreateObject());
                cJSON_AddItemToArray(asst_content, ub);
            }
            cJSON_AddItemToObject(asst_msg, "content", asst_content);
            cJSON_AddItemToArray(messages, asst_msg);

            cJSON *results_content = cJSON_CreateArray();
            for (int i = 0; i < resp.call_count; i++) {
                const llm_tool_call_t *call = &resp.calls[i];
                tool_output[0] = '\0';
                tool_registry_execute(call->name, call->input, tool_output, 8*1024);
                cJSON *rb = cJSON_CreateObject();
                cJSON_AddStringToObject(rb, "type",        "tool_result");
                cJSON_AddStringToObject(rb, "tool_use_id", call->id);
                cJSON_AddStringToObject(rb, "content",     tool_output);
                cJSON_AddItemToArray(results_content, rb);
            }
            cJSON *result_msg = cJSON_CreateObject();
            cJSON_AddStringToObject(result_msg, "role", "user");
            cJSON_AddItemToObject(result_msg, "content", results_content);
            cJSON_AddItemToArray(messages, result_msg);

            llm_response_free(&resp);
            iteration++;
        }

        cJSON_Delete(messages);

        /* 7. Prepare response text */
        const char *response_text = (final_text && final_text[0])
            ? final_text
            : "Sorry, I couldn't process your request.";

        /* 8. Save to local ring buffer */
        atom_session_append(msg.chat_id, "user",      msg.content);
        atom_session_append(msg.chat_id, "assistant", response_text);

        /* 9. Push to outbound bus */
        mimi_msg_t out = {0};
        strncpy(out.channel, msg.channel, sizeof(out.channel) - 1);
        strncpy(out.chat_id, msg.chat_id, sizeof(out.chat_id) - 1);
        strncpy(out.meta,    msg.meta,    sizeof(out.meta) - 1);
        out.content = strdup(response_text);
        if (out.content) {
            if (message_bus_push_outbound(&out) != ESP_OK) {
                ESP_LOGW(TAG, "Outbound queue full, dropping response");
                free(out.content);
            }
        }

        /* 10. Async CF save (both turns) — CF mode only */
        if (cf_ok) {
            uint32_t now = (uint32_t)time(NULL);
            cf_save_async(msg.chat_id, "user",      msg.content,   now);
            cf_save_async(msg.chat_id, "assistant", response_text, now + 1);
        }

        /* 11. ESP32側で要約生成 (CF mode + needs_summarize フラグが立っている場合)
         *     Cloudflare Worker は AI を呼ばない。要約生成は ESP32 の LLM が担う。 */
        if (cf_ok && cf_res.needs_summarize) {
            ESP_LOGI(TAG, "Generating summary for user %s (history_count=%d)",
                     msg.chat_id, cf_res.history_count);

            /* 直近履歴から要約プロンプトを組み立てる */
            char *sum_history = alloc_prefer_psram(4096, "sum_history");
            if (sum_history) {
                atom_session_get_history_json(msg.chat_id, sum_history, 4096, 0);

                const char *sum_system =
                    "You are a concise summarizer. Summarize the conversation "
                    "in 3-5 sentences focusing on key facts and user preferences. "
                    "Write in third person about 'the user'. "
                    "Reply with only the summary text, no extra commentary.";

                cJSON *sum_msgs = cJSON_Parse(sum_history);
                if (!sum_msgs) sum_msgs = cJSON_CreateArray();

                llm_response_t sum_resp = {0};
                if (llm_chat_tools(sum_system, sum_msgs, NULL, &sum_resp) == ESP_OK
                    && sum_resp.text && sum_resp.text_len > 0) {
                    /* 要約を Cloudflare KV に保存 */
                    cf_update_summary(msg.chat_id, sum_resp.text);
                }
                llm_response_free(&sum_resp);
                cJSON_Delete(sum_msgs);
                heap_caps_free(sum_history);
            }
        }

        free(final_text);
        free(msg.content);

        ESP_LOGI(TAG, "Free PSRAM: %d bytes",
                 (int)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    }
}

/* ── Outbound dispatch: routes response back to Discord ──────────────── */

static void outbound_dispatch_task(void *arg)
{
    ESP_LOGI(TAG, "Outbound dispatch started");

    while (1) {
        mimi_msg_t msg;
        if (message_bus_pop_outbound(&msg, portMAX_DELAY) != ESP_OK) continue;

        ESP_LOGI(TAG, "Dispatch → %s:%s", msg.channel, msg.chat_id);

        if (strcmp(msg.channel, ATOM_CHAN_DISCORD) == 0) {
            /* meta holds the Discord interaction token */
            discord_follow_up(msg.meta, msg.content);
        } else {
            /* CLI or other future channels */
            ESP_LOGI(TAG, "[%s] %s", msg.channel, msg.content);
        }

        free(msg.content);
    }
}

/* ── app_main ────────────────────────────────────────────────────────── */

void app_main(void)
{
    esp_log_level_set("esp-x509-crt-bundle", ESP_LOG_WARN);

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  AtomClaw - ESP32-S3 8MB AI Agent");
    ESP_LOGI(TAG, "  Discord + Cloudflare Hybrid");
    ESP_LOGI(TAG, "========================================");

    ESP_LOGI(TAG, "Internal heap: %d bytes",
             (int)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    ESP_LOGI(TAG, "PSRAM:         %d bytes",
             (int)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    /* Core init */
    ESP_ERROR_CHECK(init_nvs());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* WiFi first: keep startup path as close as possible to wifi_diag. */
    esp_err_t wifi_err = ESP_OK;
    bool wifi_connected = false;
    const char *wifi_ip = "0.0.0.0";
#if ATOM_WIFI_USE_DIAG_CONNECT
    ESP_LOGW(TAG, "ATOM_WIFI_USE_DIAG_CONNECT=1: using direct wifi_diag-style connect path");
    wifi_err = wifi_connect_diag_style(30000);
    if (wifi_err == ESP_OK) {
        wifi_connected = true;
        wifi_ip = s_diag_ip_str;
        ESP_LOGI(TAG, "WiFi connected: %s", wifi_ip);
    } else if (wifi_err == ESP_ERR_NOT_FOUND) {
        ESP_LOGW(TAG, "No WiFi credentials. Set ATOM_SECRET_WIFI_SSID in atom_secrets.h");
    } else {
        ESP_LOGW(TAG, "WiFi timeout/failure in diag-connect path");
    }
#else
    ESP_ERROR_CHECK(wifi_manager_init());
    wifi_err = wifi_manager_start();
    if (wifi_err == ESP_OK) {
        ESP_LOGI(TAG, "Waiting for WiFi...");
        if (wifi_manager_wait_connected(30000) == ESP_OK) {
            wifi_connected = true;
            wifi_ip = wifi_manager_get_ip();
            ESP_LOGI(TAG, "WiFi connected: %s", wifi_ip);
        } else {
            ESP_LOGW(TAG, "WiFi timeout. Check credentials in atom_secrets.h");
        }
    } else {
        ESP_LOGW(TAG, "No WiFi credentials. Set ATOM_SECRET_WIFI_SSID in atom_secrets.h");
    }
#endif

#if ATOM_WIFI_STRICT_DIAG_BOOT
    if (!wifi_connected) {
        ESP_LOGE(TAG, "STRICT_DIAG_BOOT=1: WiFi not connected, skipping subsystem startup");
        ESP_LOGE(TAG, "This mode mirrors wifi_diag-style boot for root-cause isolation");
        return;
    }
#endif

    /* Visual/status + filesystem init after WiFi handshake attempt. */
    ESP_ERROR_CHECK(rgb_init());
    rgb_set(255, 0, 0);
    ESP_ERROR_CHECK(init_spiffs());

    /* Subsystems */
    ESP_ERROR_CHECK(message_bus_init());
    ESP_ERROR_CHECK(memory_store_init());
    ESP_ERROR_CHECK(atom_session_init());
    ESP_ERROR_CHECK(http_proxy_init());
    ESP_ERROR_CHECK(llm_proxy_init());
    ESP_ERROR_CHECK(tool_registry_init());
    ESP_ERROR_CHECK(cf_history_init());
    ESP_LOGI(TAG, "CF history: %s",
             cf_history_is_configured()
             ? "enabled (cloud history + summary)"
             : "disabled (local-only, last 2 exchanges)");
    ESP_ERROR_CHECK(discord_server_init());
    ESP_ERROR_CHECK(serial_cli_init());

    if (wifi_connected) {
        /* RGB: green when ready */
        rgb_set(0, 255, 0);

        /* Start Discord HTTP server */
        ESP_ERROR_CHECK(discord_server_start());

        /* Start agent loop */
        BaseType_t agent_ok = xTaskCreatePinnedToCore(atom_agent_task, "atom_agent",
                                                       ATOM_AGENT_STACK, NULL,
                                                       ATOM_AGENT_PRIO, NULL, ATOM_AGENT_CORE);

        /* Start outbound dispatcher */
        BaseType_t out_ok = xTaskCreatePinnedToCore(outbound_dispatch_task, "outbound",
                                                     ATOM_OUTBOUND_STACK, NULL,
                                                     ATOM_OUTBOUND_PRIO, NULL, ATOM_OUTBOUND_CORE);
        if (agent_ok != pdPASS || out_ok != pdPASS) {
            ESP_LOGE(TAG, "Failed to create tasks: agent=%d outbound=%d",
                     (int)agent_ok, (int)out_ok);
            rgb_set(255, 0, 0);
            return;
        }

        ESP_LOGI(TAG, "AtomClaw ready! Discord interaction endpoint: "
                      "http://%s%s",
                 wifi_ip, ATOM_DISCORD_INTERACTION_PATH);
    } else if (wifi_err == ESP_OK) {
        rgb_set(255, 128, 0);  /* orange: WiFi timeout */
    } else {
        rgb_set(255, 0, 0);    /* red: no credentials */
    }

    ESP_LOGI(TAG, "CLI ready. Type 'help' for commands.");
}
