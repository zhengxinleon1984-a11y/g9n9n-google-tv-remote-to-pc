#include "app_log.h"
#include <ArduinoJson.h>
#include <USBCDC.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#if !ARDUINO_USB_CDC_ON_BOOT
extern USBCDC USBSerial;
#endif

// CDC mirroring is off by default; enable at runtime with the CLI command "log on".
static bool s_cdc_log_enabled = false;

#define STATIC_LOG_LINES 250
#define LOG_LINE_MAX_LEN 160

static char   s_log_lines[STATIC_LOG_LINES][LOG_LINE_MAX_LEN];
static size_t s_log_head = 0;
static size_t s_log_count = 0;
static uint32_t s_log_seq = 0;
static uint32_t s_out_seq = 0;
static uint32_t s_cdc_seq = 0;
static portMUX_TYPE s_log_mux = portMUX_INITIALIZER_UNLOCKED;

void app_log_init(void) {
    s_log_head = 0;
    s_log_count = 0;
    s_log_seq = 0;
    s_out_seq = 0;
    s_cdc_seq = 0;
    memset(s_log_lines, 0, sizeof(s_log_lines));
}

void app_log(const char* tag, const char* format, ...) {
    char msg_buf[128];
    va_list args;
    va_start(args, format);
    vsnprintf(msg_buf, sizeof(msg_buf), format, args);
    va_end(args);

    uint32_t now_ms = millis();
    uint32_t sec = now_ms / 1000;
    uint32_t ms = now_ms % 1000;

    char full_line[LOG_LINE_MAX_LEN];
    snprintf(full_line, sizeof(full_line), "[%04u.%03u] [%s] %s", (unsigned int)sec, (unsigned int)ms, tag, msg_buf);

    // Store only. Never write USB/UART from ISR or TinyUSB callback context.
    taskENTER_CRITICAL(&s_log_mux);
    strncpy(s_log_lines[s_log_head], full_line, LOG_LINE_MAX_LEN - 1);
    s_log_lines[s_log_head][LOG_LINE_MAX_LEN - 1] = '\0';
    s_log_head = (s_log_head + 1) % STATIC_LOG_LINES;
    if (s_log_count < STATIC_LOG_LINES) {
        s_log_count++;
    }
    s_log_seq++;
    taskEXIT_CRITICAL(&s_log_mux);
}

static bool copy_log_line(uint32_t seq, char* out) {
    bool ok = false;
    taskENTER_CRITICAL(&s_log_mux);
    uint32_t head_seq = s_log_seq;
    if (seq < head_seq && head_seq - seq <= s_log_count) {
        size_t back = head_seq - seq;
        size_t idx = (s_log_head + STATIC_LOG_LINES - back) % STATIC_LOG_LINES;
        memcpy(out, s_log_lines[idx], LOG_LINE_MAX_LEN);
        out[LOG_LINE_MAX_LEN - 1] = '\0';
        ok = true;
    }
    taskEXIT_CRITICAL(&s_log_mux);
    return ok;
}

void app_log_poll(void) {
    char line[LOG_LINE_MAX_LEN];
    int budget = 4;

    while (s_out_seq < s_log_seq && budget-- > 0) {
        uint32_t seq = s_out_seq;
        if (!copy_log_line(seq, line)) {
            s_out_seq = (s_log_seq > s_log_count) ? (s_log_seq - s_log_count) : 0;
            continue;
        }
        if (Serial) {
            Serial.println(line);
        }
        s_out_seq = seq + 1;
    }

#if !ARDUINO_USB_CDC_ON_BOOT
    if (s_cdc_log_enabled && USBSerial) {
        budget = 8;
        while (s_cdc_seq < s_log_seq && budget-- > 0) {
            uint32_t seq = s_cdc_seq;
            if (!copy_log_line(seq, line)) {
                s_cdc_seq = (s_log_seq > s_log_count) ? (s_log_seq - s_log_count) : 0;
                continue;
            }
            USBSerial.println(line);
            s_cdc_seq = seq + 1;
        }
    }
#endif
}

String app_log_get_json(void) {
    JsonDocument doc;
    JsonArray arr = doc["logs"].to<JsonArray>();

    taskENTER_CRITICAL(&s_log_mux);
    size_t start_idx = (s_log_count < STATIC_LOG_LINES) ? 0 : s_log_head;
    for (size_t i = 0; i < s_log_count; i++) {
        size_t idx = (start_idx + i) % STATIC_LOG_LINES;
        arr.add(s_log_lines[idx]);
    }
    taskEXIT_CRITICAL(&s_log_mux);

    String out;
    serializeJson(doc, out);
    return out;
}

void app_log_clear(void) {
    taskENTER_CRITICAL(&s_log_mux);
    s_log_head = 0;
    s_log_count = 0;
    taskEXIT_CRITICAL(&s_log_mux);
}

void app_log_set_cdc_enabled(bool enabled) {
    s_cdc_log_enabled = enabled;
}

bool app_log_get_cdc_enabled(void) {
    return s_cdc_log_enabled;
}
