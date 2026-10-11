/*
 * ============================================================
 *  ESP32 Murottal Bluetooth - BAGIAN 2: BLUETOOTH BRIDGE
 * ============================================================
 *  Tugas board ini:
 *   - Terima audio dari ESP32-S3 lewat I2S
 *   - Kirim audio ke speaker Bluetooth (A2DP Source)
 *   - WiFi TIDAK dipakai sama sekali (supaya Bluetooth stabil)
 *
 *  Fitur auto-recovery:
 *   - Speaker putus sebentar  -> auto reconnect
 *   - Speaker putus > 60 detik -> restart & scan ulang
 *
 *  Board  : ESP32 original / WROOM-32 (diuji: LilyGO T-Display v1.2)
 *           WAJIB chip yang punya Bluetooth Classic.
 *           ESP32-S2 / S3 / C3 / C6 TIDAK BISA (tidak ada A2DP).
 *  Library: ESP32-A2DP, arduino-audio-tools (pschatzmann)
 * ============================================================
 */

#include "AudioTools.h"
#include "AudioTools/Communication/A2DPStream.h"

// ================== KONFIGURASI (UBAH DI SINI) ==================
// Nama speaker Bluetooth (cukup sebagian nama, cek pakai HP saat pairing)
const char* speakerKeyword = "NAMA_SPEAKER_KAMU";   // contoh: "WZ-BT5.0", "JBL"

// Pin I2S masuk (dari ESP32-S3)
// LilyGO T-Display : 26, 25, 33
// ESP32 DevKit biasa: boleh juga 14, 15, 32
const int PIN_BCK  = 26;  // <- S3 GPIO4
const int PIN_WS   = 25;  // <- S3 GPIO5
const int PIN_DATA = 33;  // <- S3 GPIO6

// Restart kalau speaker tidak tersambung selama ini (ms)
const unsigned long BATAS_PUTUS = 60000;
// ================================================================

I2SStream i2s;
BluetoothA2DPSource a2dp_source;

unsigned long lastConnected = 0;

// Dipanggil Bluetooth: ambil audio dari I2S, teruskan ke speaker
int32_t get_sound_data(uint8_t* data, int32_t len) {
  return i2s.readBytes(data, len);
}

// Cari speaker berdasarkan nama
bool address_valid(const char* name, esp_bd_addr_t addr, int rssi) {
  if (name != nullptr && strstr(name, speakerKeyword) != nullptr) {
    Serial.printf("Speaker ditemukan: %s (RSSI: %d)\n", name, rssi);
    return true;
  }
  return false;
}

void setup() {
  Serial.begin(115200);

  // I2S sebagai SLAVE (ikut clock dari S3)
  auto cfg = i2s.defaultConfig(RX_MODE);
  cfg.sample_rate = 44100;
  cfg.channels = 2;
  cfg.bits_per_sample = 16;
  cfg.is_master = false;
  cfg.pin_bck = PIN_BCK;
  cfg.pin_ws = PIN_WS;
  cfg.pin_data = PIN_DATA;
  i2s.begin(cfg);

  // Bluetooth ke speaker
  a2dp_source.set_ssid_callback(address_valid);
  a2dp_source.set_data_callback(get_sound_data);
  a2dp_source.set_auto_reconnect(true);
  a2dp_source.start();
  Serial.println("Menunggu speaker...");

  lastConnected = millis();
}

void loop() {
  static unsigned long lastPrint = 0;

  if (a2dp_source.is_connected()) {
    lastConnected = millis();
  }

  // Laporan status tiap 30 detik
  if (millis() - lastPrint > 30000) {
    lastPrint = millis();
    Serial.printf("Speaker: %s | Uptime: %lu menit\n",
      a2dp_source.is_connected() ? "CONNECTED" : "PUTUS",
      millis() / 60000);
  }

  // Penjaga: speaker putus terlalu lama -> restart & scan ulang
  if (millis() - lastConnected > BATAS_PUTUS) {
    Serial.println("Speaker putus terlalu lama, restart...");
    delay(500);
    ESP.restart();
  }

  delay(100);
}
