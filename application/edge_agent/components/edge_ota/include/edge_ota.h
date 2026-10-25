/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EDGE_OTA_STATE_LEN   16
#define EDGE_OTA_MESSAGE_LEN 96

typedef struct {
    bool supported;                    /**< The flash layout has a second app slot. */
    char state[EDGE_OTA_STATE_LEN];    /**< idle, checking, downloading, uptodate, done, error */
    int progress;                      /**< Download progress in percent, 0-100. */
    char message[EDGE_OTA_MESSAGE_LEN];
} edge_ota_status_t;

/**
 * Start an update in a background task.
 *
 * @param url        Image URL, or NULL for CONFIG_EDGE_OTA_DEFAULT_URL.
 * @param check_only Only compare the remote image with the running one, do not flash.
 * @return ESP_ERR_NOT_SUPPORTED without a second app slot, ESP_ERR_INVALID_STATE when
 *         already running, ESP_ERR_INVALID_ARG when no URL is available.
 */
/** Call once at boot, before any other edge_ota function. */
void edge_ota_init(void);

esp_err_t edge_ota_start(const char *url, bool check_only);

void edge_ota_get_status(edge_ota_status_t *out);

/** Confirm the running image after a successful boot so the bootloader keeps it. */
void edge_ota_mark_boot_ok(void);

void register_ota_command(void);

#ifdef __cplusplus
}
#endif
