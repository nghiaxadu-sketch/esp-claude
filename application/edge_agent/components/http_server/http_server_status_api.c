/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "http_server_priv.h"

static esp_err_t status_handler(httpd_req_t *req)
{
    http_server_ctx_t *ctx = http_server_ctx();
    http_server_wifi_status_t status = {0};
    esp_err_t err = ctx->services.get_wifi_status(&status);
    if (err != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read Wi-Fi status");
    }

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        httpd_resp_send_500(req);
        return ESP_ERR_NO_MEM;
    }

    http_server_json_add_string(root, "boot_id", ctx->boot_id);
    cJSON_AddBoolToObject(root, "wifi_connected", status.wifi_connected);
    http_server_json_add_string(root, "ip", status.ip);
    http_server_json_add_string(root, "storage_base_path", ctx->storage_base_path);
    cJSON_AddBoolToObject(root, "ap_active", status.ap_active);
    http_server_json_add_string(root, "ap_ssid", status.ap_ssid);
    http_server_json_add_string(root, "ap_ip", status.ap_ip);
    http_server_json_add_string(root, "wifi_mode", status.wifi_mode);
    return http_server_send_json_response(req, root);
}

static esp_err_t restart_handler(httpd_req_t *req)
{
    http_server_ctx_t *ctx = http_server_ctx();
    esp_err_t err = ctx->services.restart_device ? ctx->services.restart_device() : ESP_ERR_NOT_SUPPORTED;
    if (err != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to restart device");
    }

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        httpd_resp_send_500(req);
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddBoolToObject(root, "ok", true);
    http_server_json_add_string(root, "message", "device restart scheduled");
    return http_server_send_json_response(req, root);
}

static esp_err_t ota_get_handler(httpd_req_t *req)
{
    http_server_ctx_t *ctx = http_server_ctx();
    http_server_ota_status_t status = {0};

    if (!ctx->services.ota_get_status || ctx->services.ota_get_status(&status) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_501_METHOD_NOT_IMPLEMENTED, "OTA not available");
    }
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        httpd_resp_send_500(req);
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddBoolToObject(root, "supported", status.supported);
    http_server_json_add_string(root, "state", status.state);
    cJSON_AddNumberToObject(root, "progress", status.progress);
    http_server_json_add_string(root, "message", status.message);
    return http_server_send_json_response(req, root);
}

/* Always installs the firmware URL built into the image; no URL is accepted from the client. */
static esp_err_t ota_post_handler(httpd_req_t *req)
{
    http_server_ctx_t *ctx = http_server_ctx();
    esp_err_t err = ctx->services.ota_start ? ctx->services.ota_start() : ESP_ERR_NOT_SUPPORTED;

    if (err == ESP_ERR_NOT_SUPPORTED) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "This board has no space for over-the-air updates");
    }
    if (err == ESP_ERR_INVALID_ARG) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No update address in this firmware");
    }
    if (err == ESP_ERR_INVALID_STATE) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "An update is already running");
    }
    if (err != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to start update");
    }
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        httpd_resp_send_500(req);
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddBoolToObject(root, "ok", true);
    return http_server_send_json_response(req, root);
}

esp_err_t http_server_register_status_routes(httpd_handle_t server)
{
    const httpd_uri_t handlers[] = {
        { .uri = "/api/status", .method = HTTP_GET, .handler = status_handler },
        { .uri = "/api/restart", .method = HTTP_POST, .handler = restart_handler },
        { .uri = "/api/ota", .method = HTTP_GET, .handler = ota_get_handler },
        { .uri = "/api/ota", .method = HTTP_POST, .handler = ota_post_handler },
    };

    for (size_t i = 0; i < sizeof(handlers) / sizeof(handlers[0]); i++) {
        esp_err_t err = httpd_register_uri_handler(server, &handlers[i]);
        if (err != ESP_OK) {
            return err;
        }
    }
    return ESP_OK;
}
