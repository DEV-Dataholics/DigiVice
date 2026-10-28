#pragma once
#include <Arduino.h>

// ============================================================================
// Scrap-Hound Audio Synthesizer (ESP32-2432S028 CYD Speaker on GPIO 26)
// Provides acoustic cues for Alarm, Maniacal Laugh, Evil Chomp, and Growl
// ============================================================================

#define AUDIO_PIN 26

class ScrapHoundAudio {
private:
  bool _muted;

public:
  ScrapHoundAudio() : _muted(false) {}

  void begin() {
    pinMode(AUDIO_PIN, OUTPUT);
    noTone(AUDIO_PIN);
  }

  void setMuted(bool mute) {
    _muted = mute;
    if (_muted) noTone(AUDIO_PIN);
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

  // Quick UI tap feedback
  void playTap() {
    if (_muted) return;
    playTone(2400, 15);
  }

  // Low angry growl when the pet is poked or tapped
  void playGrowl() {
    if (_muted) return;
    for (int i = 0; i < 4; i++) {
      playTone(160, 25);
      delay(10);
      playTone(140, 30);
      delay(10);
    }
  }

  // Ticket Opened: Two-tone industrial klaxon / war-horn siren
  void playWarKlaxon() {
    if (_muted) return;
    for (int rep = 0; rep < 2; rep++) {
      for (int f = 550; f <= 1100; f += 50) {
        tone(AUDIO_PIN, f, 12);
        delay(10);
      }
      delay(30);
      for (int f = 1100; f >= 550; f -= 50) {
        tone(AUDIO_PIN, f, 12);
        delay(10);
      }
      delay(40);
    }
    noTone(AUDIO_PIN);
  }

  // Ticket Updated: Maniacal Gremlin / Mad Max cackle laugh
  void playManiacalLaugh() {
    if (_muted) return;
    int laughNotes[] = {880, 1174, 1396, 987, 1318, 1567, 1108, 1479, 1760};
    for (int i = 0; i < 9; i++) {
      playTone(laughNotes[i], 35);
      delay(20);
    }
    // High-pitched giggle flutter
    for (int i = 0; i < 4; i++) {
      playTone(1800, 18);
      delay(15);
      playTone(2100, 20);
      delay(15);
    }
  }

  // Ticket Resolved: Evil Screen-Shake CHOMP crunch + Victory Fanfare
  void playEvilChompFanfare() {
    if (_muted) return;
    // Metallic bite snap / crunch noise sweep
    for (int f = 2800; f >= 300; f -= 250) {
      tone(AUDIO_PIN, f, 8);
      delay(6);
    }
    // Heavy low crunch impact
    tone(AUDIO_PIN, 120, 90);
    delay(100);
    noTone(AUDIO_PIN);
    delay(50);

    // Mad victory fanfare (Wasteland style)
    int victory[] = {523, 659, 784, 1046, 1318};
    for (int i = 0; i < 5; i++) {
      playTone(victory[i], 50);
      delay(20);
    }
    playTone(1567, 180);
  }
};
