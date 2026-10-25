/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>

#include "argtable3/argtable3.h"
#include "edge_ota.h"
#include "esp_console.h"
#include "esp_err.h"

static struct {
    struct arg_lit *status;
    struct arg_lit *check;
    struct arg_lit *update;
    struct arg_str *url;
    struct arg_end *end;
} ota_args;

static int ota_func(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&ota_args);
    int ops = ota_args.status->count + ota_args.check->count + ota_args.update->count;

    if (nerrors != 0 || ops != 1) {
        printf("usage: ota --status | --check [--url <https-url>] | --update [--url <https-url>]\n");
        return 1;
    }
    if (ota_args.status->count) {
        edge_ota_status_t st;

        edge_ota_get_status(&st);
        printf("ota supported=%d state=%s progress=%d msg=%s\n", st.supported, st.state, st.progress, st.message);
        return 0;
    }

    const char *url = ota_args.url->count ? ota_args.url->sval[0] : NULL;
    esp_err_t err = edge_ota_start(url, ota_args.check->count > 0);

    if (err != ESP_OK) {
        printf("ota failed to start: %s\n", esp_err_to_name(err));
        return 1;
    }
    printf("ota started, follow progress with: ota --status\n");
    return 0;
}

void register_ota_command(void)
{
    ota_args.status = arg_lit0(NULL, "status", "Show update state");
    ota_args.check = arg_lit0(NULL, "check", "Only check whether a newer firmware exists");
    ota_args.update = arg_lit0(NULL, "update", "Download and install the newer firmware, then restart");
    ota_args.url = arg_str0(NULL, "url", "<https-url>", "Image URL (default: the one built into the firmware)");
    ota_args.end = arg_end(4);

    const esp_console_cmd_t cmd = {
        .command = "ota",
        .help = "Over-the-air firmware update.\n"
                "Examples:\n"
                " ota --check\n"
                " ota --update\n"
                " ota --update --url https://example.com/edge_agent.bin\n",
        .func = ota_func,
        .argtable = &ota_args,
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}
