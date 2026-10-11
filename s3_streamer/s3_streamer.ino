/*
 * ============================================================
 *  ESP32 Murottal Bluetooth - BAGIAN 1: STREAMER (ESP32-S3)
 * ============================================================
 *  Tugas board ini:
 *   - Connect ke WiFi
 *   - Ambil stream radio Quran (MP3) dari internet
 *   - Decode MP3 -> audio PCM
 *   - Kirim audio lewat I2S ke board Bluetooth (ESP32 / T-Display)
 *
 *  Fitur auto-recovery:
 *   - WiFi putus     -> reconnect otomatis
 *   - Stream macet   -> sambung ulang otomatis
 *   - Gagal 5x       -> restart otomatis
 *
 *  Board  : ESP32-S3 (diuji: ESP32-S3 N16R8)
 *  Library: arduino-audio-tools, arduino-libhelix (pschatzmann)
 * ============================================================
 */

#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include "AudioTools/Communication/AudioHttp.h"

// ================== KONFIGURASI (UBAH DI SINI) ==================
const char* ssid      = "NAMA_WIFI_KAMU";
const char* password  = "PASSWORD_WIFI_KAMU";

// Stream radio (WAJIB http, bukan https, dan tanpa redirect)
const char* streamURL = "http://66.45.232.132:9994/stream";

// Pin I2S keluar (ke board Bluetooth)
const int PIN_BCK  = 4;   // -> Bluetooth board BCK
const int PIN_WS   = 5;   // -> Bluetooth board WS
const int PIN_DATA = 6;   // -> Bluetooth board DATA
// ================================================================

URLStream url(ssid, password);
I2SStream i2s;
EncodedAudioStream dec(&i2s, new MP3DecoderHelix());
StreamCopy copier(dec, url);

unsigned long lastData = 0;
int gagalBeruntun = 0;

// Buka stream, restart kalau gagal 5x berturut-turut
void bukaStream() {
  url.end();
  delay(1000);

  Serial.printf("Menyambung ke: %s\n", streamURL);
  if (url.begin(streamURL, "audio/mpeg")) {
    Serial.println(">> Berhasil, memutar...");
    gagalBeruntun = 0;
  } else {
    gagalBeruntun++;
    Serial.printf(">> Gagal (%d kali)\n", gagalBeruntun);
    if (gagalBeruntun >= 5) {
      Serial.println("Restart ESP32-S3...");
      delay(1000);
      ESP.restart();
    }
  }
  lastData = millis();
}

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  // I2S sebagai MASTER (S3 yang mengatur clock)
  auto cfg = i2s.defaultConfig(TX_MODE);
  cfg.sample_rate = 44100;
  cfg.channels = 2;
  cfg.bits_per_sample = 16;
  cfg.is_master = true;
  cfg.pin_bck = PIN_BCK;
  cfg.pin_ws = PIN_WS;
  cfg.pin_data = PIN_DATA;
  i2s.begin(cfg);

  dec.begin();
  url.setTimeout(10000);
  bukaStream();
}

void loop() {
  static size_t total = 0;
  static unsigned long lastPrint = 0;

  size_t n = copier.copy();
  if (n > 0) {
    total += n;
    lastData = millis();
  }

  // Laporan status tiap 30 detik
  if (millis() - lastPrint > 30000) {
    lastPrint = millis();
    Serial.printf("Data: %d KB | WiFi: %s | Uptime: %lu menit\n",
      (int)(total / 1024),
      WiFi.status() == WL_CONNECTED ? "OK" : "PUTUS",
      millis() / 60000);
  }

  // WiFi putus -> reconnect
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi putus, reconnect...");
    WiFi.reconnect();
    delay(3000);
    return;
  }

  // Stream macet > 15 detik -> sambung ulang
  if (millis() - lastData > 15000) {
    Serial.println("Stream macet, sambung ulang...");
    bukaStream();
  }
}
