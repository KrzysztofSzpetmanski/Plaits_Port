#include <DaisyDuino.h>
#include "voice.h"        // Główna klasa silnika Plaits
#include <U8g2lib.h>      // Biblioteka dla OLED

// Inicjalizacja sprzętu
DaisyHardware hw;
plaits::Voice voice;
plaits::Patch patch;
plaits::Modulations modulations;

// Bufor audio (256 dla bezpieczeństwa, aby uniknąć błędów alokacji)
float out_buffer[256];

// Konfiguracja OLED (Software SPI zgodnie z Twoim pinoutem)
U8G2_SSD1306_128X64_NONAME_F_4W_SW_SPI oled(U8G2_R0, 8, 10, 7, 9, 0);

// Tablica nazw silników Plaits
const char* engine_names[] = {
  "Virtual Analog", "Waveshaping", "FM", "Grain", 
  "Additive", "Wavetable", "Chord", "Speech",
  "Granular Cloud", "Filtered Noise", "Particle", "Inh. String",
  "Modal Verb", "FM Drum", "Bass Drum", "Snare/Hi-Hat"
};

void MyCallback(float **in, float **out, size_t size) {
    // 1. Odczyt parametrów z potencjometrów
    patch.harmonics = analogRead(A0) / 1023.0f;
    patch.timbre = analogRead(A1) / 1023.0f;
    patch.morph = analogRead(A2) / 1023.0f;
    
    // 2. Wybór silnika (mapujemy potencjometr A3 na 16 silników)
    int engine_idx = (analogRead(A3) / 1024.0f) * 16;
    patch.engine = engine_idx;

    // 3. Częstotliwość (V/Oct lub stała)
    patch.frequency = 440.0f / 48000.0f; 

    // 4. Generowanie dźwięku do bufora
    voice.Render(patch, modulations, out_buffer, size);

    // 5. Przekazanie do wyjść audio
    for (size_t i = 0; i < size; i++) {
        out[0][i] = out_buffer[i];
        out[1][i] = out_buffer[i];
    }
}

void update_display() {
    oled.clearBuffer();
    oled.setFont(u8g2_font_6x10_tf);
    oled.drawStr(0, 10, "PLAITS DAISY PORT");
    oled.drawHLine(0, 13, 128);
    
    // Wyświetlanie aktualnego silnika
    oled.setFont(u8g2_font_9x15_tf);
    oled.setCursor(0, 40);
    oled.print(engine_names[patch.engine]);
    
    // Pasek wizualny dla parametru Timbre
    oled.drawFrame(0, 55, 128, 7);
    oled.drawBox(0, 55, (int)(patch.timbre * 128), 7);
    
    oled.sendBuffer();
}

void setup() {
    // Inicjalizacja Daisy
    hw = DAISY.init(DAISY_SEED, AUDIO_SR_48K);
    
    // Inicjalizacja OLED
    oled.begin();

    // Inicjalizacja silnika Plaits
    voice.Init();
    
    // Ustawienia początkowe modulacji
    modulations.engine = 0.0f;
    modulations.frequency = 0.0f;
    modulations.harmonics = 0.0f;
    modulations.timbre = 0.0f;
    modulations.morph = 0.0f;
    modulations.trigger = 0.0f; // Jeśli nie używasz Triggera, Plaits gra w trybie ciągłym
    modulations.level = 1.0f;

    DAISY.begin(MyCallback);
}

void loop() {
    // Odświeżanie ekranu (nie robimy tego w MyCallback!)
    static uint32_t last_display_update = 0;
    if (millis() - last_display_update > 100) { // 10 klatek na sekundę wystarczy
        update_display();
        last_display_update = millis();
    }
}