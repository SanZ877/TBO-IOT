#include <driver/i2s.h>

// PINOUT (Sesuai patokan warna kabel Anda)
#define I2S_WS   15  // BIRU
#define I2S_SD   32  // HIJAU
#define I2S_SCK  14  // KUNING
#define I2S_PORT I2S_NUM_0

void setup() {
  // Baud rate dinaikkan ke 921600 agar bandwidth data USB Serial (32 KB/s) lancar tanpa buffer overflow
  Serial.begin(921600);

  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S),
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
    .dma_buf_len = 256,
    .use_apll = false
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD
  };

  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);
}

void loop() {
  int32_t i2s_buffer[256];
  int16_t pcm16_buffer[256];
  size_t bytes_read = 0;

  i2s_read(I2S_PORT, i2s_buffer, sizeof(i2s_buffer), &bytes_read, portMAX_DELAY);
  int samples_read = bytes_read / sizeof(int32_t);

// 1. Hitung titik tengah DC offset buffer (Gunakan shift 10)
  int64_t sum = 0;
  for (int i = 0; i < samples_read; i++) {
    sum += (i2s_buffer[i] >> 10);
  }
  int32_t dc_offset = sum / samples_read;

  // 2. Ambil audio dengan resolusi tinggi (Tanpa perkalian perkasa * 14)
  for (int i = 0; i < samples_read; i++) {
    int32_t sample = (i2s_buffer[i] >> 10) - dc_offset;

    // Hard Clamping 16-bit
    if (sample > 32767) sample = 32767;
    if (sample < -32768) sample = -32768;

    pcm16_buffer[i] = (int16_t)sample;
  }

  // Kirim biner PCM via Serial High-Speed
  Serial.write((uint8_t*)pcm16_buffer, samples_read * sizeof(int16_t));
}