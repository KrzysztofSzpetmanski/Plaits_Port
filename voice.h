// Copyright 2016 Emilie Gillet.
//
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
// 
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// -----------------------------------------------------------------------------
//
// Main synthesis voice.

#ifndef PLAITS_DSP_VOICE_H_
#define PLAITS_DSP_VOICE_H_

#include "stmlib.h"

#include "filter.h"
#include "limiter.h"
#include "buffer_allocator.h"

#include "additive_engine.h"
#include "bass_drum_engine.h"
#include "chord_engine.h"
#include "engine.h"
#include "fm_engine.h"
#include "grain_engine.h"
#include "hi_hat_engine.h"
#include "modal_engine.h"
#include "noise_engine.h"
#include "particle_engine.h"
#include "snare_drum_engine.h"
#include "speech_engine.h"
#include "string_engine.h"
#include "swarm_engine.h"
#include "virtual_analog_engine.h"
#include "waveshaping_engine.h"
#include "wavetable_engine.h"

#include "chiptune_engine.h"
#include "phase_distortion_engine.h"
#include "six_op_engine.h"
#include "string_machine_engine.h"
#include "virtual_analog_vcf_engine.h"
#include "wave_terrain_engine.h"

#include "envelope.h"

#include "low_pass_gate.h"

namespace plaits {

const int kMaxEngines = 24;
const int kMaxTriggerDelay = 8;
const int kTriggerDelay = 5;

class ChannelPostProcessor {
 public:
  ChannelPostProcessor() { }
  ~ChannelPostProcessor() { }
  
  void Init() {
    lpg_.Init();
    Reset();
  }
  
  void Reset() {
    limiter_.Init();
  }
  
  void Process(
      float gain,
      bool bypass_lpg,
      float low_pass_gate_gain,
      float low_pass_gate_frequency,
      float low_pass_gate_hf_bleed,
      float* in,
      short* out,
      size_t size,
      size_t stride) {
    if (gain < 0.0f) {
      limiter_.Process(-gain, in, size);
    }
    const float post_gain = (gain < 0.0f ? 1.0f : gain) * -32767.0f;
    if (!bypass_lpg) {
      lpg_.Process(
          post_gain * low_pass_gate_gain,
          low_pass_gate_frequency,
          low_pass_gate_hf_bleed,
          in,
          out,
          size,
          stride);
    } else {
      while (size--) {
        *out = stmlib::Clip16(1 + static_cast<int32_t>(*in++ * post_gain));
        out += stride;
      }
    }
  }
  
 private:
  stmlib::Limiter limiter_;
  LowPassGate lpg_;
  
  DISALLOW_COPY_AND_ASSIGN(ChannelPostProcessor);
};

struct Patch {
  float note;
  float harmonics;
  float timbre;
  float morph;
  float frequency_modulation_amount;
  float timbre_modulation_amount;
  float morph_modulation_amount;

  int engine;
  float decay;
  float lpg_colour;
};

struct Modulations {
  float engine;
  float note;
  float frequency;
  float harmonics;
  float timbre;
  float morph;
  float trigger;
  float level;

  bool frequency_patched;
  bool timbre_patched;
  bool morph_patched;
  bool trigger_patched;
  bool level_patched;
  
  // Warps Lite dual-stage audio modulation (optional, for Daisy Patch port)
  // Set audio_mod_in to nullptr to disable that stage
  // Stage 1 modes: 0=OFF, 1=XFADE, 2=FOLD, 3=AnaRM, 4=DigRM, 5=XOR, 6=COMP, 7=FM
  // Stage 2 modes: 0=OFF, 1=XFADE, 2=FOLD, 3=AnaRM, 4=DigRM, 5=XOR, 6=COMP, 7=FM, 8=VOCODER
  // Signal flow: Plaits -> Stage1(IN1) -> Stage2(IN2) -> wet out
  
  // Stage 1 (IN1) - 7 algorithms (no Vocoder)
  const float* audio_mod_in1;   // Pointer to modulation input buffer (Audio In 1)
  int audio_mod_mode1;          // Algorithm selection
  float audio_mod_gain1;        // 1.0 to 10.0 (input preamp for weak signals)
  float audio_mod_level1;       // 0.0 to 1.0 (modulator level into algorithm)
  float audio_mod_timbre1;      // 0.0 to 1.0 (algorithm-specific parameter)
  
  // Stage 2 (IN2) - 8 algorithms (includes Vocoder)
  const float* audio_mod_in2;   // Pointer to modulation input buffer (Audio In 2)
  int audio_mod_mode2;          // Algorithm selection
  float audio_mod_gain2;        // 1.0 to 10.0 (input preamp for weak signals)
  float audio_mod_level2;       // 0.0 to 1.0 (modulator level into algorithm)
  float audio_mod_timbre2;      // 0.0 to 1.0 (algorithm-specific parameter)
};

// char (*__foo)[sizeof(HiHatEngine)] = 1;


class Voice {
 public:
  Voice() { }
  ~Voice() { }
  
  struct Frame {
    short out;
    short aux;
    short out_dry;  // Dry output (no audio modulation applied)
    short aux_dry;  // Dry aux (no audio modulation applied)
  };
  
  void Init(stmlib::BufferAllocator* allocator);
  void ReloadUserData() {
    reload_user_data_ = true;
  }
  void Render(
      const Patch& patch,
      const Modulations& modulations,
      Frame* frames,
      size_t size);
  inline int active_engine() const { return previous_engine_index_; }
  
  // Get LPG envelope gain for CV output (0.0 to 1.0)
  inline float GetLPGGain() const { return lpg_envelope_.gain(); }
  
  // Get internal decay envelope value (0.0 to 1.0) - for polyphonic voice management
  inline float GetDecayEnvelope() const { return decay_envelope_.value(); }
    
 private:
  void ComputeDecayParameters(const Patch& settings);
  
  inline float ApplyModulations(
      float base_value,
      float modulation_amount,
      bool use_external_modulation,
      float external_modulation,
      bool use_internal_envelope,
      float envelope,
      float default_internal_modulation,
      float minimum_value,
      float maximum_value) {
    float value = base_value;
    modulation_amount *= std::max(fabsf(modulation_amount) - 0.05f, 0.05f);
    modulation_amount *= 1.05f;
    
    float modulation = use_external_modulation
        ? external_modulation
        : (use_internal_envelope ? envelope : default_internal_modulation);
    value += modulation_amount * modulation;
    CONSTRAIN(value, minimum_value, maximum_value);
    return value;
  }

  VirtualAnalogEngine virtual_analog_engine_;
  WaveshapingEngine waveshaping_engine_;
  FMEngine fm_engine_;
  GrainEngine grain_engine_;
  AdditiveEngine additive_engine_;
  WavetableEngine wavetable_engine_;
  ChordEngine chord_engine_;
  SpeechEngine speech_engine_;

  SwarmEngine swarm_engine_;
  NoiseEngine noise_engine_;
  ParticleEngine particle_engine_;
  StringEngine string_engine_;
  ModalEngine modal_engine_;
  BassDrumEngine bass_drum_engine_;
  SnareDrumEngine snare_drum_engine_;
  HiHatEngine hi_hat_engine_;
  
  VirtualAnalogVCFEngine virtual_analog_vcf_engine_;
  PhaseDistortionEngine phase_distortion_engine_;
  SixOpEngine six_op_engine_;
  WaveTerrainEngine wave_terrain_engine_;
  StringMachineEngine string_machine_engine_;
  ChiptuneEngine chiptune_engine_;

  stmlib::HysteresisQuantizer2 engine_quantizer_;
  
  bool reload_user_data_;
  int previous_engine_index_;
  float engine_cv_;
  
  float previous_note_;
  bool trigger_state_;
  
  DecayEnvelope decay_envelope_;
  LPGEnvelope lpg_envelope_;
  
  float trigger_delay_line_[kMaxTriggerDelay];
  DelayLine<float, kMaxTriggerDelay> trigger_delay_;
  
  ChannelPostProcessor out_post_processor_;
  ChannelPostProcessor aux_post_processor_;
  ChannelPostProcessor out_dry_post_processor_;
  ChannelPostProcessor aux_dry_post_processor_;
  
  EngineRegistry<kMaxEngines> engines_;
  
  float out_buffer_[kMaxBlockSize];
  float aux_buffer_[kMaxBlockSize];
  float out_buffer_dry_[kMaxBlockSize];
  float aux_buffer_dry_[kMaxBlockSize];
  
  // Warps Lite phase modulator state
  float fm_phase_;
  
  // Apply Warps Lite audio modulation to wet buffers
  // Stage 1: 7 algorithms (no Vocoder), Stage 2: 8 algorithms (with Vocoder)
  void ApplyAudioModulation(
      float* out, float* aux,
      const float* mod_in,
      int mode, float gain, float level, float timbre,
      size_t size,
      bool allow_vocoder);
  
  DISALLOW_COPY_AND_ASSIGN(Voice);
};

}  // namespace plaits

#endif  // PLAITS_DSP_VOICE_H_
