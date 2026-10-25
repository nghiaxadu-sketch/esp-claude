/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "edge_ota.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_app_desc.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define TAG "EDGE_OTA"

#define OTA_TASK_STACK_BYTES   8192
#define OTA_TASK_PRIORITY      5
#define OTA_URL_MAX_LEN        512
#define OTA_HTTP_TIMEOUT_MS    20000
#define OTA_HTTP_RX_BUF_BYTES  4096 /* GitHub redirects carry a long Location header */
#define OTA_HTTP_TX_BUF_BYTES  2048
#define OTA_RESTART_DELAY_MS   1500

typedef struct {
    char url[OTA_URL_MAX_LEN];
    bool check_only;
} ota_job_t;

static struct {
    SemaphoreHandle_t lock;
    edge_ota_status_t status;
    bool running;
} s_ota;

static void ota_set_status(const char *state, int progress, const char *message)
{
    xSemaphoreTake(s_ota.lock, portMAX_DELAY);
    strlcpy(s_ota.status.state, state, sizeof(s_ota.status.state));
    s_ota.status.progress = progress;
    strlcpy(s_ota.status.message, message ? message : "", sizeof(s_ota.status.message));
    xSemaphoreGive(s_ota.lock);
}

static void ota_finish_task(const char *state, int progress, const char *message)
{
    ota_set_status(state, progress, message);
    xSemaphoreTake(s_ota.lock, portMAX_DELAY);
    s_ota.running = false;
    xSemaphoreGive(s_ota.lock);
}

static bool ota_same_image(const esp_app_desc_t *remote)
{
    const esp_app_desc_t *running = esp_app_get_description();

    return memcmp(remote->app_elf_sha256, running->app_elf_sha256,
                  sizeof(running->app_elf_sha256)) == 0;
}

static void ota_task(void *arg)
{
    ota_job_t *job = (ota_job_t *)arg;
    esp_https_ota_handle_t handle = NULL;
    esp_app_desc_t remote_desc = {0};
    char message[EDGE_OTA_MESSAGE_LEN];
    esp_err_t err;

    esp_http_client_config_t http_config = {
        .url = job->url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = OTA_HTTP_TIMEOUT_MS,
        .buffer_size = OTA_HTTP_RX_BUF_BYTES,
        .buffer_size_tx = OTA_HTTP_TX_BUF_BYTES,
        .keep_alive_enable = true,
    };
    esp_https_ota_config_t ota_config = {
        .http_config = &http_config,
    };

    ota_set_status("checking", 0, "Connecting to the update server");
    err = esp_https_ota_begin(&ota_config, &handle);
    if (err != ESP_OK) {
        snprintf(message, sizeof(message), "Cannot download image: %s", esp_err_to_name(err));
        goto fail;
    }
    err = esp_https_ota_get_img_desc(handle, &remote_desc);
    if (err != ESP_OK) {
        snprintf(message, sizeof(message), "Not a valid firmware image: %s", esp_err_to_name(err));
        goto fail;
    }
    if (ota_same_image(&remote_desc)) {
        esp_https_ota_abort(handle);
        ota_finish_task("uptodate", 100, "Already running the latest firmware");
        goto out;
    }
    if (job->check_only) {
        esp_https_ota_abort(handle);
        snprintf(message, sizeof(message), "New firmware available (built %s %s)",
                 remote_desc.date, remote_desc.time);
        ota_finish_task("idle", 0, message);
        goto out;
    }

    ota_set_status("downloading", 0, "Downloading new firmware");
    while ((err = esp_https_ota_perform(handle)) == ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
        int total = esp_https_ota_get_image_size(handle);
        int read = esp_https_ota_get_image_len_read(handle);

        if (total > 0) {
            ota_set_status("downloading", (int)((int64_t)read * 100 / total), "Downloading new firmware");
        }
    }
    if (err != ESP_OK) {
        snprintf(message, sizeof(message), "Download failed: %s", esp_err_to_name(err));
        goto fail;
    }
    if (!esp_https_ota_is_complete_data_received(handle)) {
        snprintf(message, sizeof(message), "Download ended early");
        err = ESP_FAIL;
        goto fail;
    }
    err = esp_https_ota_finish(handle);
    handle = NULL;
    if (err != ESP_OK) {
        snprintf(message, sizeof(message), "Image rejected: %s", esp_err_to_name(err));
        goto fail;
    }

    ota_finish_task("done", 100, "Update installed, restarting");
    ESP_LOGW(TAG, "Update installed, restarting");
    vTaskDelay(pdMS_TO_TICKS(OTA_RESTART_DELAY_MS));
    esp_restart();

fail:
    ESP_LOGW(TAG, "%s", message);
    if (handle) {
        esp_https_ota_abort(handle);
    }
    ota_finish_task("error", 0, message);
out:
    free(job);
    vTaskDelete(NULL);
}

esp_err_t edge_ota_start(const char *url, bool check_only)
{
    ota_job_t *job = NULL;
    const char *source = (url && url[0]) ? url : CONFIG_EDGE_OTA_DEFAULT_URL;

    if (!esp_ota_get_next_update_partition(NULL)) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (!source[0] || strlen(source) >= OTA_URL_MAX_LEN) {
        return ESP_ERR_INVALID_ARG;
    }
    /* Never fetch firmware over plain HTTP: the image is not otherwise authenticated. */
    if (strncmp(source, "https://", 8) != 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_ota.lock) {
        return ESP_ERR_INVALID_STATE;
    }
    xSemaphoreTake(s_ota.lock, portMAX_DELAY);
    if (s_ota.running) {
        xSemaphoreGive(s_ota.lock);
        return ESP_ERR_INVALID_STATE;
    }
    s_ota.running = true;
    xSemaphoreGive(s_ota.lock);

    job = calloc(1, sizeof(*job));
    if (!job) {
        ota_finish_task("error", 0, "Out of memory");
        return ESP_ERR_NO_MEM;
    }
    strlcpy(job->url, source, sizeof(job->url));
    job->check_only = check_only;

    if (xTaskCreate(ota_task, "edge_ota", OTA_TASK_STACK_BYTES, job, OTA_TASK_PRIORITY, NULL) != pdPASS) {
        free(job);
        ota_finish_task("error", 0, "Cannot start update task");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void edge_ota_get_status(edge_ota_status_t *out)
{
    if (!out) {
        return;
    }
    memset(out, 0, sizeof(*out));
    out->supported = esp_ota_get_next_update_partition(NULL) != NULL;
    if (!s_ota.lock) {
        strlcpy(out->state, "idle", sizeof(out->state));
        return;
    }
    xSemaphoreTake(s_ota.lock, portMAX_DELAY);
    out->progress = s_ota.status.progress;
    strlcpy(out->state, s_ota.status.state, sizeof(out->state));
    strlcpy(out->message, s_ota.status.message, sizeof(out->message));
    xSemaphoreGive(s_ota.lock);
}

void edge_ota_init(void)
{
    if (s_ota.lock) {
        return;
    }
    s_ota.lock = xSemaphoreCreateMutex();
    strlcpy(s_ota.status.state, "idle", sizeof(s_ota.status.state));
}

void edge_ota_mark_boot_ok(void)
{
    /* Returns an error when the image was not pending verification (e.g. USB flash). */
    esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Boot confirmed, rollback cancelled");
    }
}
