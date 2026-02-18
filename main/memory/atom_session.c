#include "atom_session.h"
#include "atom_config.h"

#include <string.h>
#include <stdlib.h>

#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "cJSON.h"

static const char *TAG = "atom_session";

/* ── Data structures ─────────────────────────────────────────────────── */

typedef struct {
    char role[12];                          /* "user" or "assistant" */
    char content[ATOM_SESSION_MSG_MAX_LEN]; /* truncated message text */
} atom_msg_t;

typedef struct {
    char      user_id[32];
    atom_msg_t msgs[ATOM_SESSION_MAX_MSGS]; /* ring buffer */
    int       head;     /* index of next write position */
    int       count;    /* number of valid entries (0..MAX_MSGS) */
    bool      in_use;
} atom_user_session_t;

static atom_user_session_t *s_sessions = NULL;   /* PSRAM array */
static SemaphoreHandle_t    s_mutex    = NULL;

/* ── Helpers ─────────────────────────────────────────────────────────── */

/* Find session slot for user_id, or allocate a new one.
 * Returns NULL if no slot available.  Caller must hold mutex. */
static atom_user_session_t *find_or_alloc(const char *user_id)
{
    atom_user_session_t *empty = NULL;
    for (int i = 0; i < ATOM_SESSION_MAX_USERS; i++) {
        if (s_sessions[i].in_use &&
            strncmp(s_sessions[i].user_id, user_id, sizeof(s_sessions[i].user_id)-1) == 0) {
            return &s_sessions[i];
        }
        if (!s_sessions[i].in_use && !empty) {
            empty = &s_sessions[i];
        }
    }
    if (empty) {
        memset(empty, 0, sizeof(*empty));
        strncpy(empty->user_id, user_id, sizeof(empty->user_id) - 1);
        empty->in_use = true;
        ESP_LOGD(TAG, "New session for user %s", user_id);
    }
    return empty;
}

/* ── Public API ──────────────────────────────────────────────────────── */

esp_err_t atom_session_init(void)
{
    s_sessions = heap_caps_calloc(ATOM_SESSION_MAX_USERS,
                                  sizeof(atom_user_session_t),
                                  MALLOC_CAP_SPIRAM);
    if (!s_sessions) {
        ESP_LOGE(TAG, "Failed to allocate session buffer in PSRAM");
        return ESP_ERR_NO_MEM;
    }

    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) {
        heap_caps_free(s_sessions);
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Session ring buffer ready (%d users × %d msgs each)",
             ATOM_SESSION_MAX_USERS, ATOM_SESSION_MAX_MSGS);
    return ESP_OK;
}

esp_err_t atom_session_append(const char *user_id,
                              const char *role,
                              const char *content)
{
    if (!user_id || !role || !content) return ESP_ERR_INVALID_ARG;

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    atom_user_session_t *sess = find_or_alloc(user_id);
    if (!sess) {
        xSemaphoreGive(s_mutex);
        ESP_LOGW(TAG, "No session slot for user %s", user_id);
        return ESP_ERR_NO_MEM;
    }

    atom_msg_t *m = &sess->msgs[sess->head];
    strncpy(m->role, role, sizeof(m->role) - 1);
    m->role[sizeof(m->role)-1] = '\0';
    strncpy(m->content, content, sizeof(m->content) - 1);
    m->content[sizeof(m->content)-1] = '\0';

    sess->head = (sess->head + 1) % ATOM_SESSION_MAX_MSGS;
    if (sess->count < ATOM_SESSION_MAX_MSGS) sess->count++;

    xSemaphoreGive(s_mutex);
    return ESP_OK;
}

esp_err_t atom_session_get_history_json(const char *user_id,
                                        char *buf, size_t buf_size,
                                        int max_msgs)
{
    buf[0] = '\0';
    if (!user_id) return ESP_ERR_INVALID_ARG;

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    atom_user_session_t *sess = NULL;
    for (int i = 0; i < ATOM_SESSION_MAX_USERS; i++) {
        if (s_sessions[i].in_use &&
            strncmp(s_sessions[i].user_id, user_id, sizeof(s_sessions[i].user_id)-1) == 0) {
            sess = &s_sessions[i];
            break;
        }
    }

    if (!sess || sess->count == 0) {
        xSemaphoreGive(s_mutex);
        strncpy(buf, "[]", buf_size);
        return ESP_OK;
    }

    cJSON *arr = cJSON_CreateArray();

    /* Clamp count to max_msgs (0 means no limit) */
    int count = sess->count;
    if (max_msgs > 0 && max_msgs < count) count = max_msgs;

    /* Traverse ring buffer in chronological order (most recent `count` msgs) */
    int start = (sess->head - count + ATOM_SESSION_MAX_MSGS) % ATOM_SESSION_MAX_MSGS;
    for (int i = 0; i < count; i++) {
        int idx = (start + i) % ATOM_SESSION_MAX_MSGS;
        cJSON *msg = cJSON_CreateObject();
        cJSON_AddStringToObject(msg, "role",    sess->msgs[idx].role);
        cJSON_AddStringToObject(msg, "content", sess->msgs[idx].content);
        cJSON_AddItemToArray(arr, msg);
    }

    xSemaphoreGive(s_mutex);

    char *json = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);

    if (json) {
        strncpy(buf, json, buf_size - 1);
        buf[buf_size - 1] = '\0';
        free(json);
    } else {
        strncpy(buf, "[]", buf_size);
    }

    return ESP_OK;
}

esp_err_t atom_session_clear(const char *user_id)
{
    if (!user_id) return ESP_ERR_INVALID_ARG;

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    for (int i = 0; i < ATOM_SESSION_MAX_USERS; i++) {
        if (s_sessions[i].in_use &&
            strncmp(s_sessions[i].user_id, user_id, sizeof(s_sessions[i].user_id)-1) == 0) {
            memset(&s_sessions[i], 0, sizeof(s_sessions[i]));
            break;
        }
    }
    xSemaphoreGive(s_mutex);
    return ESP_OK;
}
