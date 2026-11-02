#include "status_display.h"

#include <Arduino.h>
#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <esp_err.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <cstdio>
#include <cstring>

#include "app_config.h"
#include "audio/audio_pipeline.h"
#include "ble/ble_remote_client.h"
#include "log/app_log.h"
#include "status_display_font.h"
#include "usb/uac_microphone.h"
#include "version.h"
#include "wifi/wifi_manager.h"

namespace {

constexpr char kTag[] = "STATUS_LCD";
constexpr int kWidth = 320;
constexpr int kHeight = 240;
// Keep each SPI transaction below the USB isochronous service window. The
// previous 20-row chunks could monopolize the SPI/DMA path for ~6ms and starve
// the 2ms UAC IN endpoint.
constexpr int kChunkRows = 4;
constexpr int kChunkPixels = kWidth * kChunkRows;

constexpr gpio_num_t kPinBacklight = GPIO_NUM_45;
constexpr gpio_num_t kPinSclk = GPIO_NUM_12;
constexpr gpio_num_t kPinMosi = GPIO_NUM_11;
constexpr gpio_num_t kPinMiso = GPIO_NUM_13;
constexpr gpio_num_t kPinCs = GPIO_NUM_10;
constexpr gpio_num_t kPinDc = GPIO_NUM_46;
constexpr int kSpiClockHz = 20 * 1000 * 1000;

constexpr uint16_t Rgb565(uint32_t rgb) {
    return (uint16_t)((((rgb >> 16) & 0xF8) << 8) |
                      (((rgb >> 8) & 0xFC) << 3) |
                      ((rgb & 0xF8) >> 3));
}

constexpr uint16_t kBackground = Rgb565(0x06111C);
constexpr uint16_t kHeaderLine = Rgb565(0x15344C);
constexpr uint16_t kPanel = Rgb565(0x0B1B2A);
constexpr uint16_t kPanelBorder = Rgb565(0x24445B);
constexpr uint16_t kText = Rgb565(0xE7F4FF);
constexpr uint16_t kMuted = Rgb565(0x7692A8);
constexpr uint16_t kDim = Rgb565(0x38566C);
constexpr uint16_t kCyan = Rgb565(0x59D8FF);
constexpr uint16_t kGreen = Rgb565(0x46E49A);
constexpr uint16_t kAmber = Rgb565(0xFFC857);
constexpr uint16_t kRed = Rgb565(0xFF6B72);
constexpr uint16_t kBlue = Rgb565(0x66A7FF);

spi_device_handle_t g_spi = nullptr;
uint16_t* g_framebuffer = nullptr;
uint16_t* g_dma_buffer = nullptr;
bool g_ready = false;
uint32_t g_last_sample_ms = 0;
uint32_t g_last_frames = 0;
uint32_t g_last_uac_xfers = 0;
float g_audio_fps = 0.0f;
float g_uac_tps = 0.0f;

struct DisplayMetrics {
    ble_remote_state_t ble_state = BLE_STATE_DISCONNECTED;
    int battery_pct = -1;
    bool atvv_ready = false;
    uint32_t last_audio_age_ms = UINT32_MAX;
    uint32_t frame_size = AUDIO_DEFAULT_FRAME_BYTES;
    bool audio_active = false;
    bool audio_buffering = false;
    uint32_t frames = 0;
    uint32_t samples = 0;
    uint32_t underruns = 0;
    size_t ring_samples = 0;
    float agc_gain = 1.0f;
    bool uac_mounted = false;
    bool uac_streaming = false;
    uint32_t uac_transfers = 0;
    bool wifi_connected = false;
    String ip;
    uint32_t uptime_ms = 0;
};

uint16_t SwapBytes(uint16_t value) {
    return (uint16_t)((value >> 8) | (value << 8));
}

void PutPixel(int x, int y, uint16_t color) {
    if (x < 0 || x >= kWidth || y < 0 || y >= kHeight || g_framebuffer == nullptr) {
        return;
    }
    g_framebuffer[y * kWidth + x] = SwapBytes(color);
}

uint16_t GetPixel(int x, int y) {
    if (x < 0 || x >= kWidth || y < 0 || y >= kHeight || g_framebuffer == nullptr) {
        return kBackground;
    }
    return SwapBytes(g_framebuffer[y * kWidth + x]);
}

uint16_t Blend565(uint16_t foreground, uint16_t background, uint8_t alpha) {
    const uint32_t inverse = 255u - alpha;
    const uint32_t fr = (foreground >> 11) & 0x1F;
    const uint32_t fg = (foreground >> 5) & 0x3F;
    const uint32_t fb = foreground & 0x1F;
    const uint32_t br = (background >> 11) & 0x1F;
    const uint32_t bg = (background >> 5) & 0x3F;
    const uint32_t bb = background & 0x1F;
    const uint32_t r = (fr * alpha + br * inverse + 127) / 255;
    const uint32_t g = (fg * alpha + bg * inverse + 127) / 255;
    const uint32_t b = (fb * alpha + bb * inverse + 127) / 255;
    return (uint16_t)((r << 11) | (g << 5) | b);
}

void FillRect(int x, int y, int width, int height, uint16_t color) {
    const int left = max(0, x);
    const int top = max(0, y);
    const int right = min(kWidth, x + width);
    const int bottom = min(kHeight, y + height);
    if (left >= right || top >= bottom || g_framebuffer == nullptr) return;

    const uint16_t swapped = SwapBytes(color);
    for (int row = top; row < bottom; ++row) {
        uint16_t* start = g_framebuffer + row * kWidth;
        for (int col = left; col < right; ++col) {
            start[col] = swapped;
        }
    }
}

void StrokeRect(int x, int y, int width, int height, uint16_t color) {
    FillRect(x, y, width, 1, color);
    FillRect(x, y + height - 1, width, 1, color);
    FillRect(x, y, 1, height, color);
    FillRect(x + width - 1, y, 1, height, color);
}

uint32_t NextCodepoint(const char*& cursor) {
    const uint8_t first = (uint8_t)*cursor;
    if (first == 0) return 0;
    if (first < 0x80) {
        ++cursor;
        return first;
    }

    uint32_t codepoint = 0;
    int extra = 0;
    if ((first & 0xE0) == 0xC0) {
        codepoint = first & 0x1F;
        extra = 1;
    } else if ((first & 0xF0) == 0xE0) {
        codepoint = first & 0x0F;
        extra = 2;
    } else if ((first & 0xF8) == 0xF0) {
        codepoint = first & 0x07;
        extra = 3;
    } else {
        ++cursor;
        return '?';
    }

    ++cursor;
    for (int index = 0; index < extra; ++index) {
        const uint8_t next = (uint8_t)*cursor;
        if ((next & 0xC0) != 0x80) return '?';
        codepoint = (codepoint << 6) | (next & 0x3F);
        ++cursor;
    }
    return codepoint;
}

template <typename Glyph, size_t Count>
const Glyph* FindGlyph(const Glyph (&font)[Count], uint32_t codepoint) {
    for (size_t index = 0; index < Count; ++index) {
        if (font[index].codepoint == codepoint) return &font[index];
    }
    return nullptr;
}

template <typename Glyph, size_t Count>
int MeasureText(const Glyph (&font)[Count], const char* text) {
    if (text == nullptr) return 0;
    int width = 0;
    const char* cursor = text;
    while (*cursor != 0) {
        const uint32_t codepoint = NextCodepoint(cursor);
        const Glyph* glyph = FindGlyph(font, codepoint);
        if (glyph != nullptr) {
            width += glyph->advance;
        } else {
            const Glyph* fallback = FindGlyph(font, (uint32_t)'?');
            if (fallback != nullptr) width += fallback->advance;
        }
    }
    return width;
}

uint8_t GlyphAlpha(const uint8_t* alpha, int cell, int x, int y) {
    const int index = y * cell + x;
    const uint8_t packed = alpha[index / 2];
    return (index & 1) ? (packed & 0x0F) * 17 : (packed >> 4) * 17;
}

template <typename Glyph, size_t Count>
void DrawTextImpl(const Glyph (&font)[Count], int cell, int x, int y,
                  uint16_t color, const char* text) {
    if (text == nullptr) return;
    int cursor_x = x;
    const char* cursor = text;
    while (*cursor != 0) {
        const uint32_t codepoint = NextCodepoint(cursor);
        const Glyph* glyph = FindGlyph(font, codepoint);
        if (glyph == nullptr) glyph = FindGlyph(font, (uint32_t)'?');
        if (glyph == nullptr) continue;

        if (codepoint != ' ') {
            for (int gy = 0; gy < cell; ++gy) {
                const int py = y + gy;
                if (py < 0 || py >= kHeight) continue;
                for (int gx = 0; gx < cell; ++gx) {
                    const uint8_t alpha = GlyphAlpha(glyph->alpha, cell, gx, gy);
                    if (alpha == 0) continue;
                    const int px = cursor_x + gx;
                    if (px < 0 || px >= kWidth) continue;
                    PutPixel(px, py, Blend565(color, GetPixel(px, py), alpha));
                }
            }
        }
        cursor_x += glyph->advance;
    }
}

void DrawText12(int x, int y, uint16_t color, const char* text) {
    DrawTextImpl(status_font_12, STATUS_FONT_12_CELL, x, y, color, text);
}

void DrawText16(int x, int y, uint16_t color, const char* text) {
    DrawTextImpl(status_font_16, STATUS_FONT_16_CELL, x, y, color, text);
}

void DrawText12Right(int right, int y, uint16_t color, const char* text) {
    DrawText12(right - MeasureText(status_font_12, text), y, color, text);
}

void DrawText12Centered(int center_x, int y, uint16_t color, const char* text) {
    DrawText12(center_x - MeasureText(status_font_12, text) / 2, y, color, text);
}

esp_err_t PanelTransmit(const void* data, size_t length_bits) {
    spi_transaction_t transaction = {};
    transaction.length = length_bits;
    transaction.tx_buffer = data;
    return spi_device_transmit(g_spi, &transaction);
}

esp_err_t PanelCommand(uint8_t command, const uint8_t* parameters = nullptr,
                       size_t parameter_count = 0) {
    gpio_set_level(kPinDc, 0);
    spi_transaction_t command_transaction = {};
    command_transaction.length = 8;
    command_transaction.tx_buffer = &command;
    esp_err_t error = spi_device_transmit(g_spi, &command_transaction);
    if (error != ESP_OK || parameter_count == 0) return error;

    gpio_set_level(kPinDc, 1);
    spi_transaction_t parameter_transaction = {};
    parameter_transaction.length = parameter_count * 8;
    parameter_transaction.tx_buffer = parameters;
    return spi_device_transmit(g_spi, &parameter_transaction);
}

bool InitPanel() {
    gpio_config_t backlight = {};
    backlight.mode = GPIO_MODE_OUTPUT;
    backlight.pin_bit_mask = 1ULL << kPinBacklight;
    backlight.pull_down_en = GPIO_PULLDOWN_DISABLE;
    backlight.pull_up_en = GPIO_PULLUP_DISABLE;
    backlight.intr_type = GPIO_INTR_DISABLE;
    if (gpio_config(&backlight) != ESP_OK) return false;
    gpio_set_level(kPinBacklight, 0);

    gpio_config_t dc = {};
    dc.mode = GPIO_MODE_OUTPUT;
    dc.pin_bit_mask = 1ULL << kPinDc;
    dc.pull_down_en = GPIO_PULLDOWN_DISABLE;
    dc.pull_up_en = GPIO_PULLUP_DISABLE;
    dc.intr_type = GPIO_INTR_DISABLE;
    if (gpio_config(&dc) != ESP_OK) return false;

    spi_bus_config_t bus = {};
    bus.mosi_io_num = kPinMosi;
    bus.miso_io_num = kPinMiso;
    bus.sclk_io_num = kPinSclk;
    bus.quadwp_io_num = GPIO_NUM_NC;
    bus.quadhd_io_num = GPIO_NUM_NC;
    bus.max_transfer_sz = kChunkPixels * sizeof(uint16_t);
    esp_err_t error = spi_bus_initialize(SPI3_HOST, &bus, SPI_DMA_CH_AUTO);
    if (error != ESP_OK) {
        app_log(kTag, "SPI bus init failed: %s", esp_err_to_name(error));
        return false;
    }

    spi_device_interface_config_t device = {};
    device.clock_speed_hz = kSpiClockHz;
    device.mode = 0;
    device.spics_io_num = kPinCs;
    device.queue_size = 1;
    error = spi_bus_add_device(SPI3_HOST, &device, &g_spi);
    if (error != ESP_OK) {
        app_log(kTag, "SPI device init failed: %s", esp_err_to_name(error));
        return false;
    }

    PanelCommand(0x01);
    vTaskDelay(pdMS_TO_TICKS(120));

    const uint8_t power_b[] = {0x00, 0xC1, 0x30};
    const uint8_t power_seq[] = {0x64, 0x03, 0x12, 0x81};
    const uint8_t driver_timing_a[] = {0x85, 0x00, 0x78};
    const uint8_t power_a[] = {0x39, 0x2C, 0x00, 0x34, 0x02};
    const uint8_t pump_ratio[] = {0x20};
    const uint8_t driver_timing_b[] = {0x00, 0x00};
    const uint8_t power_control_1[] = {0x23};
    const uint8_t power_control_2[] = {0x10};
    const uint8_t vcom_control_1[] = {0x3E, 0x28};
    const uint8_t vcom_control_2[] = {0x86};
    const uint8_t madctl[] = {0x28};
    const uint8_t pixel_format[] = {0x55};
    const uint8_t frame_rate[] = {0x00, 0x18};
    const uint8_t display_function[] = {0x08, 0x82, 0x27};
    const uint8_t enable_3g[] = {0x00};
    const uint8_t gamma_disable[] = {0x01};
    const uint8_t positive_gamma[] = {
        0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1,
        0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00,
    };
    const uint8_t negative_gamma[] = {
        0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1,
        0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F,
    };

    PanelCommand(0xEF);
    PanelCommand(0xCF, power_b, sizeof(power_b));
    PanelCommand(0xED, power_seq, sizeof(power_seq));
    PanelCommand(0xE8, driver_timing_a, sizeof(driver_timing_a));
    PanelCommand(0xCB, power_a, sizeof(power_a));
    PanelCommand(0xF7, pump_ratio, sizeof(pump_ratio));
    PanelCommand(0xEA, driver_timing_b, sizeof(driver_timing_b));
    PanelCommand(0xC0, power_control_1, sizeof(power_control_1));
    PanelCommand(0xC1, power_control_2, sizeof(power_control_2));
    PanelCommand(0xC5, vcom_control_1, sizeof(vcom_control_1));
    PanelCommand(0xC7, vcom_control_2, sizeof(vcom_control_2));
    PanelCommand(0x36, madctl, sizeof(madctl));
    PanelCommand(0x3A, pixel_format, sizeof(pixel_format));
    PanelCommand(0xB1, frame_rate, sizeof(frame_rate));
    PanelCommand(0xB6, display_function, sizeof(display_function));
    PanelCommand(0xF2, enable_3g, sizeof(enable_3g));
    PanelCommand(0x26, gamma_disable, sizeof(gamma_disable));
    PanelCommand(0xE0, positive_gamma, sizeof(positive_gamma));
    PanelCommand(0xE1, negative_gamma, sizeof(negative_gamma));
    PanelCommand(0x21);
    PanelCommand(0x11);
    vTaskDelay(pdMS_TO_TICKS(120));
    PanelCommand(0x29);
    vTaskDelay(pdMS_TO_TICKS(20));

    gpio_set_level(kPinBacklight, 1);
    return true;
}

esp_err_t SetAddressWindow(int x0, int y0, int x1, int y1) {
    const uint8_t columns[] = {
        (uint8_t)(x0 >> 8), (uint8_t)x0,
        (uint8_t)(x1 >> 8), (uint8_t)x1,
    };
    const uint8_t rows[] = {
        (uint8_t)(y0 >> 8), (uint8_t)y0,
        (uint8_t)(y1 >> 8), (uint8_t)y1,
    };
    esp_err_t error = PanelCommand(0x2A, columns, sizeof(columns));
    if (error != ESP_OK) return error;
    error = PanelCommand(0x2B, rows, sizeof(rows));
    if (error != ESP_OK) return error;
    return PanelCommand(0x2C);
}

bool Present() {
    if (!g_ready) return false;
    if (ble_remote_get_state() == BLE_STATE_TALKING ||
        uac_microphone_is_streaming()) {
        return false;
    }
    if (SetAddressWindow(0, 0, kWidth - 1, kHeight - 1) != ESP_OK) return false;
    gpio_set_level(kPinDc, 1);

    for (int row = 0; row < kHeight; row += kChunkRows) {
        const int rows = min(kChunkRows, kHeight - row);
        const size_t bytes = (size_t)rows * kWidth * sizeof(uint16_t);
        memcpy(g_dma_buffer, g_framebuffer + (size_t)row * kWidth, bytes);
        if (PanelTransmit(g_dma_buffer, bytes * 8) != ESP_OK) return false;
        if (ble_remote_get_state() == BLE_STATE_TALKING ||
            uac_microphone_is_streaming()) {
            return false;
        }
        if (row + kChunkRows < kHeight) taskYIELD();
    }
    return true;
}

const char* BleStateLabel(ble_remote_state_t state) {
    switch (state) {
        case BLE_STATE_DISCONNECTED:
            return "离线";
        case BLE_STATE_SCANNING:
            return "扫描中";
        case BLE_STATE_CONNECTING:
            return "连接中";
        case BLE_STATE_CONNECTED:
            return "已连接";
        case BLE_STATE_TALKING:
            return "通话中";
        default:
            return "未知";
    }
}

uint16_t BleStateColor(ble_remote_state_t state) {
    if (state >= BLE_STATE_CONNECTED) return kGreen;
    if (state == BLE_STATE_SCANNING || state == BLE_STATE_CONNECTING) return kAmber;
    return kRed;
}

const char* OverallLabel(const DisplayMetrics& metrics, uint16_t& color) {
    if (metrics.ble_state < BLE_STATE_CONNECTED) {
        color = metrics.ble_state == BLE_STATE_DISCONNECTED ? kRed : kAmber;
        return BleStateLabel(metrics.ble_state);
    }
    if (!metrics.atvv_ready) {
        color = kRed;
        return "ATVV未就绪";
    }
    if (!metrics.uac_mounted) {
        color = kRed;
        return "USB未连接";
    }
    if (metrics.ble_state == BLE_STATE_TALKING &&
        metrics.last_audio_age_ms < 1000 && metrics.uac_streaming) {
        color = kGreen;
        return "语音传输中";
    }
    color = kBlue;
    return "待机正常";
}

void DrawFlowNode(int x, int y, int width, const char* label, uint16_t color) {
    FillRect(x, y, width, 28, kPanel);
    StrokeRect(x, y, width, 28, color);
    FillRect(x + 3, y + 3, 3, 22, color);
    DrawText12Centered(x + width / 2 + 2, y + 7, kText, label);
}

void DrawCard(int x, int y, int width, int height, const char* label,
              const char* value, const char* detail, uint16_t accent) {
    FillRect(x, y, width, height, kPanel);
    StrokeRect(x, y, width, height, kPanelBorder);
    FillRect(x, y, 3, height, accent);
    DrawText12(x + 7, y + 3, kMuted, label);
    DrawText16(x + 7, y + 18, kText, value);
    if (detail != nullptr && detail[0] != 0) {
        DrawText12Right(x + width - 6, y + 24, accent, detail);
    }
}

void FormatAge(uint32_t age_ms, char* out, size_t out_size) {
    if (age_ms == UINT32_MAX) {
        snprintf(out, out_size, "--");
    } else if (age_ms < 1000) {
        snprintf(out, out_size, "%.1fs", age_ms / 1000.0f);
    } else {
        snprintf(out, out_size, "%.1fs", age_ms / 1000.0f);
    }
}

void FormatUptime(uint32_t milliseconds, char* out, size_t out_size) {
    const uint32_t total_seconds = milliseconds / 1000;
    const uint32_t hours = total_seconds / 3600;
    const uint32_t minutes = (total_seconds / 60) % 60;
    const uint32_t seconds = total_seconds % 60;
    snprintf(out, out_size, "%02lu:%02lu:%02lu",
             (unsigned long)hours, (unsigned long)minutes,
             (unsigned long)seconds);
}

DisplayMetrics CaptureMetrics() {
    DisplayMetrics metrics;
    const uint32_t now = millis();
    metrics.ble_state = ble_remote_get_state();
    metrics.battery_pct = ble_remote_get_battery_pct();
    metrics.atvv_ready = ble_remote_is_audio_ready();
    metrics.last_audio_age_ms = ble_remote_get_last_audio_age_ms();
    metrics.frame_size = ble_remote_get_frame_size();
    metrics.audio_active = g_audio_pipeline.active;
    metrics.audio_buffering = g_audio_pipeline.buffering;
    metrics.frames = g_audio_pipeline.total_frames_decoded;
    metrics.samples = g_audio_pipeline.total_samples_pushed;
    metrics.underruns = g_audio_pipeline.underrun_count;
    metrics.ring_samples = audio_ring_buffer_available_read(&g_audio_pipeline.ring_buf);
    metrics.agc_gain = g_audio_pipeline.agc.gain;
    metrics.uac_mounted = uac_microphone_is_mounted();
    metrics.uac_streaming = uac_microphone_is_streaming();
    metrics.uac_transfers = uac_microphone_get_transfer_count();
    metrics.wifi_connected = wifi_manager_is_sta_connected();
    metrics.ip = metrics.wifi_connected ? wifi_manager_get_sta_ip() : String();
    metrics.uptime_ms = now;

    if (g_last_sample_ms != 0) {
        const uint32_t elapsed = now - g_last_sample_ms;
        if (elapsed > 0) {
            const uint32_t frame_delta =
                metrics.frames >= g_last_frames ? metrics.frames - g_last_frames : metrics.frames;
            const uint32_t uac_delta =
                metrics.uac_transfers >= g_last_uac_xfers
                    ? metrics.uac_transfers - g_last_uac_xfers
                    : metrics.uac_transfers;
            g_audio_fps = (float)frame_delta * 1000.0f / (float)elapsed;
            g_uac_tps = (float)uac_delta * 1000.0f / (float)elapsed;
        }
    }
    g_last_sample_ms = now;
    g_last_frames = metrics.frames;
    g_last_uac_xfers = metrics.uac_transfers;
    return metrics;
}

void Render() {
    const DisplayMetrics metrics = CaptureMetrics();
    uint16_t overall_color = kRed;
    const char* overall = OverallLabel(metrics, overall_color);

    FillRect(0, 0, kWidth, kHeight, kBackground);
    DrawText12(8, 4, kCyan, "G9N9N");
    DrawText12(50, 4, kText, "语音链路");
    DrawText12Right(312, 4, overall_color, overall);
    FillRect(8, 24, 304, 1, kHeaderLine);

    const uint16_t ble_color = BleStateColor(metrics.ble_state);
    const uint16_t atvv_color =
        metrics.atvv_ready ? kGreen : (metrics.ble_state >= BLE_STATE_CONNECTED ? kRed : kDim);
    const bool live_audio = metrics.ble_state == BLE_STATE_TALKING &&
                            metrics.last_audio_age_ms < 1000;
    const uint16_t audio_color = live_audio ? kGreen : (metrics.audio_active ? kAmber : kDim);
    const uint16_t uac_color = metrics.uac_streaming
                                   ? kGreen
                                   : (metrics.uac_mounted ? kAmber : kRed);

    constexpr int kNodeWidth = 68;
    constexpr int kNodeGap = 10;
    constexpr int kNodeStartX = 8;
    constexpr int kNodeY = 29;
    DrawFlowNode(kNodeStartX, kNodeY, kNodeWidth, "遥控器", ble_color);
    DrawFlowNode(kNodeStartX + (kNodeWidth + kNodeGap), kNodeY, kNodeWidth, "ATVV", atvv_color);
    DrawFlowNode(kNodeStartX + 2 * (kNodeWidth + kNodeGap), kNodeY, kNodeWidth, "音频", audio_color);
    DrawFlowNode(kNodeStartX + 3 * (kNodeWidth + kNodeGap), kNodeY, kNodeWidth, "UAC", uac_color);
    FillRect(kNodeStartX + kNodeWidth, kNodeY + 13, kNodeGap, 2, ble_color);
    FillRect(kNodeStartX + 2 * kNodeWidth + kNodeGap, kNodeY + 13, kNodeGap, 2, atvv_color);
    FillRect(kNodeStartX + 3 * kNodeWidth + 2 * kNodeGap, kNodeY + 13, kNodeGap, 2, audio_color);

    char detail[32];
    char value[24];
    constexpr int kCardX1 = 8;
    constexpr int kCardX2 = 164;
    constexpr int kCardWidth = 148;
    constexpr int kCardHeight = 44;
    constexpr int kCardY1 = 64;
    constexpr int kCardY2 = 112;
    constexpr int kCardY3 = 160;

    if (metrics.battery_pct >= 0) {
        snprintf(detail, sizeof(detail), "BAT %d%%", metrics.battery_pct);
    } else {
        snprintf(detail, sizeof(detail), "BAT --");
    }
    DrawCard(kCardX1, kCardY1, kCardWidth, kCardHeight, "遥控器 BLE",
             BleStateLabel(metrics.ble_state), detail, ble_color);

    snprintf(detail, sizeof(detail), "FS %lu", (unsigned long)metrics.frame_size);
    const char* atvv_value = metrics.atvv_ready
                                 ? (metrics.ble_state == BLE_STATE_TALKING ? "通话中" : "待机")
                                 : "未就绪";
    DrawCard(kCardX2, kCardY1, kCardWidth, kCardHeight, "ATVV 麦克风",
             atvv_value, detail, atvv_color);

    snprintf(value, sizeof(value), "%lu", (unsigned long)metrics.frames);
    char age[16];
    FormatAge(metrics.last_audio_age_ms, age, sizeof(age));
    snprintf(detail, sizeof(detail), "%.0ffps %s", g_audio_fps, age);
    DrawCard(kCardX1, kCardY2, kCardWidth, kCardHeight, "音频帧",
             value, detail, audio_color);

    const char* uac_value = metrics.uac_streaming
                                ? "传输中"
                                : (metrics.uac_mounted ? "等待主机" : "未连接");
    snprintf(detail, sizeof(detail), "TX %.0f/s", g_uac_tps);
    DrawCard(kCardX2, kCardY2, kCardWidth, kCardHeight, "UAC 麦克风",
             uac_value, detail, uac_color);

    snprintf(value, sizeof(value), "%.1fx", metrics.agc_gain);
    snprintf(detail, sizeof(detail), "B %lu", (unsigned long)metrics.ring_samples);
    DrawCard(kCardX1, kCardY3, kCardWidth, kCardHeight, "AGC / 缓冲",
             value, detail, metrics.audio_buffering ? kAmber : kCyan);

    const bool chain_fault = metrics.ble_state >= BLE_STATE_CONNECTED &&
                             (!metrics.atvv_ready || !metrics.uac_mounted);
    const char* health = chain_fault
                             ? "异常"
                             : (metrics.ble_state >= BLE_STATE_CONNECTED ? "正常" : "等待");
    const uint16_t health_color = chain_fault
                                      ? kRed
                                      : (metrics.ble_state >= BLE_STATE_CONNECTED ? kGreen : kAmber);
    snprintf(detail, sizeof(detail), "U %lu", (unsigned long)metrics.underruns);
    DrawCard(kCardX2, kCardY3, kCardWidth, kCardHeight, "运行状态",
             health, detail, health_color);

    FillRect(8, 210, 304, 1, kHeaderLine);
    char uptime[16];
    FormatUptime(metrics.uptime_ms, uptime, sizeof(uptime));
    char web[48];
    if (metrics.wifi_connected) {
        snprintf(web, sizeof(web), "WEB %s", metrics.ip.c_str());
    } else {
        snprintf(web, sizeof(web), "WEB 离线");
    }
    DrawText12(8, 218, kMuted, web);
    snprintf(detail, sizeof(detail), "UP %s", uptime);
    DrawText12Right(312, 218, kMuted, detail);

    Present();
}

void StatusTask(void*) {
    app_log(kTag, "Status display task started");
    while (true) {
        if (ble_remote_get_state() != BLE_STATE_TALKING &&
            !uac_microphone_is_streaming()) {
            Render();
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

}  // namespace

extern "C" bool status_display_init(void) {
    if (g_ready) return true;

    g_framebuffer = (uint16_t*)heap_caps_malloc(
        kWidth * kHeight * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    g_dma_buffer = (uint16_t*)heap_caps_malloc(
        kChunkPixels * sizeof(uint16_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (g_framebuffer == nullptr || g_dma_buffer == nullptr) {
        app_log(kTag, "Display buffers unavailable: framebuffer=%p dma=%p",
                g_framebuffer, g_dma_buffer);
        return false;
    }
    memset(g_framebuffer, 0, kWidth * kHeight * sizeof(uint16_t));
    memset(g_dma_buffer, 0, kChunkPixels * sizeof(uint16_t));

    if (!InitPanel()) {
        app_log(kTag, "ILI9341 initialization failed");
        return false;
    }

    g_ready = true;
    Render();
    BaseType_t created = xTaskCreatePinnedToCore(
        StatusTask, "status_lcd", 6144, nullptr, 1, nullptr, 1);
    if (created != pdPASS) {
        app_log(kTag, "Failed to create status task");
        g_ready = false;
        return false;
    }
    app_log(kTag, "ILI9341 ready: 320x240, backlight=%d", (int)kPinBacklight);
    return true;
}

extern "C" bool status_display_is_ready(void) {
    return g_ready;
}
