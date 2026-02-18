/**
 * AtomClaw - Cloudflare Worker (Storage Only)
 *
 * このWorkerはAI処理を一切行わない。
 * 要約生成はESP32側（llm_proxy）が担う。
 * Workerは会話履歴のKVストレージに徹する。
 *
 * KV namespace binding: ATOMCLAW_HISTORY
 * KV key: user:{discord_user_id}
 * KV value: { summary: string, history: Message[], history_count: number }
 *
 * Environment variables:
 *   AUTH_TOKEN       - Optional bearer token to restrict access from ESP32
 *   SUMMARIZE_EVERY  - How many new messages before ESP32 is asked to summarize
 *                      (Worker just sets a flag; ESP32 decides what to do)
 *
 * Routes:
 *   GET  /summary?user_id=...   → { summary, needs_summarize, history_count }
 *   POST /save                  → { ok, history_count, needs_summarize }
 *   POST /update_summary        → { ok }   (ESP32 pushes its own summary)
 *   GET  /history?user_id=...   → { history: [...] }  (for debug/admin)
 *   GET  /health                → { ok }
 *
 * Deploy:
 *   wrangler deploy
 */

const DEFAULT_SUMMARIZE_EVERY = 20;
const MAX_HISTORY = 100;

// ── Auth ──────────────────────────────────────────────────────────────────

function checkAuth(request, env) {
  const token = env.AUTH_TOKEN;
  if (!token) return true;
  return (request.headers.get("Authorization") ?? "") === `Bearer ${token}`;
}

// ── KV ────────────────────────────────────────────────────────────────────

async function getUserData(env, userId) {
  const raw = await env.ATOMCLAW_HISTORY.get(`user:${userId}`);
  if (!raw) return { summary: "", history: [], history_count: 0 };
  try { return JSON.parse(raw); }
  catch { return { summary: "", history: [], history_count: 0 }; }
}

async function putUserData(env, userId, data) {
  await env.ATOMCLAW_HISTORY.put(
    `user:${userId}`,
    JSON.stringify(data),
    { expirationTtl: 60 * 60 * 24 * 90 }  // 90日TTL
  );
}

// ── Handlers ──────────────────────────────────────────────────────────────

/**
 * GET /summary?user_id=...
 *
 * ESP32はここから要約を取得してコンテキストに組み込む。
 * needs_summarize=true の場合、ESP32は次の応答後に /update_summary を呼ぶ。
 */
async function handleSummary(request, env) {
  const userId = new URL(request.url).searchParams.get("user_id");
  if (!userId) return Response.json({ error: "Missing user_id" }, { status: 400 });

  const data  = await getUserData(env, userId);
  const every = parseInt(env.SUMMARIZE_EVERY ?? DEFAULT_SUMMARIZE_EVERY);

  // needs_summarize: Worker が ESP32 に「そろそろ要約して」と伝えるフラグ
  const needs_summarize =
    data.history_count > 0 && data.history_count % every === 0 && !data.last_summarized_at;

  return Response.json({
    summary:          data.summary ?? "",
    needs_summarize:  needs_summarize,
    history_count:    data.history_count ?? 0,
  });
}

/**
 * POST /save
 * Body: { user_id, role, content, timestamp }
 *
 * 会話の1ターンを保存する。AIは使わない。
 * レスポンスに needs_summarize フラグを含める。
 */
async function handleSave(request, env) {
  let body;
  try { body = await request.json(); }
  catch { return Response.json({ error: "Invalid JSON" }, { status: 400 }); }

  const { user_id, role, content, timestamp } = body;
  if (!user_id || !role || !content) {
    return Response.json({ error: "Missing fields" }, { status: 400 });
  }

  const data = await getUserData(env, user_id);

  data.history.push({
    role,
    content: String(content).slice(0, 2000),
    ts: timestamp ?? Math.floor(Date.now() / 1000),
  });

  data.history_count = (data.history_count ?? 0) + 1;

  // 古い履歴をローテーション
  if (data.history.length > MAX_HISTORY) {
    data.history = data.history.slice(-MAX_HISTORY);
    // 大幅に削った後は要約済みフラグをリセット
    delete data.last_summarized_at;
  }

  await putUserData(env, user_id, data);

  const every = parseInt(env.SUMMARIZE_EVERY ?? DEFAULT_SUMMARIZE_EVERY);
  const needs_summarize = data.history_count % every === 0;

  return Response.json({
    ok:             true,
    history_count:  data.history_count,
    needs_summarize,  // ESP32 はこれを見て llm_proxy で要約を生成し /update_summary を叩く
  });
}

/**
 * POST /update_summary
 * Body: { user_id, summary }
 *
 * ESP32が生成した要約をKVに保存する。
 * AIはESP32側で動く—Workerは保存するだけ。
 */
async function handleUpdateSummary(request, env) {
  let body;
  try { body = await request.json(); }
  catch { return Response.json({ error: "Invalid JSON" }, { status: 400 }); }

  const { user_id, summary } = body;
  if (!user_id || !summary) {
    return Response.json({ error: "Missing fields" }, { status: 400 });
  }

  const data = await getUserData(env, user_id);
  data.summary = String(summary).slice(0, 3000);
  data.last_summarized_at = Math.floor(Date.now() / 1000);

  await putUserData(env, user_id, data);
  return Response.json({ ok: true });
}

/**
 * GET /history?user_id=...
 * デバッグ/管理用。全履歴を返す。
 */
async function handleHistory(request, env) {
  const userId = new URL(request.url).searchParams.get("user_id");
  if (!userId) return Response.json({ error: "Missing user_id" }, { status: 400 });

  const data = await getUserData(env, userId);
  return Response.json({ history: data.history, history_count: data.history_count });
}

// ── Router ────────────────────────────────────────────────────────────────

export default {
  async fetch(request, env) {
    const url = new URL(request.url);

    if (request.method === "OPTIONS") {
      return new Response(null, {
        headers: {
          "Access-Control-Allow-Origin":  "*",
          "Access-Control-Allow-Methods": "GET, POST, OPTIONS",
          "Access-Control-Allow-Headers": "Content-Type, Authorization",
        },
      });
    }

    if (url.pathname === "/health") {
      return Response.json({ ok: true, ts: Date.now() });
    }

    if (!checkAuth(request, env)) {
      return Response.json({ error: "Unauthorized" }, { status: 401 });
    }

    if (url.pathname === "/summary"        && request.method === "GET")  return handleSummary(request, env);
    if (url.pathname === "/save"           && request.method === "POST") return handleSave(request, env);
    if (url.pathname === "/update_summary" && request.method === "POST") return handleUpdateSummary(request, env);
    if (url.pathname === "/history"        && request.method === "GET")  return handleHistory(request, env);

    return Response.json({ error: "Not found" }, { status: 404 });
  },
};
