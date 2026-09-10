/*
  SPH0645 I2S microphone test for ESP32 (ESP-IDF, native i2s_std driver)

  Wiring (see ../mic-pin.md):
    SPH0645 3V      -> ESP32 3V3
    SPH0645 GND     -> ESP32 GND
    SPH0645 BCLK    -> GPIO 4
    SPH0645 LRCL/WS -> GPIO 2
    SPH0645 DOUT    -> GPIO 0
    SPH0645 SEL     -> GND   (selects LEFT channel)

  NOTE: GPIO0 is an ESP32 boot-strapping pin (must read HIGH at reset for
  normal boot, LOW = download mode). Driving it from the mic's DOUT line
  can occasionally cause boot issues. If the board fails to boot or drops
  into download mode, move DOUT to a plain GPIO (e.g. 32, 33, 25, 26) and
  update I2S_SD_PIN below.

  Build/flash: idf.py set-target esp32 && idf.py -p <PORT> flash monitor

  What it does: continuously reads I2S audio from the mic and logs the
  peak and RMS amplitude of each block. Talk, clap, or tap near the mic
  and watch the numbers jump.
*/

#include <math.h>
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define I2S_SCK_PIN GPIO_NUM_4
#define I2S_WS_PIN  GPIO_NUM_2
#define I2S_SD_PIN  GPIO_NUM_0

#define SAMPLE_RATE 16000
#define READ_LEN    1024  // samples per read

static const char *TAG = "sph0645";

void app_main(void)
{
    i2s_chan_handle_t rx_handle;
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_handle));

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        // Philips format, mono -> left slot only, which matches SEL tied to GND
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_SCK_PIN,
            .ws   = I2S_WS_PIN,
            .dout = I2S_GPIO_UNUSED,
            .din  = I2S_SD_PIN,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));

    static int32_t samples[READ_LEN];
    ESP_LOGI(TAG, "SPH0645 I2S microphone test starting...");

    while (1) {
        size_t bytes_read = 0;
        esp_err_t ret = i2s_channel_read(rx_handle, samples, sizeof(samples), &bytes_read, 1000);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "i2s_channel_read failed: %d", ret);
            continue;
        }
        int n = bytes_read / sizeof(int32_t);
        if (n == 0) {
            continue;
        }

        int32_t peak = 0;
        double sum_squares = 0;
        for (int i = 0; i < n; i++) {
            // SPH0645 erratum: the 18-bit sample is shifted one BCLK late, so the
            // usable data sits in the upper bits of the 32-bit word. Shifting
            // right by 14 recovers a correctly-aligned, sign-extended sample.
            int32_t s = samples[i] >> 14;
            int32_t abs_s = s < 0 ? -s : s;
            if (abs_s > peak) {
                peak = abs_s;
            }
            sum_squares += (double)s * (double)s;
        }
        double rms = sqrt(sum_squares / n);

        ESP_LOGI(TAG, "peak: %6ld  rms: %8.1f", (long)peak, rms);
    }
}
