/*
 * Host test for the Claude request/response handling of the Anthropic backend.
 * Run by .github/workflows/build-esp-claude.yml (job "host-test").
 */
#include "../src/llm/backends/claw_llm_backend_anthropic.c"
#include <assert.h>

static char *g_body; static size_t g_hdrs; static const char *g_reply;
static struct { char name[64]; char value[64]; } g_h[4];
esp_err_t claw_llm_http_post_endpoint(claw_endpoint_handle_t e, const char *p,
        const claw_llm_http_json_request_t *r, claw_llm_http_response_t *out, char **err)
{
    (void)e; (void)p; (void)err;
    free(g_body); g_body = strdup(r->body); g_hdrs = r->header_count;
    for (size_t i = 0; i < g_hdrs; i++) { strcpy(g_h[i].name, r->headers[i].name); strcpy(g_h[i].value, r->headers[i].value); }
    memset(out, 0, sizeof(*out)); out->body = strdup(g_reply);
    return ESP_OK;
}
void claw_llm_http_response_free(claw_llm_http_response_t *r) { free(r->body); r->body = NULL; }
esp_err_t claw_media_prepare_asset(const claw_media_asset_t *a, const claw_llm_model_profile_t *pf, size_t m, claw_media_prepared_t *o, char **e)
{ (void)a; (void)pf; (void)m; (void)o; (void)e; return ESP_FAIL; }
void claw_media_prepared_free(claw_media_prepared_t *p) { (void)p; }
void claw_llm_response_free(claw_llm_response_t *r) { (void)r; }

static esp_err_t run(const char *model, const char *effort, const char *reply, claw_llm_response_t *resp, char **err)
{
    claw_llm_backend_config_t cfg = { .model = model, .reasoning_effort = effort, .timeout_ms = 1000, .max_tokens = 8192 };
    claw_llm_model_profile_t prof = { .chat_path = "/messages" };
    void *ctx = NULL; char *e = NULL;
    assert(anthropic_init(&cfg, &prof, (claw_endpoint_handle_t)1, &ctx, &e) == ESP_OK);
    cJSON *msgs = cJSON_Parse("[{\"role\":\"user\",\"content\":\"hi\"},"
        "{\"role\":\"assistant\",\"content\":\"x\",\"reasoning_content\":\"old deepseek thought\"},"
        "{\"role\":\"user\",\"content\":\"again\"}]");
    claw_llm_chat_request_t req = { .system_prompt = "sys", .messages = msgs,
        .tools_json = "[{\"name\":\"t\",\"description\":\"d\",\"input_schema\":{\"type\":\"object\"}}]" };
    g_reply = reply;
    esp_err_t r = anthropic_chat(ctx, &prof, &req, resp, err);
    cJSON_Delete(msgs); anthropic_deinit(ctx);
    return r;
}

int main(void)
{
    claw_llm_response_t resp; char *err = NULL;
    const char *ok = "{\"content\":[{\"type\":\"thinking\",\"thinking\":\"\",\"signature\":\"s\"},{\"type\":\"text\",\"text\":\"hello\"}],\"stop_reason\":\"end_turn\"}";

    assert(run("claude-opus-5-5", "medium", ok, &resp, &err) == ESP_OK);
    printf("opus body: %s\nheaders=%zu %s=%s\n", g_body, g_hdrs, g_h[0].name, g_h[0].value);
    cJSON *b = cJSON_Parse(g_body);
    assert(strcmp(cJSON_GetObjectItem(cJSON_GetObjectItem(b, "output_config"), "effort")->valuestring, "medium") == 0);
    assert(cJSON_GetObjectItem(b, "cache_control"));
    assert(strcmp(cJSON_GetObjectItem(b, "fallbacks")->valuestring, "default") == 0);
    assert(!strstr(g_body, "old deepseek thought"));
    assert(g_hdrs == 2 && strcmp(g_h[1].value, "server-side-fallback-2026-07-01") == 0);
    assert(strcmp(resp.text, "hello") == 0 && strstr(resp.raw_message_json, "\"signature\":\"s\""));
    cJSON_Delete(b);

    assert(run("claude-haiku-5-5", "none", ok, &resp, &err) == ESP_OK);
    printf("haiku body: %s headers=%zu\n", g_body, g_hdrs);
    assert(!strstr(g_body, "output_config") && !strstr(g_body, "fallbacks") && strstr(g_body, "cache_control") && g_hdrs == 1);

    assert(run("MiniMax-M2", "high", ok, &resp, &err) == ESP_OK);
    printf("minimax body: %s headers=%zu\n", g_body, g_hdrs);
    assert(!strstr(g_body, "output_config") && !strstr(g_body, "cache_control") && !strstr(g_body, "fallbacks"));
    assert(strstr(g_body, "old deepseek thought"));

    err = NULL;
    assert(run("claude-opus-5-5", "high", "{\"content\":[],\"stop_reason\":\"refusal\",\"stop_details\":{\"type\":\"refusal\",\"category\":\"cyber\"}}", &resp, &err) == ESP_FAIL);
    printf("refusal error: %s\n", err);
    assert(strstr(err, "cyber"));
    puts("ALL OK");
    return 0;
}
