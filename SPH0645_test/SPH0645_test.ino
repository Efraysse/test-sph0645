/*
  SPH0645 I2S microphone test for ESP32 (Arduino IDE)

  Wiring (see ../mic-pin.md):
    SPH0645 3V      -> ESP32 3V3
    SPH0645 GND     -> ESP32 GND
    SPH0645 BCLK    -> GPIO 4
    SPH0645 LRCL/WS -> GPIO 2
    SPH0645 DOUT    -> GPIO 0
    SPH0645 SEL     -> GND   (selects LEFT channel)

  NOTE: GPIO0 is an ESP32 boot-strapping pin (must read HIGH at reset for
  normal boot, LOW = download mode). Driving it from the mic's DOUT line
  can occasionally cause boot issues. If you see the board fail to boot
  or drop into download mode, move DOUT to a plain GPIO (e.g. 32, 33, 25,
  26) and update I2S_SD_PIN below.

  Board: "ESP32 Dev Module" (or your specific ESP32 board)
  Serial Monitor: 115200 baud

  What it does: continuously reads I2S audio from the mic and prints the
  peak and RMS amplitude of each block, ~a few times per second. Talk,
  clap, or tap near the mic and watch the numbers jump.
*/

#include <driver/i2s.h>

#define I2S_SCK_PIN   4   // BCLK
#define I2S_WS_PIN    2   // LRCL / Word Select
#define I2S_SD_PIN    0   // DOUT / DATA (see boot-pin note above)

#define I2S_PORT      I2S_NUM_0
#define SAMPLE_RATE   16000
#define DMA_BUF_LEN   256
#define DMA_BUF_COUNT 4

int32_t samples[DMA_BUF_LEN];

void setupI2S() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,  // SEL tied to GND -> left channel
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = DMA_BUF_COUNT,
    .dma_buf_len = DMA_BUF_LEN,
    .use_apll = true,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK_PIN,
    .ws_io_num = I2S_WS_PIN,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD_PIN
  };

  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("SPH0645 I2S microphone test starting...");
  setupI2S();
}

void loop() {
  size_t bytes_read = 0;
  i2s_read(I2S_PORT, samples, sizeof(samples), &bytes_read, portMAX_DELAY);
  int n = bytes_read / sizeof(int32_t);
  if (n == 0) return;

  int32_t peak = 0;
  double sumSquares = 0;

  for (int i = 0; i < n; i++) {
    // SPH0645 erratum: the 18-bit sample is shifted one BCLK late, so the
    // usable data sits in the upper bits of the 32-bit word. Shifting
    // right by 14 recovers a correctly-aligned, sign-extended sample.
    int32_t s = samples[i] >> 14;
    int32_t abs_s = s < 0 ? -s : s;
    if (abs_s > peak) peak = abs_s;
    sumSquares += (double)s * (double)s;
  }

  double rms = sqrt(sumSquares / n);

  Serial.print("peak: ");
  Serial.print(peak);
  Serial.print("\trms: ");
  Serial.println(rms, 1);
}
