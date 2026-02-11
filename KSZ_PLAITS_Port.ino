#include <DaisyDuino.h>
#include "voice.h"
#include <U8g2lib.h>
#include <string.h>
#include <cstring>


DaisyHardware hw;
daisy::AnalogControl pots[6];
plaits::Voice voice;
plaits::Patch patch;
plaits::Modulations modulations;

// float out_buffer[256];
plaits::Voice::Frame out_buffer[256];

namespace plaits { UserDataProvider* g_user_data_provider = nullptr; }

// Twoja konfiguracja OLED
U8G2_SSD1306_128X64_NONAME_F_4W_SW_SPI oled(U8G2_R0, 8, 10, 7, 9, 0);

// Nazwy silników Plaits (8 zielonych, 8 czerwonych w oryginale)
const char* engine_names[] = {
  "Virtual Analog", "Waveshaping", "FM", "Grain", 
  "Additive", "Wavetable", "Chord", "Speech",
  "Granular Cloud", "Filtered Noise", "Particle", "Inh. String",
  "Modal Verb", "FM Drum", "Bass Drum", "Snare/Hi-Hat"
};

void MyCallback(float **in, float **out, size_t size) {
    // Zabezpieczenie: jeśli size jakimś cudem przekroczy 256, ograniczamy go
    size_t block_size = (size <= 256) ? size : 256;

    // modulations.frequency = voice.GetLPGGain();



    // Renderujemy dokładnie tyle próbek, ile chce od nas Daisy
    voice.Render(patch, modulations, out_buffer, block_size);

    for (size_t i = 0; i < block_size; i++) {
        out[0][i] = out_buffer[i].out / 32768.0f;
        out[1][i] = out_buffer[i].aux / 32768.0f;
    }

    // for (size_t i = 0; i < block_size; i++) {
    //     out[0][i] = out_buffer[i].out;
    //     out[1][i] = out_buffer[i].aux;
    // }
}


void update_pots() {
    // 1. Czyścimy całą strukturę modyfikacji (ważne ze względu na rozmiar w Twoim voice.h)
    memset(&modulations, 0, sizeof(modulations));

    // 2. Przetwarzamy ADC (AnalogControl robi wygładzanie za nas)
    float p0 = pots[0].Process(); // Freq
    float p1 = pots[1].Process(); // Harmonics
    float p2 = pots[2].Process(); // Timbre
    float p3 = pots[3].Process(); // Morph
    float p4 = pots[4].Process(); // FM Amt
    float p5 = pots[5].Process(); // Engine

    // 3. Przypisanie do struktury Patch
    // Częstotliwość: mapujemy 0..1 na zakres nut MIDI
    patch.note = 24.0f + (p0 * 60.0f); 

    // Parametry barwy (już wygładzone przez AnalogControl)
    patch.harmonics = p1;
    patch.timbre    = p2;
    patch.morph     = p3;
    
    // FM Amount (Twój "Internal Exciter" / Obwiednia)
    patch.frequency_modulation_amount = p4;

    // Wybór silnika (0-15)
    patch.engine = static_cast<int>(p5 * 15.99f);

    // 4. Wymuś wartości dla silników modelowania (Strings/FM/LPG)
    patch.decay = 0.5f;
    patch.lpg_colour = 0.5f;

    // 5. Ustawienia flag modulacji (bezpieczeństwo dla Twojego voice.h)
    modulations.engine = 0.0f;
    modulations.trigger_patched = false;
    modulations.frequency_patched = false;
    modulations.timbre_patched = false;
    modulations.morph_patched = false;
    modulations.level_patched = false;
}



void update_display() {
    oled.clearBuffer();
    // oled.setFont(u8g2_font_6x10_tf);
    // oled.drawStr(3, 10, "PLAITS DAISY PORT");
    // oled.drawHLine(0, 13, 128);
    
    // Nazwa aktualnego silnika
    oled.setFont(u8g2_font_7x14_tf);
    oled.setCursor(3, 15);
    if(patch.engine < 16) {
        oled.print(engine_names[patch.engine]);
    }
    
    oled.setFont(u8g2_font_6x10_tf);

    // HArmonics
    oled.setCursor(3, 26);
    oled.print("Harmonics: ");
    oled.print(patch.harmonics);

    // Timbre
    oled.setCursor(3, 37);
    oled.print("Timbre: ");
    oled.print(patch.timbre);

    // Morph
    oled.setCursor(3, 48);
    oled.print("Morph: ");
    oled.print(patch.morph);

    //FM
    oled.setCursor(3, 59);
    oled.print("FM amt: ");
    oled.print(patch.frequency_modulation_amount);

    // Wizualizacja Timbre
    // oled.drawFrame(0, 55, 128, 7);
    // oled.drawBox(0, 55, (int)(patch.timbre * 128), 7);

    oled.sendBuffer();
}

void setup() {
    hw = DAISY.init(DAISY_SEED, AUDIO_SR_48K);
    
    DAISY.SetAudioBlockSize(48);
    
    


    // Czyścimy wszystko na zero
    memset(&patch, 0, sizeof(patch));
    memset(&modulations, 0, sizeof(modulations));

    // Rezerwujemy pamięć dla silników (np. 8kB)
    // static uint8_t memory_pool[8192];
    static uint8_t memory_pool[128 * 1024]; // 128 KB – Daisy ma 64 MB, nie żałuj jej!
    stmlib::BufferAllocator allocator;
    allocator.Init(memory_pool, sizeof(memory_pool));

    // PODAJEMY ALOKATOR DO FUNKCJI INIT
    voice.Init(&allocator); 

    patch.frequency_modulation_amount = 0.0f;
    patch.timbre_modulation_amount = 0.0f;
    patch.morph_modulation_amount = 0.0f;

    // Ustawiamy domyślne "zdrowe" wartości
    patch.note = 55.0f;
    patch.decay = 0.5f;     // Ważne, nawet jeśli LPG jest wyłączone
    patch.lpg_colour = 0.5f;


    // Konfiguracja pinów dla AnalogControl
    pots[0].Init(hw.GetAdcPin(0), hw.AudioSampleRate()); // A0 - Frequency
    pots[1].Init(hw.GetAdcPin(1), hw.AudioSampleRate()); // A1 - Harmonics
    pots[2].Init(hw.GetAdcPin(2), hw.AudioSampleRate()); // A2 - Timbre
    pots[3].Init(hw.GetAdcPin(3), hw.AudioSampleRate()); // A3 - Morph
    pots[4].Init(hw.GetAdcPin(4), hw.AudioSampleRate()); // A4 - FM Amt
    pots[5].Init(hw.GetAdcPin(5), hw.AudioSampleRate()); // A5 - Engine



    oled.begin();

    
    // hw.StartAudio(MyCallback);
    DAISY.begin(MyCallback);

}

void loop() {
    static uint32_t last_display_update = 0;
    static uint32_t last_pots_update = 0;
    
       if (millis() - last_pots_update > 40) {
        update_pots();
        last_pots_update = millis();
    }

    if (millis() - last_display_update > 100) {
        update_display();
        last_display_update = millis();
    }
}
