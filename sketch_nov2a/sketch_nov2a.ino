#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiUdp.h>
#include <driver/i2s.h>

// PINOUT I2S (INMP441)
#define I2S_WS   15  // BIRU
#define I2S_SD   32  // HIJAU
#define I2S_SCK  14  // KUNING
#define I2S_PORT I2S_NUM_0

// PIN LED & BUZZER
#define LED_PIN    2
#define BUZZER_PIN 25 // GPIO 25

// KONFIGURASI WIFI & UDP
WiFiMulti wifiMulti;
WiFiUDP udpTx; // Mic ke Laptop (Port 8888)
WiFiUDP udpRx; // Audio Laptop ke Buzzer (Port 8889)

const char* udpAddress = "255.255.255.255"; 
const int udpTxPort = 8888;
const int udpRxPort = 8889;

// Buffer UDP TX (Mic)
#define UDP_PACKET_SAMPLES 512
int16_t udp_payload[UDP_PACKET_SAMPLES];
int udp_payload_idx = 0;

// Buffer UDP RX (Audio Speaker)
uint8_t rx_buffer[1024];

// Timer LED Non-Blocking
unsigned long lastBlinkTime = 0;
bool ledState = LOW;

void setup() {
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  pinMode(BUZZER_PIN, OUTPUT);

  // --- TES HARDWARE: BIP 3 KALI SAAT NYALA ---
  for (int i = 0; i < 3; i++) {
    tone(BUZZER_PIN, 2000, 150); // Bip 2kHz selama 150ms
    delay(250);
  }
  noTone(BUZZER_PIN);
  pinMode(BUZZER_PIN, OUTPUT); // Reset kembali ke Digital Output
  // -------------------------------------------

  // Auto-switch Multi-SSID
  wifiMulti.addAP("L3", "sesipotret");
  wifiMulti.addAP("WiFi_Kampus", "password_kampus");
  wifiMulti.addAP("Redmi 10C", "ahsan999");

  Serial.print("Menghubungkan ke Wi-Fi...");
  while (wifiMulti.run() != WL_CONNECTED) {
    if (millis() - lastBlinkTime >= 250) {
      lastBlinkTime = millis();
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState);
    }
    Serial.print(".");
    delay(10); 
  }

  digitalWrite(LED_PIN, HIGH);
  Serial.println("\nTerhubung ke Wi-Fi!");
  Serial.print("IP ESP32: ");
  Serial.println(WiFi.localIP());

  udpRx.begin(udpRxPort);

  // Konfigurasi I2S INMP441
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
  if (wifiMulti.run() != WL_CONNECTED) {
    if (millis() - lastBlinkTime >= 250) {
      lastBlinkTime = millis();
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState);
    }
    return;
  } else {
    digitalWrite(LED_PIN, HIGH);
  }

  // =========================================================
  // 1. TERIMA AUDIO DARI LAPTOP (KURAS BUFFER AGAR CONTINUOUS)
  // =========================================================
  int packetSize;
  while ((packetSize = udpRx.parsePacket()) > 0) {
    int bytesRead = udpRx.read(rx_buffer, sizeof(rx_buffer));
    for (int i = 0; i < bytesRead; i++) {
      digitalWrite(BUZZER_PIN, rx_buffer[i] > 127 ? HIGH : LOW);
      delayMicroseconds(50); // Timing presisi untuk output sinyal audio digital
    }
  }

  // =========================================================
  // 2. BACA INMP441 MIC & BROADCAST KE LAPTOP (PORT 8888)
  // =========================================================
  int32_t i2s_buffer[256];
  size_t bytes_read = 0;

  i2s_read(I2S_PORT, i2s_buffer, sizeof(i2s_buffer), &bytes_read, 0);
  int samples_read = bytes_read / sizeof(int32_t);

  if (samples_read <= 0) return;

  int64_t sum = 0;
  for (int i = 0; i < samples_read; i++) {
    sum += (i2s_buffer[i] >> 10);
  }
  int32_t dc_offset = sum / samples_read;

  for (int i = 0; i < samples_read; i++) {
    int32_t sample = (i2s_buffer[i] >> 10) - dc_offset;

    if (sample > 32767) sample = 32767;
    if (sample < -32768) sample = -32768;

    udp_payload[udp_payload_idx++] = (int16_t)sample;

    if (udp_payload_idx >= UDP_PACKET_SAMPLES) {
      udpTx.beginPacket(udpAddress, udpTxPort);
      udpTx.write((uint8_t*)udp_payload, UDP_PACKET_SAMPLES * sizeof(int16_t));
      udpTx.endPacket();
      udp_payload_idx = 0;
    }
  }
}