/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "system_ui_private.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "claw_paths.h"
#include "display_service.h"
#include "esp_check.h"

#define SYSTEM_UI_HOME_CLOCK_CENTER_PCT 40
#define SYSTEM_UI_HOME_WEATHER_Y_PCT 56
#define SYSTEM_UI_HOME_BARS_TOP_PCT 64
#define SYSTEM_UI_HOME_WEATHER_REFRESH_S 30
#define SYSTEM_UI_HOME_WEATHER_FILE "cache/weather/home.txt"
#define SYSTEM_UI_HOME_WEATHER_LEN 64
#define SYSTEM_UI_HOME_HINT_TEXT "Swipe left for apps  >"

typedef struct {
    int32_t pad;
    int32_t header_y;
    int32_t status_h;
    int32_t clock_y;
    int32_t clock_w;
    int32_t clock_h;
    int32_t date_y;
    int32_t weather_y;
    int32_t bars_y;
    int32_t bars_h;
    int32_t hint_y;
} system_ui_home_layout_t;

static system_ui_home_layout_t system_ui_home_layout(void)
{
    int32_t width = (int32_t)s_ui.width;
    int32_t height = (int32_t)s_ui.height;
    int32_t short_side = system_ui_short_side_from(s_ui.width, s_ui.height);
    int32_t header_y = system_ui_clamp_i32(short_side / 22, 10, 20);
    int32_t status_h = system_ui_clamp_i32(short_side / 9, 28, 42);
    int32_t clock_h = system_ui_clamp_i32(height * 30 / 100, 68, 118);
    int32_t clock_w = system_ui_clamp_i32(width - system_ui_clamp_i32(short_side / 8, 28, 64), 176, 380);
    int32_t clock_center_y = system_ui_clamp_i32(height * SYSTEM_UI_HOME_CLOCK_CENTER_PCT / 100, clock_h / 2 + 64, height - 48);
    int32_t line_h = system_ui_clamp_i32(short_side / 14, 18, 30);
    int32_t clock_y = clock_center_y - clock_h / 2;
    int32_t weather_y = system_ui_clamp_i32(height * SYSTEM_UI_HOME_WEATHER_Y_PCT / 100, clock_y + clock_h + 2, height - 42);
    int32_t hint_y = height - line_h - header_y;
    int32_t bars_y = system_ui_max_i32(weather_y + line_h + 6, height * SYSTEM_UI_HOME_BARS_TOP_PCT / 100);

    return (system_ui_home_layout_t) {
        .pad = system_ui_clamp_i32(short_side / 16, 14, 30),
        .header_y = header_y,
        .status_h = status_h,
        .clock_y = clock_y,
        .clock_w = clock_w,
        .clock_h = clock_h,
        .date_y = system_ui_max_i32(header_y + status_h, clock_y - line_h),
        .weather_y = weather_y,
        .bars_y = bars_y,
        .bars_h = system_ui_max_i32(0, hint_y - bars_y - 4),
        .hint_y = hint_y,
    };
}

/* Weather summary written by the Weather app; empty when the app has never fetched data. */
static void system_ui_home_update_weather_locked(void)
{
    char path[SYSTEM_UI_PATH_MAX];
    char text[SYSTEM_UI_HOME_WEATHER_LEN] = "";

    if (!s_ui.weather_label) {
        return;
    }
    if (claw_paths_join(CLAW_PATH_DATA, SYSTEM_UI_HOME_WEATHER_FILE, path, sizeof(path)) == ESP_OK) {
        FILE *file = fopen(path, "r");
        if (file) {
            if (!fgets(text, sizeof(text), file)) {
                text[0] = '\0';
            }
            fclose(file);
            text[strcspn(text, "\r\n")] = '\0';
        }
    }
    lv_label_set_text(s_ui.weather_label, text);
}

/* Slowly scrolling pixel skyline under the clock, one column per bar object. */
static void system_ui_home_update_bars_locked(void)
{
    static const uint8_t pattern[] = { 2, 3, 5, 4, 6, 7, 5, 3, 2, 4, 6, 8, 7, 5, 4, 3, 5, 6, 4, 2 };
    const size_t pattern_len = sizeof(pattern) / sizeof(pattern[0]);
    int32_t step = s_ui.home_bar_cell + 2;

    if (step <= 2 || s_ui.home_bar_max_h <= 0) {
        return;
    }
    int32_t max_cells = s_ui.home_bar_max_h / step;
    for (uint8_t i = 0; i < s_ui.home_bar_count; i++) {
        lv_obj_t *bar = s_ui.home_bars[i];
        if (!bar) {
            continue;
        }
        int32_t cells = pattern[(i + s_ui.home_tick / 2) % pattern_len] * max_cells / 8;
        cells = system_ui_clamp_i32(cells, 1, system_ui_max_i32(1, max_cells));
        /* Bars grow upwards from a shared baseline. */
        int32_t bar_h = cells * step - 2;
        lv_obj_set_y(bar, s_ui.home_bar_bottom - bar_h);
        lv_obj_set_height(bar, bar_h);
    }
}

static void system_ui_home_update_clock_locked(void)
{
    time_t now = time(NULL);
    struct tm tm_now = {0};
    char time_text[16];
    char date_text[32];
    char weekday_text[16];

    if (now > 0 && localtime_r(&now, &tm_now)) {
        strftime(time_text, sizeof(time_text), "%H:%M", &tm_now);
        strftime(date_text, sizeof(date_text), "%m/%d \xC2\xB7 ", &tm_now);
        strftime(weekday_text, sizeof(weekday_text), "%a", &tm_now);
    } else {
        snprintf(time_text, sizeof(time_text), "--:--");
        snprintf(date_text, sizeof(date_text), "--/-- \xC2\xB7 ");
        snprintf(weekday_text, sizeof(weekday_text), "---");
    }

    if (s_ui.time_label) {
        lv_label_set_text(s_ui.time_label, time_text);
    }
    if (s_ui.date_label) {
        lv_label_set_text(s_ui.date_label, date_text);
    }
    if (s_ui.weekday_label) {
        lv_label_set_text(s_ui.weekday_label, weekday_text);
    }
}

static void system_ui_home_clock_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    s_ui.home_tick++;
    system_ui_home_update_clock_locked();
    system_ui_home_update_bars_locked();
    if (s_ui.home_tick % SYSTEM_UI_HOME_WEATHER_REFRESH_S == 0) {
        system_ui_home_update_weather_locked();
    }
}

void system_ui_home_update_locked(void)
{
    char status[96];
    if (s_ui.sta_connected && s_ui.ap_ssid[0]) {
        snprintf(status, sizeof(status), "WIFI ON  |  AP %s", s_ui.ap_ssid);
    } else if (s_ui.sta_connected) {
        snprintf(status, sizeof(status), "WIFI ON");
    } else if (s_ui.ap_ssid[0]) {
        snprintf(status, sizeof(status), "WIFI OFF  |  AP %s", s_ui.ap_ssid);
    } else {
        snprintf(status, sizeof(status), "WIFI OFF");
    }

    if (s_ui.status_label) {
        lv_label_set_text(s_ui.status_label, status);
    }
    system_ui_home_update_clock_locked();
}

static esp_err_t system_ui_home_create_status_bar_locked(const system_ui_home_layout_t *layout)
{
    int32_t status_w = system_ui_max_i32(80, (int32_t)s_ui.width - layout->pad * 2);
    lv_obj_t *bar = lv_obj_create(s_ui.home_tile);
    ESP_RETURN_ON_FALSE(bar != NULL, ESP_ERR_NO_MEM, SYSTEM_UI_TAG, "create status bar failed");
    lv_obj_set_size(bar, (int32_t)s_ui.width - layout->pad * 2, layout->status_h);
    lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, layout->header_y - 2);
    lv_obj_set_style_bg_opa(bar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    /* Keep network state centered at the top as one compact status group. */
    s_ui.status_label = lv_label_create(bar);
    ESP_RETURN_ON_FALSE(s_ui.status_label != NULL, ESP_ERR_NO_MEM, SYSTEM_UI_TAG, "create status failed");
    lv_obj_set_size(s_ui.status_label, status_w, layout->status_h);
    lv_label_set_long_mode(s_ui.status_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_color(s_ui.status_label, system_ui_color(SYSTEM_UI_COLOR_MUTED), 0);
    lv_obj_set_style_text_align(s_ui.status_label, LV_TEXT_ALIGN_CENTER, 0);
    system_ui_apply_font(s_ui.status_label);

    return ESP_OK;
}

static esp_err_t system_ui_home_create_date_row_locked(const system_ui_home_layout_t *layout)
{
    lv_obj_t *row = lv_obj_create(s_ui.home_tile);
    ESP_RETURN_ON_FALSE(row != NULL, ESP_ERR_NO_MEM, SYSTEM_UI_TAG, "create date row failed");
    lv_obj_set_size(row, layout->clock_w, LV_SIZE_CONTENT);
    lv_obj_align(row, LV_ALIGN_TOP_MID, 0, layout->date_y);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    s_ui.date_label = lv_label_create(row);
    ESP_RETURN_ON_FALSE(s_ui.date_label != NULL, ESP_ERR_NO_MEM, SYSTEM_UI_TAG, "create date failed");
    lv_obj_set_style_text_color(s_ui.date_label, system_ui_color(SYSTEM_UI_COLOR_TEXT), 0);
    system_ui_apply_font(s_ui.date_label);

    /* The weekday is highlighted in the accent color, like a watch face. */
    s_ui.weekday_label = lv_label_create(row);
    ESP_RETURN_ON_FALSE(s_ui.weekday_label != NULL, ESP_ERR_NO_MEM, SYSTEM_UI_TAG, "create weekday failed");
    lv_obj_set_style_text_color(s_ui.weekday_label, system_ui_color(SYSTEM_UI_COLOR_ACCENT), 0);
    system_ui_apply_font(s_ui.weekday_label);
    return ESP_OK;
}

static esp_err_t system_ui_home_create_bars_locked(const system_ui_home_layout_t *layout)
{
    int32_t short_side = system_ui_short_side_from(s_ui.width, s_ui.height);
    int32_t cell = system_ui_clamp_i32(short_side / 22, 6, 22);
    int32_t step = cell + 2;
    int32_t avail_w = (int32_t)s_ui.width - layout->pad * 2;
    int32_t cols = system_ui_clamp_i32(avail_w / step, 0, SYSTEM_UI_HOME_BAR_MAX_COLS);

    s_ui.home_bar_count = 0;
    s_ui.home_bar_cell = cell;
    s_ui.home_bar_max_h = layout->bars_h;
    s_ui.home_bar_bottom = layout->bars_y + layout->bars_h;
    if (layout->bars_h < step * 2 || cols <= 0) {
        return ESP_OK;
    }
    int32_t x0 = ((int32_t)s_ui.width - cols * step + 2) / 2;
    int32_t bottom = s_ui.home_bar_bottom;
    for (int32_t i = 0; i < cols; i++) {
        lv_obj_t *bar = lv_obj_create(s_ui.home_tile);
        ESP_RETURN_ON_FALSE(bar != NULL, ESP_ERR_NO_MEM, SYSTEM_UI_TAG, "create home bar failed");
        lv_obj_set_size(bar, cell, cell);
        lv_obj_set_style_radius(bar, 2, 0);
        lv_obj_set_style_bg_color(bar, system_ui_color(SYSTEM_UI_COLOR_BAR), 0);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(bar, 0, 0);
        lv_obj_set_style_pad_all(bar, 0, 0);
        lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_flag(bar, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_pos(bar, x0 + i * step, bottom - cell);
        s_ui.home_bars[s_ui.home_bar_count++] = bar;
    }
    return ESP_OK;
}

static esp_err_t system_ui_create_home_tile_locked(void)
{
    system_ui_home_layout_t layout = system_ui_home_layout();
    lv_dir_t dir = s_ui.launcher_app_count > 0 ? LV_DIR_RIGHT : LV_DIR_NONE;

    /* The clock page is the first tile; launcher pages, when present, sit to the right. */
    s_ui.home_tile = lv_tileview_add_tile(s_ui.tileview, 0, 0, dir);
    ESP_RETURN_ON_FALSE(s_ui.home_tile != NULL, ESP_ERR_NO_MEM, SYSTEM_UI_TAG, "create home tile failed");
    lv_obj_set_style_bg_color(s_ui.home_tile, system_ui_color(SYSTEM_UI_COLOR_BG), 0);
    lv_obj_set_style_bg_opa(s_ui.home_tile, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_ui.home_tile, 0, 0);
    lv_obj_set_style_pad_all(s_ui.home_tile, 0, 0);
    lv_obj_clear_flag(s_ui.home_tile, LV_OBJ_FLAG_SCROLLABLE);
    system_ui_apply_font(s_ui.home_tile);

    ESP_RETURN_ON_ERROR(system_ui_home_create_status_bar_locked(&layout), SYSTEM_UI_TAG, "create status bar failed");

    s_ui.time_label = lv_label_create(s_ui.home_tile);
    ESP_RETURN_ON_FALSE(s_ui.time_label != NULL, ESP_ERR_NO_MEM, SYSTEM_UI_TAG, "create time failed");
    lv_obj_set_size(s_ui.time_label, layout.clock_w, layout.clock_h);
    lv_label_set_long_mode(s_ui.time_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_color(s_ui.time_label, system_ui_color(SYSTEM_UI_COLOR_TEXT), 0);
    lv_obj_set_style_text_align(s_ui.time_label, LV_TEXT_ALIGN_CENTER, 0);
    system_ui_apply_clock_font(s_ui.time_label);
    lv_obj_align(s_ui.time_label, LV_ALIGN_TOP_MID, 0, layout.clock_y);

    ESP_RETURN_ON_ERROR(system_ui_home_create_date_row_locked(&layout), SYSTEM_UI_TAG, "create date row failed");

    s_ui.weather_label = lv_label_create(s_ui.home_tile);
    ESP_RETURN_ON_FALSE(s_ui.weather_label != NULL, ESP_ERR_NO_MEM, SYSTEM_UI_TAG, "create weather failed");
    lv_obj_set_width(s_ui.weather_label, layout.clock_w);
    lv_label_set_long_mode(s_ui.weather_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_color(s_ui.weather_label, system_ui_color(SYSTEM_UI_COLOR_MUTED), 0);
    lv_obj_set_style_text_align(s_ui.weather_label, LV_TEXT_ALIGN_CENTER, 0);
    system_ui_apply_font(s_ui.weather_label);
    lv_obj_align(s_ui.weather_label, LV_ALIGN_TOP_MID, 0, layout.weather_y);
    lv_label_set_text(s_ui.weather_label, "");

    ESP_RETURN_ON_ERROR(system_ui_home_create_bars_locked(&layout), SYSTEM_UI_TAG, "create bars failed");

    if (s_ui.launcher_app_count > 0) {
        s_ui.hint_label = lv_label_create(s_ui.home_tile);
        ESP_RETURN_ON_FALSE(s_ui.hint_label != NULL, ESP_ERR_NO_MEM, SYSTEM_UI_TAG, "create hint failed");
        lv_label_set_text(s_ui.hint_label, SYSTEM_UI_HOME_HINT_TEXT);
        lv_obj_set_style_text_color(s_ui.hint_label, system_ui_color(SYSTEM_UI_COLOR_FAINT), 0);
        system_ui_apply_font(s_ui.hint_label);
        lv_obj_align(s_ui.hint_label, LV_ALIGN_TOP_MID, 0, layout.hint_y);
    }

    s_ui.home_clock_timer = lv_timer_create(system_ui_home_clock_timer_cb, 1000, NULL);
    ESP_RETURN_ON_FALSE(s_ui.home_clock_timer != NULL, ESP_ERR_NO_MEM, SYSTEM_UI_TAG, "create home clock timer failed");
    system_ui_home_update_locked();
    system_ui_home_update_weather_locked();
    system_ui_home_update_bars_locked();
    return ESP_OK;
}

esp_err_t system_ui_create_home_locked(void)
{
    ESP_RETURN_ON_ERROR(system_ui_launcher_load_locked(), SYSTEM_UI_TAG, "load launcher failed");

    s_ui.home_screen = lv_obj_create(NULL);
    ESP_RETURN_ON_FALSE(s_ui.home_screen != NULL, ESP_ERR_NO_MEM, SYSTEM_UI_TAG, "create home failed");
    lv_obj_set_style_bg_color(s_ui.home_screen, system_ui_color(SYSTEM_UI_COLOR_BG), 0);
    lv_obj_set_style_bg_opa(s_ui.home_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_ui.home_screen, 0, 0);
    lv_obj_set_style_pad_all(s_ui.home_screen, 0, 0);
    lv_obj_clear_flag(s_ui.home_screen, LV_OBJ_FLAG_SCROLLABLE);

    s_ui.tileview = lv_tileview_create(s_ui.home_screen);
    ESP_RETURN_ON_FALSE(s_ui.tileview != NULL, ESP_ERR_NO_MEM, SYSTEM_UI_TAG, "create tileview failed");
    lv_obj_set_size(s_ui.tileview, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_ui.tileview, system_ui_color(SYSTEM_UI_COLOR_BG), 0);
    lv_obj_set_style_bg_opa(s_ui.tileview, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_ui.tileview, 0, 0);
    lv_obj_set_style_pad_all(s_ui.tileview, 0, 0);
    lv_obj_set_scrollbar_mode(s_ui.tileview, LV_SCROLLBAR_MODE_OFF);

    ESP_RETURN_ON_ERROR(system_ui_create_home_tile_locked(), SYSTEM_UI_TAG, "create home tile failed");
    ESP_RETURN_ON_ERROR(system_ui_launcher_create_pages_locked(), SYSTEM_UI_TAG, "create launcher pages failed");
    lv_tileview_set_tile(s_ui.tileview, s_ui.home_tile, LV_ANIM_OFF);
    system_ui_load_screen_locked(s_ui.home_screen);
    return ESP_OK;
}

esp_err_t system_ui_show_home(void)
{
    ESP_RETURN_ON_FALSE(s_ui.started && s_ui.home_screen, ESP_ERR_INVALID_STATE,
                        SYSTEM_UI_TAG, "runtime not started");
    ESP_RETURN_ON_FALSE(!display_service_has_exclusive_session(), ESP_ERR_INVALID_STATE,
                        SYSTEM_UI_TAG, "exclusive display session active");
    ESP_RETURN_ON_ERROR(system_ui_lock(), SYSTEM_UI_TAG, "lock failed");
    if (s_ui.tileview && s_ui.home_tile) {
        lv_tileview_set_tile(s_ui.tileview, s_ui.home_tile, LV_ANIM_OFF);
    }
    system_ui_load_screen_locked(s_ui.home_screen);
    system_ui_unlock();
    return ESP_OK;
}

esp_err_t system_ui_reload_home(void)
{
    ESP_RETURN_ON_FALSE(s_ui.started, ESP_ERR_INVALID_STATE,
                        SYSTEM_UI_TAG, "runtime not started");
    ESP_RETURN_ON_FALSE(!display_service_has_exclusive_session(), ESP_ERR_INVALID_STATE,
                        SYSTEM_UI_TAG, "exclusive display session active");
    ESP_RETURN_ON_ERROR(system_ui_lock(), SYSTEM_UI_TAG, "lock failed");
    system_ui_delete_home_locked();
    esp_err_t err = system_ui_create_home_locked();
    if (err == ESP_OK) {
        display_service_set_default_screen_locked(s_ui.home_screen);
    } else {
        system_ui_delete_home_locked();
        display_service_set_default_screen_locked(NULL);
    }
    system_ui_unlock();
    return err;
}

void system_ui_delete_home_locked(void)
{
    if (s_ui.home_clock_timer) {
        lv_timer_delete(s_ui.home_clock_timer);
        s_ui.home_clock_timer = NULL;
    }
    if (s_ui.home_screen) {
        lv_obj_delete(s_ui.home_screen);
    }
    system_ui_launcher_delete_locked();
    s_ui.home_screen = NULL;
    s_ui.home_tile = NULL;
    s_ui.tileview = NULL;
    s_ui.launcher_first_tile = NULL;
    s_ui.status_label = NULL;
    s_ui.time_label = NULL;
    s_ui.date_label = NULL;
    s_ui.weekday_label = NULL;
    s_ui.weather_label = NULL;
    s_ui.hint_label = NULL;
    memset(s_ui.home_bars, 0, sizeof(s_ui.home_bars));
    s_ui.home_bar_count = 0;
}

esp_err_t system_ui_set_network_status(bool sta_connected, const char *ap_ssid)
{
    system_ui_work_event_t event = {
        .type = SYSTEM_UI_WORK_EVENT_NETWORK_STATUS,
    };

    /* Network callbacks run from the system event task, so keep this path non-blocking and let the UI event task take the LVGL lock. */
    ESP_RETURN_ON_FALSE(s_ui.started, ESP_ERR_INVALID_STATE, SYSTEM_UI_TAG, "runtime not started");
    event.generation = s_ui.generation;
    event.network_status.sta_connected = sta_connected;
    strlcpy(event.network_status.ap_ssid, ap_ssid ? ap_ssid : "", sizeof(event.network_status.ap_ssid));
    return system_ui_post_work_event(&event, pdMS_TO_TICKS(100));
}
