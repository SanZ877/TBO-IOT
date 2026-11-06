#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiUdp.h>
#include <driver/i2s.h>

// PINOUT I2S (TETAP SAMA)
#define I2S_WS   15  // BIRU
#define I2S_SD   32  // HIJAU
#define I2S_SCK  14  // KUNING
#define I2S_PORT I2S_NUM_0

// LED INDIKATOR (Built-in LED ESP32 / GPIO 2 - Biru)
#define LED_PIN  2

// KONFIGURASI WIFI & UDP BROADCAST
WiFiMulti wifiMulti;
WiFiUDP udp;

// Broadcast Address: Siaran ke semua IP di jaringan yang sama
const char* udpAddress = "255.255.255.255"; 
const int udpPort = 8888;

// Buffer UDP (512 sampel = 1024 byte per paket)
#define UDP_PACKET_SAMPLES 512
int16_t udp_payload[UDP_PACKET_SAMPLES];
int udp_payload_idx = 0;

// Timer LED Non-Blocking
unsigned long lastBlinkTime = 0;
bool ledState = LOW;

void setup() {
  Serial.begin(115200);

  // Setup PIN LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Auto-switch Multi-SSID (Wi-Fi Kosan, Kampus, & Hotspot HP)
  wifiMulti.addAP("L3", "sesipotret");
  wifiMulti.addAP("WiFi_Kampus", "password_kampus");
  wifiMulti.addAP("Redmi 10C", "ahsan999");

  Serial.print("Menghubungkan ke Wi-Fi...");
  while (wifiMulti.run() != WL_CONNECTED) {
    // Kedip cepat tiap 250ms saat proses searching awal
    if (millis() - lastBlinkTime >= 250) {
      lastBlinkTime = millis();
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState);
    }
    Serial.print(".");
    delay(10); 
  }

  digitalWrite(LED_PIN, HIGH); // Nyala terus begitu terhubung
  Serial.println("\nTerhubung ke Wi-Fi!");
  Serial.print("IP ESP32: ");
  Serial.println(WiFi.localIP());

  // Konfigurasi I2S (SAMA PERSIS)
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
  // Cek Koneksi Wi-Fi & Atur Indikator LED
  if (wifiMulti.run() != WL_CONNECTED) {
    // Jika koneksi terputus: Kedip cepat tiap 250ms
    if (millis() - lastBlinkTime >= 250) {
      lastBlinkTime = millis();
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState);
    }
    return; // Tunda proses audio jika Wi-Fi putus
  } else {
    // Jika koneksi stabil: LED Nyala Terus
    digitalWrite(LED_PIN, HIGH);
  }

  int32_t i2s_buffer[256];
  size_t bytes_read = 0;

  i2s_read(I2S_PORT, i2s_buffer, sizeof(i2s_buffer), &bytes_read, portMAX_DELAY);
  int samples_read = bytes_read / sizeof(int32_t);

  if (samples_read <= 0) return;

  // 1. Hitung titik tengah DC offset (SAMA PERSIS)
  int64_t sum = 0;
  for (int i = 0; i < samples_read; i++) {
    sum += (i2s_buffer[i] >> 10);
  }
  int32_t dc_offset = sum / samples_read;

  // 2. Olah audio & kumpulkan ke buffer UDP 1024 byte (SAMA PERSIS)
  for (int i = 0; i < samples_read; i++) {
    int32_t sample = (i2s_buffer[i] >> 10) - dc_offset;

    if (sample > 32767) sample = 32767;
    if (sample < -32768) sample = -32768;

    udp_payload[udp_payload_idx++] = (int16_t)sample;

    if (udp_payload_idx >= UDP_PACKET_SAMPLES) {
      udp.beginPacket(udpAddress, udpPort);
      udp.write((uint8_t*)udp_payload, UDP_PACKET_SAMPLES * sizeof(int16_t));
      udp.endPacket();
      udp_payload_idx = 0;
    }
  }
}