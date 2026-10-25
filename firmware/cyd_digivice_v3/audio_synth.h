#pragma once
#include <Arduino.h>

// ============================================================================
// Cyberpunk Synthesizer Engine for ESP32-2432S028 (CYD)
// Output: Audio DAC / Speaker Header on GPIO 26
// Compatible with ESP32 Arduino Core 2.x and 3.x
// ============================================================================

#define AUDIO_PIN 26

class CyberSynth {
private:
  bool _muted;

public:
  CyberSynth() : _muted(false) {}

  void begin() {
    pinMode(AUDIO_PIN, OUTPUT);
    noTone(AUDIO_PIN);
  }

  void setMuted(bool mute) {
    _muted = mute;
    if (_muted) {
      noTone(AUDIO_PIN);
    }
  }

  bool isMuted() const {
    return _muted;
  }

  void playTone(int freq, int durationMs) {
    if (_muted || freq <= 0) return;
    tone(AUDIO_PIN, freq, durationMs);
    delay(durationMs);
    noTone(AUDIO_PIN);
  }

  void playTap() {
    if (_muted) return;
    playTone(2800, 15);
  }

  void playChirp() {
    if (_muted) return;
    playTone(1800, 30);
    delay(20);
    playTone(2600, 45);
  }

  void playGatePass() {
    if (_muted) return;
    playTone(1600, 35);
    delay(30);
    playTone(2400, 50);
    delay(30);
    playTone(3200, 70);
  }

  void playGateFail() {
    if (_muted) return;
    playTone(850, 70);
    delay(40);
    playTone(550, 110);
  }

  void playEvolutionArpeggio() {
    if (_muted) return;
    const int arpeggio[] = { 1046, 1318, 1567, 2093, 2637, 3135 }; // C6 - E6 - G6 - C7 - E7 - G7
    for (int i = 0; i < 6; i++) {
      playTone(arpeggio[i], 50);
      delay(30);
    }
  }

  void playCircuitBreakerAlert() {
    if (_muted) return;
    for (int i = 0; i < 3; i++) {
      playTone(1900, 60);
      delay(40);
      playTone(950, 80);
      delay(40);
    }
  }

  void playTombstoneGlitch() {
    if (_muted) return;
    const int freqs[] = { 400, 250, 600, 180, 120 };
    for (int f : freqs) {
      playTone(f, 35);
      delay(20);
    }
  }
};

extern CyberSynth soundFx;
