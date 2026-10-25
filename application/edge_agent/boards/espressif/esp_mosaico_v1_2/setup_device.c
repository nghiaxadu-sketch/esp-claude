/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * VCC soft-start is adapted from espressif/esp_boards `esp_mosaico/board_runtime.c`
 * (LicenseRef-Espressif-Modified-MIT, see LICENSE.esp_boards).
 */

/* ESP-Mosaico V1.2 board hooks:
 * - lcd_power_ctrl: VCC_3V3 rail on GPIO60 (active-low), ramped on over 50 ms with LEDC
 *   so the AMOLED and module slots do not brown out the board at power-up.
 * - CO5300 panel factory: 480x480 window. The driver's built-in table targets the common
 *   466x466 module (x offset 6), so a 480x480 table is passed explicitly.
 * - CST9220 touch factory.
 */

#include <string.h>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_board_device.h"
#include "esp_err.h"
#include "esp_lcd_co5300.h"
#include "esp_lcd_touch_cst9217.h"
#include "esp_log.h"
#if defined(CONFIG_ESP_BOARD_DEV_POWER_CTRL_SUB_CUSTOM_SUPPORT) && CONFIG_ESP_BOARD_DEV_POWER_CTRL_SUB_CUSTOM_SUPPORT
#include "dev_power_ctrl.h"
#endif

static const char *TAG = "esp_mosaico_v1_2";

#define MOSAICO_VCC_GPIO     GPIO_NUM_60
#define MOSAICO_VCC_RAMP_HZ  100000
#define MOSAICO_VCC_RAMP_MS  50

/* 480x480 CO5300 sequence (same panel geometry as the Waveshare 2.16" CO5300 board).
 * MADCTL 0x00: no rotation; adjust 0x36 if the image is mirrored on real hardware. */
static const co5300_lcd_init_cmd_t s_lcd_init_cmds[] = {
    {0x11, (uint8_t[]){0x00}, 0, 600},
    {0xFE, (uint8_t[]){0x20}, 1, 0},
    {0x19, (uint8_t[]){0x10}, 1, 0},
    {0x1C, (uint8_t[]){0xA0}, 1, 0},

    {0xFE, (uint8_t[]){0x00}, 1, 0},
    {0xC4, (uint8_t[]){0x80}, 1, 0},
    {0x3A, (uint8_t[]){0x55}, 1, 0},
    {0x35, (uint8_t[]){0x00}, 1, 0},
    {0x53, (uint8_t[]){0x20}, 1, 0},
    {0x51, (uint8_t[]){0xFF}, 1, 0},
    {0x63, (uint8_t[]){0xFF}, 1, 0},
    {0x2A, (uint8_t[]){0x00, 0x00, 0x01, 0xDF}, 4, 0},
    {0x2B, (uint8_t[]){0x00, 0x00, 0x01, 0xDF}, 4, 0},
    {0x36, (uint8_t[]){0x00}, 1, 0},
    {0x29, (uint8_t[]){0x00}, 0, 600},
};

static const co5300_vendor_config_t s_lcd_vendor_config = {
    .init_cmds = s_lcd_init_cmds,
    .init_cmds_size = sizeof(s_lcd_init_cmds) / sizeof(s_lcd_init_cmds[0]),
    .flags = {
        .use_qspi_interface = 1,
    },
};

esp_err_t lcd_panel_factory_entry_t(esp_lcd_panel_io_handle_t io,
                                    const esp_lcd_panel_dev_config_t *panel_dev_config,
                                    esp_lcd_panel_handle_t *ret_panel)
{
    esp_lcd_panel_dev_config_t panel_dev_cfg = {0};
    memcpy(&panel_dev_cfg, panel_dev_config, sizeof(panel_dev_cfg));
    panel_dev_cfg.vendor_config = (void *)&s_lcd_vendor_config;

    esp_err_t ret = esp_lcd_new_panel_co5300(io, &panel_dev_cfg, ret_panel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create CO5300 panel: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t lcd_touch_factory_entry_t(esp_lcd_panel_io_handle_t io,
                                    const esp_lcd_touch_config_t *touch_dev_config,
                                    esp_lcd_touch_handle_t *ret_touch)
{
    esp_lcd_touch_config_t touch_cfg = {0};
    memcpy(&touch_cfg, touch_dev_config, sizeof(touch_cfg));
    touch_cfg.levels.interrupt = 0;

    esp_err_t ret = esp_lcd_touch_new_i2c_cst9217(io, &touch_cfg, ret_touch);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create CST9220 touch: %s", esp_err_to_name(ret));
    }
    return ret;
}

#if defined(CONFIG_ESP_BOARD_DEV_POWER_CTRL_SUB_CUSTOM_SUPPORT) && CONFIG_ESP_BOARD_DEV_POWER_CTRL_SUB_CUSTOM_SUPPORT
static bool s_vcc_on;

static void mosaico_keep_first_error(esp_err_t *ret, esp_err_t next)
{
    if (*ret == ESP_OK && next != ESP_OK) {
        *ret = next;
    }
}

/* Fade GPIO60 from high (off) to low (on) with PWM, then hand it back to plain GPIO. */
static esp_err_t mosaico_ramp_vcc(void)
{
    if (s_vcc_on) {
        return ESP_OK;
    }

    const ledc_timer_config_t timer_config = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_1,
        .freq_hz = MOSAICO_VCC_RAMP_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    const ledc_timer_config_t timer_deconfig = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_1,
        .deconfigure = true,
    };
    esp_err_t ret = ledc_timer_config(&timer_config);
    if (ret != ESP_OK) {
        return ret;
    }

    bool channel_configured = false;
    bool fade_installed_here = false;
    ret = gpio_reset_pin(MOSAICO_VCC_GPIO);
    if (ret == ESP_OK) {
        const ledc_channel_config_t channel_config = {
            .gpio_num = MOSAICO_VCC_GPIO,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = LEDC_CHANNEL_1,
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = LEDC_TIMER_1,
            .duty = 255,
            .hpoint = 0,
        };
        ret = ledc_channel_config(&channel_config);
        channel_configured = (ret == ESP_OK);
    }
    if (ret == ESP_OK) {
        ret = ledc_fade_func_install(0);
        if (ret == ESP_OK) {
            fade_installed_here = true;
        } else if (ret == ESP_ERR_INVALID_STATE) {
            ret = ESP_OK;  /* already installed by someone else */
        }
    }
    if (ret == ESP_OK) {
        ret = ledc_set_fade_with_time(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 0, MOSAICO_VCC_RAMP_MS);
    }
    if (ret == ESP_OK) {
        ret = ledc_fade_start(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, LEDC_FADE_WAIT_DONE);
    }

    if (channel_configured) {
        const ledc_channel_config_t channel_deconfig = {
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = LEDC_CHANNEL_1,
            .deconfigure = true,
        };
        mosaico_keep_first_error(&ret, ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, ret == ESP_OK ? 0 : 1));
        mosaico_keep_first_error(&ret, ledc_channel_config(&channel_deconfig));
    }
    if (fade_installed_here) {
        ledc_fade_func_uninstall();
    }
    mosaico_keep_first_error(&ret, ledc_timer_pause(LEDC_LOW_SPEED_MODE, LEDC_TIMER_1));
    mosaico_keep_first_error(&ret, ledc_timer_config(&timer_deconfig));

    /* Load the output latch before enabling GPIO output so the rail does not glitch. */
    const gpio_config_t output_config = {
        .pin_bit_mask = 1ULL << MOSAICO_VCC_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t out_ret = gpio_set_level(MOSAICO_VCC_GPIO, ret == ESP_OK ? 0 : 1);
    if (out_ret == ESP_OK) {
        out_ret = gpio_config(&output_config);
    }
    if (ret == ESP_OK && out_ret == ESP_OK) {
        s_vcc_on = true;
        ESP_LOGI(TAG, "VCC GPIO%d ramped on over %d ms", (int)MOSAICO_VCC_GPIO, MOSAICO_VCC_RAMP_MS);
    }
    return ret != ESP_OK ? ret : out_ret;
}

static int mosaico_lcd_power_init(const dev_power_ctrl_config_t *config, void **context)
{
    (void)config;
    *context = NULL;
    esp_err_t ret = mosaico_ramp_vcc();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "VCC ramp failed: %s", esp_err_to_name(ret));
        return -1;
    }
    return 0;
}

static int mosaico_lcd_power_deinit(void *context)
{
    (void)context;
    if (gpio_set_level(MOSAICO_VCC_GPIO, 1) != ESP_OK) {
        return -1;
    }
    s_vcc_on = false;
    return 0;
}

static int mosaico_lcd_power_set(void *context, const char *device_name, bool power_on)
{
    (void)context;
    (void)device_name;
    if (power_on) {
        return mosaico_ramp_vcc() == ESP_OK ? 0 : -1;
    }
    return mosaico_lcd_power_deinit(NULL);
}

static const dev_power_ctrl_custom_ops_t s_mosaico_lcd_power_ops = {
    .init = mosaico_lcd_power_init,
    .deinit = mosaico_lcd_power_deinit,
    .set_power = mosaico_lcd_power_set,
};

DEVICE_EXTRA_FUNC_REGISTER(lcd_power_ctrl, &s_mosaico_lcd_power_ops);
#endif
