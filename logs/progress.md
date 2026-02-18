# AtomClaw 実装進捗ログ

## セッション: 2026-02-17 続き (CF オプション化)

---

## 完了済みタスク（全件）

### partitions_atomclaw.csv
- 8MB Flash / OTAなし構成
- factory: 0x20000 (5MB), spiffs: 0x520000 (約2.6MB)

### sdkconfig.defaults.atomclaw
- 8MB Flash QIO, PSRAM 8MB Octal
- PSA Crypto有効 (Ed25519用: CONFIG_MBEDTLS_PSA_CRYPTO_C=y)
- OTA無効, coredump無効, mbedTLS バッファ削減

### main/atom_config.h
- AtomClaw全定数 (Discord, Cloudflare, session, LLM)
- LLMトークン上限 2048, セッション3往復(6メッセージ)

### main/discord/discord_server.h/.c
- HTTP server (port 80) POST /interactions
- Ed25519署名検証: PSA Crypto API (psa_verify_message)
- PING (type=1) → {"type":1} 即座返却
- APPLICATION_COMMAND → {"type":5} (deferred) 即座返却, inbound bus push
- discord_follow_up(): PATCH /webhooks/{app_id}/{token}/messages/@original

### main/cloudflare/cf_history.h/.c
- cf_get_summary(): GET /summary?user_id=...
- cf_save_async(): FreeRTOSタスクで非同期POST /save
- Bearer認証対応, エラー時サイレント無視

### main/memory/atom_session.h/.c
- PSRAMに 8ユーザー × 6メッセージ のリングバッファ
- FreeRTOS mutex で thread-safe

### main/agent/atom_context.h/.c
- atom_context_build_system(): SOUL.md + USER.md + MEMORY.md + CF summary
- atom_context_build_messages(): history + 今回の入力

### main/atom_main.c
- AtomClaw専用エントリーポイント
- atom_agent_task: CF summary → context → ReAct loop → outbound
- outbound_dispatch: discord_follow_up() → CF async save

### bus/message_bus.h 拡張
- meta[128] フィールド追加 (Discord interaction_token 用, 後方互換)

### main/CMakeLists.txt + Kconfig.projbuild
- CONFIG_DEVICE_ATOMCLAW/MIMICLAW で条件分岐コンパイル

### platformio.ini
- [env:atomclaw]: 8MB, partitions_atomclaw.csv
- [env:mimiclaw]: 16MB, partitions.csv

### cloudflare-worker/worker.js + wrangler.toml
- GET /summary, POST /save, POST /update_summary, Bearer認証
- AI処理なし (純粋なKVストレージ)
- needs_summarize フラグ: Worker→ESP32に要約生成を依頼

### main/atom_secrets.h.template
### .gitignore 更新

### CF オプション化 (2026-02-17 追加)

**変更ファイル:**
- `main/cloudflare/cf_history.h/.c`: `cf_history_is_configured()` 追加
- `main/memory/atom_session.h/.c`: `atom_session_get_history_json()` に `max_msgs` パラメータ追加
- `main/atom_main.c`: CF有無で動作を分岐

**動作:**
- CFあり (ATOM_SECRET_CF_WORKER_URL 設定済み):
  - 起動ログ: "CF history: enabled (cloud history + summary)"
  - クラウド要約取得 → ローカルリングバッファ全件 → LLM呼び出し
  - CF非同期保存 + needs_summarize 時にESP32側で要約生成・push
- CFなし (ATOM_SECRET_CF_WORKER_URL が空文字):
  - 起動ログ: "CF history: disabled (local-only, last 2 exchanges)"
  - CF呼び出し一切なし
  - ローカルリングバッファの直近4メッセージ(2往復)のみ使用
  - atom_secrets.h に CF URLなしで即試用可能

---

## ビルド手順 (PlatformIO)

  pip install platformio
  pio run -e atomclaw           # ビルド
  pio run -e atomclaw -t upload # フラッシュ書き込み
  pio device monitor -e atomclaw

