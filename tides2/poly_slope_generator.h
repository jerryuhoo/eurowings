// Copyright 2017 Emilie Gillet.
//
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to enable, copy, modify, merge, publish, distribute, sublicense, and/or sell
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
// 4 related slope generators.

#ifndef TIDES_POLY_SLOPE_GENERATOR_H_
#define TIDES_POLY_SLOPE_GENERATOR_H_

#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/parameter_interpolator.h"
#include "stmlib/dsp/polyblep.h"
#include "stmlib/dsp/hysteresis_quantizer.h"
#include "stmlib/utils/random.h"

#include "tides2/ramp_generator.h"
#include "tides2/ramp_shaper.h"
#include "tides2/resources.h"

namespace tides {

#define INSTANTIATE(x, y, z) \
  render_fn_table_[x][y][z] = &PolySlopeGenerator::RenderInternal<x, y, z>;

#define INSTANTIATE_RAM(x, y, z) \
  render_fn_table_[x][y][z] = &PolySlopeGenerator::RenderInternal_RAM<x, y, z>;

template<size_t num_channels>
class Filter {
 public:
  Filter() { }
  ~Filter() { }

  void Init() {
    std::fill(&lp_1_[0], &lp_1_[num_channels], 0.0f);
    std::fill(&lp_2_[0], &lp_2_[num_channels], 0.0f);
  }

  template<size_t num_effective_channels>
  inline void Process(
      float* f,
      float* in_out,
      size_t size) {
    while (size--) {
      for (size_t i = 0; i < num_effective_channels; ++i) {
        ONE_POLE(lp_1_[i], *in_out, f[i]);
        ONE_POLE(lp_2_[i], lp_1_[i], f[i]);
        *in_out++ = lp_2_[i];
      }
      in_out += num_channels - num_effective_channels;
    }
  }
  
 private:
  float lp_1_[num_channels];
  float lp_2_[num_channels];

  DISALLOW_COPY_AND_ASSIGN(Filter);
};

class PolySlopeGenerator {
 public:
  PolySlopeGenerator() { }
  ~PolySlopeGenerator() { }
  
  enum {
    num_channels = 4
  };

  enum FeatureMode {
    FEATURE_MODE_TIDES,
    FEATURE_MODE_QUANTUM
  };
  
  struct OutputSample {
    float channel[num_channels];
  };
  
  void Reset() {
    filter_.Init();
  }
  
  void Init() {
    frequency_ = 0.01f;
    pw_ = 0.0f;
    shift_ = 0.0f;
    shape_ = 0.0f;
    fold_ = 0.0f;
    
    feature_mode_ = FEATURE_MODE_TIDES;
    
    // NEW INIT
    shift_register_ = 0x8000; // Seed
    sub_clock_counter_ = 0;
    gate_probability_mode_ = 0;
    sub_divider_mode_ = 0;
    std::fill(&quantum_current_[0], &quantum_current_[num_channels], 0.0f);
    std::fill(&quantum_target_[0], &quantum_target_[num_channels], 0.0f);
    quantum_trigger_state_ = 0;

    ramp_generator_.Init();
    for (size_t i = 0; i < num_channels; ++i) {
      ramp_shaper_[i].Init();
      ramp_waveshaper_[i].Init();
    }
    filter_.Init();
    
    ratio_index_quantizer_.Init(21, 0.05f, false);
    
    // Force template instantiation for all combinations of settings.
    INSTANTIATE(RAMP_MODE_AD, OUTPUT_MODE_GATES, RANGE_CONTROL);
    INSTANTIATE(RAMP_MODE_AD, OUTPUT_MODE_GATES, RANGE_AUDIO);
    INSTANTIATE(RAMP_MODE_AD, OUTPUT_MODE_AMPLITUDE, RANGE_CONTROL);
    INSTANTIATE(RAMP_MODE_AD, OUTPUT_MODE_AMPLITUDE, RANGE_AUDIO);
    INSTANTIATE(RAMP_MODE_AD, OUTPUT_MODE_SLOPE_PHASE, RANGE_CONTROL);
    INSTANTIATE(RAMP_MODE_AD, OUTPUT_MODE_SLOPE_PHASE, RANGE_AUDIO);
    INSTANTIATE(RAMP_MODE_AD, OUTPUT_MODE_FREQUENCY, RANGE_CONTROL);
    INSTANTIATE(RAMP_MODE_AD, OUTPUT_MODE_FREQUENCY, RANGE_AUDIO);

    INSTANTIATE_RAM(RAMP_MODE_AR, OUTPUT_MODE_GATES, RANGE_CONTROL);
    INSTANTIATE_RAM(RAMP_MODE_AR, OUTPUT_MODE_GATES, RANGE_AUDIO);
    INSTANTIATE_RAM(RAMP_MODE_AR, OUTPUT_MODE_AMPLITUDE, RANGE_CONTROL);
    INSTANTIATE_RAM(RAMP_MODE_AR, OUTPUT_MODE_AMPLITUDE, RANGE_AUDIO);
    INSTANTIATE_RAM(RAMP_MODE_AR, OUTPUT_MODE_SLOPE_PHASE, RANGE_CONTROL);
    INSTANTIATE_RAM(RAMP_MODE_AR, OUTPUT_MODE_SLOPE_PHASE, RANGE_AUDIO);
    INSTANTIATE_RAM(RAMP_MODE_AR, OUTPUT_MODE_FREQUENCY, RANGE_CONTROL);
    INSTANTIATE_RAM(RAMP_MODE_AR, OUTPUT_MODE_FREQUENCY, RANGE_AUDIO);

    INSTANTIATE_RAM(RAMP_MODE_LOOPING, OUTPUT_MODE_GATES, RANGE_CONTROL);
    INSTANTIATE_RAM(RAMP_MODE_LOOPING, OUTPUT_MODE_GATES, RANGE_AUDIO);
    INSTANTIATE_RAM(RAMP_MODE_LOOPING, OUTPUT_MODE_AMPLITUDE, RANGE_CONTROL);
    INSTANTIATE_RAM(RAMP_MODE_LOOPING, OUTPUT_MODE_AMPLITUDE, RANGE_AUDIO);
    INSTANTIATE_RAM(RAMP_MODE_LOOPING, OUTPUT_MODE_SLOPE_PHASE, RANGE_CONTROL);
    INSTANTIATE_RAM(RAMP_MODE_LOOPING, OUTPUT_MODE_SLOPE_PHASE, RANGE_AUDIO);
    INSTANTIATE_RAM(RAMP_MODE_LOOPING, OUTPUT_MODE_FREQUENCY, RANGE_CONTROL);
    INSTANTIATE_RAM(RAMP_MODE_LOOPING, OUTPUT_MODE_FREQUENCY, RANGE_AUDIO);
  }
  
  typedef void (PolySlopeGenerator::*RenderFn)(
      float frequency, float pw, float shape, float smoothness, float shift,
      const stmlib::GateFlags* gate_flags, const float* ramp,
      OutputSample* output, size_t size);
  
  void Render(
      RampMode ramp_mode,
      OutputMode output_mode,
      Range range,
      float frequency,
      float pw,
      float shape,
      float smoothness,
      float shift,
      const stmlib::GateFlags* gate_flags,
      const float* ramp,
      OutputSample* out,
      size_t size) {
    
    // 恢复为不传 range 参数
    if (feature_mode_ == FEATURE_MODE_QUANTUM) {
      RenderQuantum(frequency, pw, shape, smoothness, shift, gate_flags, ramp, out, size);
      return; 
    }

    // --- 以下保持原厂代码不变 ---
    const float max_ratio = output_mode == OUTPUT_MODE_FREQUENCY
      ? (range == RANGE_CONTROL ? 0.125f : 0.25f)
      : 1.0f;
    
    frequency = std::min(frequency, 0.25f * max_ratio);
    
    if (range == RANGE_CONTROL && pw < 0.5f) {
      pw = 0.5f + 0.6f * (pw - 0.5f) / (fabsf(pw - 0.5f) + 0.1f);
    }
    
    if (ramp && ramp_mode == RAMP_MODE_AR) {
      frequency *= 1.0f + 2.0f * fabsf(pw - 0.5f);
    }
    
    const float slope = 3.0f + fabsf(pw - 0.5f) * 5.0f;
    const float shape_amount = fabsf(shape - 0.5f) * 2.0f;
    const float shape_amount_attenuation = Tame(frequency, slope, 16.0f);
    shape = 0.5f + (shape - 0.5f) * shape_amount_attenuation;

    if (smoothness > 0.5f) {
      smoothness = 0.5f + (smoothness - 0.5f) * Tame(
          frequency,
          slope * (3.0f + shape_amount * shape_amount_attenuation * 5.0f),
          12.0f);
    }

    (this->*render_fn_table_[ramp_mode][output_mode][range])(
        frequency, pw, shape, smoothness, shift, gate_flags, ramp, out, size);
    
    if (smoothness < 0.5f) {
      float ratio = smoothness * 2.0f;
      ratio *= ratio;
      ratio *= ratio;
      
      float f[4];
      size_t last_channel = output_mode == OUTPUT_MODE_GATES ? 1 : num_channels;
      for (size_t i = 0; i < last_channel; ++i) {
        size_t source = output_mode == OUTPUT_MODE_FREQUENCY ? i : 0;
        f[i] = ramp_generator_.frequency(source) * 0.5f;
        f[i] += (1.0f - f[i]) * ratio;
      }
      if (output_mode == OUTPUT_MODE_GATES) {
        filter_.Process<1>(f, &out[0].channel[0], size);
      } else {
        filter_.Process<num_channels>(f, &out[0].channel[0], size);
      }
    }
  }

  void set_feature_mode(FeatureMode mode) {
    feature_mode_ = mode;
  }

  void set_gate_probability_mode(int mode) { gate_probability_mode_ = mode; }
  void set_sub_divider_mode(int mode) { sub_divider_mode_ = mode; }
  int gate_probability_mode() const { return gate_probability_mode_; }
  int sub_divider_mode() const { return sub_divider_mode_; }
  
 private:
  // --- Standard Tides DSP Helpers ---
  inline float Tame(float f0, float harmonics, float order) {
    f0 *= harmonics;
    float max_f = 0.5f * (1.0f / order);
    float max_amount = 1.0f - (f0 - max_f) / (0.5f - max_f);
    CONSTRAIN(max_amount, 0.0f, 1.0f);
    return max_amount * max_amount * max_amount;
  }

  template<RampMode ramp_mode>
  inline float Fold(float unipolar, float fold_amount) {
    if (ramp_mode == RAMP_MODE_LOOPING) {
      float bipolar = 2.0f * unipolar - 1.0f;
      float folded = fold_amount > 0.0f ? stmlib::Interpolate(
          lut_bipolar_fold,
          0.5f + bipolar * (0.03f + 0.46f * fold_amount),
          1024.0f) : 0.0f;
      return 5.0f * (bipolar + (folded - bipolar) * fold_amount);
    } else {
      float folded = fold_amount > 0.0f ? stmlib::Interpolate(
          lut_unipolar_fold,
          unipolar * fold_amount,
          1024.0f) : 0.0f;
      return 8.0f * (unipolar + (folded - unipolar) * fold_amount);
    }
  }
  
  template<RampMode ramp_mode>
  inline float Scale(float unipolar) {
    if (ramp_mode == RAMP_MODE_LOOPING) {
      return 10.0f * unipolar - 5.0f;
    } else {
      return 8.0f * unipolar;
    }
  }

  // --- Standard Tides Internal Renderers ---
  template<RampMode ramp_mode, OutputMode output_mode, Range range>
  inline void RenderInternal(
      float frequency,
      float pw,
      float shape,
      float smoothness,
      float shift,
      const stmlib::GateFlags* gate_flags,
      const float* ramp,
      OutputSample* out,
      size_t size) {
    const bool is_phasor = !(range == RANGE_AUDIO && \
        ramp_mode == RAMP_MODE_LOOPING);

    stmlib::ParameterInterpolator fm(&frequency_, frequency, size);
    stmlib::ParameterInterpolator pwm(&pw_, pw, size);
    stmlib::ParameterInterpolator shift_modulation(
        &shift_, 2.0f * shift - 1.0f, size);
    stmlib::ParameterInterpolator shape_modulation(
        &shape_, is_phasor ? shape * 5.9999f + 5.0f : shape * 3.9999f, size);
    stmlib::ParameterInterpolator fold_modulation(
        &fold_, std::max(2.0f * (smoothness - 0.5f), 0.0f), size);
    
    if (output_mode == OUTPUT_MODE_FREQUENCY) {
      const int ratio_index = ratio_index_quantizer_.Process(shift);
      if (range == RANGE_CONTROL) {
        ramp_generator_.set_next_ratio(control_ratio_table_[ratio_index]);
      } else {
        ramp_generator_.set_next_ratio(audio_ratio_table_[ratio_index]);
      }
    }
    
    for (size_t i = 0; i < size; ++i) {
      const float f0 = fm.Next();
      const float pw = pwm.Next();
      const float shift = shift_modulation.Next();
      const float step = shift * (1.0f / (num_channels - 1));
      const float partial_step = shift * (1.0f / num_channels);
      const float fold = fold_modulation.Next();

      float per_channel_pw[num_channels];
      const float pw_increment = (shift > 0.0f ? (1.0f - pw) : pw) * step;
      for (size_t j = 0; j < num_channels; ++j) {
        per_channel_pw[j] = pw + pw_increment * float(j);
      }

      if (output_mode == OUTPUT_MODE_SLOPE_PHASE && ramp_mode == RAMP_MODE_AR) {
        if (ramp) {
          ramp_generator_.Step<ramp_mode, output_mode, range, true>(
              f0, per_channel_pw, stmlib::GATE_FLAG_LOW, ramp[i]);
        } else {
          ramp_generator_.Step<ramp_mode, output_mode, range, false>(
              f0, per_channel_pw, gate_flags[i], 0.0f);
        }
      } else {
        if (ramp) {
          ramp_generator_.Step<ramp_mode, output_mode, range, true>(
              f0, &pw, stmlib::GATE_FLAG_LOW, ramp[i]);
        } else {
          ramp_generator_.Step<ramp_mode, output_mode, range, false>(
              f0, &pw, gate_flags[i], 0.0f);
        }
      }
      
      const float shape = shape_modulation.Next();
      MAKE_INTEGRAL_FRACTIONAL(shape);
      const int16_t* shape_table = &lut_wavetable[shape_integral * 1025];
      
      if (output_mode == OUTPUT_MODE_GATES) {
        const float phase = ramp_generator_.phase(0);
        const float frequency = ramp_generator_.frequency(0);
        const float raw = ramp_shaper_[0].Slope<
              ramp_mode, range>(phase, 0.0f, frequency, pw);
        const float slope = ramp_waveshaper_[0].Shape<
              ramp_mode>(raw, shape_table, shape_fractional);

        out[i].channel[0] = Fold<ramp_mode>(slope, fold) * shift;
        out[i].channel[1] = Scale<ramp_mode>(is_phasor
            ? ramp_waveshaper_[1].Shape<ramp_mode>(
                raw, &lut_wavetable[8200], 0.0f)
            : raw);
        out[i].channel[2] = ramp_shaper_[2].EOA<ramp_mode, range>(
            phase, frequency, pw) * 8.0f;
        out[i].channel[3] = ramp_shaper_[3].EOR<ramp_mode, range>(
            phase, frequency, pw) * 8.0f;
      } else if (output_mode == OUTPUT_MODE_AMPLITUDE) {
        const float phase = ramp_generator_.phase(0);
        const float frequency = ramp_generator_.frequency(0);
        const float raw = ramp_shaper_[0].Slope<
              ramp_mode, range>(phase, 0.0f, frequency, pw);
        const float shaped = ramp_waveshaper_[0].Shape<
              ramp_mode>(raw, shape_table, shape_fractional);
        const float slope = Fold<ramp_mode>(shaped, fold) * \
              (shift < 0.0f ? -1.0f : + 1.0f);
        const float channel_index = fabsf(shift * 5.1f);
        for (size_t j = 0; j < num_channels; ++j) {
          const float channel = static_cast<float>(j + 1);
          const float gain = std::max(
              1.0f - fabsf(channel - channel_index), 0.0f);
          const bool equal_pow = range == RANGE_AUDIO;
          out[i].channel[j] = slope * gain * (equal_pow ? (2.0f - gain) : 1.0f);
        }
      } else if (output_mode == OUTPUT_MODE_SLOPE_PHASE) {
        float phase_shift = 0.0f;
        for (size_t j = 0; j < num_channels; ++j) {
          size_t source = ramp_mode == RAMP_MODE_AR ? j : 0;
          out[i].channel[j] = Fold<ramp_mode>(
              ramp_waveshaper_[j].Shape<ramp_mode>(
                  ramp_shaper_[j].Slope<ramp_mode, range>(
                      ramp_generator_.phase(source),
                      phase_shift, 
                      ramp_generator_.frequency(source),
                      ramp_mode == RAMP_MODE_AD ? per_channel_pw[j] : pw),
                  shape_table,
                  shape_fractional),
              fold);
          phase_shift -= range == RANGE_AUDIO ? step : partial_step;
        }
      } else if (output_mode == OUTPUT_MODE_FREQUENCY) {
        for (size_t j = 0; j < num_channels; ++j) {
          out[i].channel[j] = Fold<ramp_mode>(
              ramp_waveshaper_[j].Shape<ramp_mode>(
                  ramp_shaper_[j].Slope<ramp_mode, range>(
                      ramp_generator_.phase(j),
                      0.0f, 
                      ramp_generator_.frequency(j),
                      pw),
                  shape_table,
                  shape_fractional),
              fold);
        }
      }
    }
  }
  
  template<RampMode ramp_mode, OutputMode output_mode, Range range>
  void IN_RAM RenderInternal_RAM(
      float frequency,
      float pw,
      float shape,
      float smoothness,
      float shift,
      const stmlib::GateFlags* gate_flags,
      const float* ramp,
      OutputSample* out,
      size_t size) {
    RenderInternal<ramp_mode, output_mode, range>(
        frequency, pw, shape, smoothness, shift, gate_flags, ramp, out, size);
  }
  
  // --- Quantum Mode Logic ---
  float GetGateProbability() {
    switch(gate_probability_mode_) {
      case 0: return 1.01f; 
      case 1: return 0.75f;
      case 2: return 0.50f;
      case 3: return 0.25f;
    }
    return 1.0f;
  }
  
  bool CheckSubDivider() {
    sub_clock_counter_++;
    switch(sub_divider_mode_) {
      case 0: return true; // 1:1
      case 1: return (sub_clock_counter_ % 2) == 0; // 1:2
      case 2: return (sub_clock_counter_ % 4) == 0; // 1:4
      case 3: return (stmlib::Random::GetFloat() > 0.5f); 
    }
    return true;
  }

  float Quantize(float voltage, int scale_idx) {
    int semitone = static_cast<int>(std::floor(voltage * 12.0f + 0.5f));
    
    // --- 修复点 2: 负数 Octave 计算 ---
    int octave = semitone >= 0 ? semitone / 12 : (semitone - 11) / 12;
    
    int note = semitone % 12;
    if (note < 0) note += 12; 
    
    const uint16_t kScales[6] = {
      0b111111111111, // Chromatic
      0b101011010101, // Major 
      0b101101011010, // Minor 
      0b101010010100, // Penta Major 
      0b100101010010, // Penta Minor 
      0b100000000000  // Octaves 
    };
    
    uint16_t mask = kScales[scale_idx];
    
    if (!(mask & (1 << note))) {
       for (int k = 1; k <= 6; ++k) {
          int up = (note + k) % 12;
          if (mask & (1 << up)) { note = up; break; }
          int down = (note - k);
          if (down < 0) down += 12;
          if (mask & (1 << down)) { note = down; break; }
       }
    }
    return (float)(octave * 12 + note) / 12.0f;
  }

  void RenderQuantum(
      float frequency,
      float pw,
      float shape,
      float smoothness,
      float shift,
      const stmlib::GateFlags* gate_flags,
      const float* ramp, 
      OutputSample* out,
      size_t size) {
      
    // 1. 锁定内部时钟比率 (Unison)
    ramp_generator_.set_next_ratio(&audio_ratio_table_[10][0]);
    
    // 2.【修复】重置滤波器状态，防止切回原厂模式时产生爆音或信号堵塞
    filter_.Init();

    // 3. 速度调整
    float effective_frequency = frequency; 
    effective_frequency = std::min(effective_frequency, 0.49f);

    float slew_coeff;
    if (smoothness < 0.05f) slew_coeff = 1.0f; 
    else {
      float s = 1.0f - smoothness;
      slew_coeff = 0.001f + s * s * 0.5f; 
    }

    const int kNumScales = 6;
    int scale_index = static_cast<int>(pw * kNumScales * 0.99f);
    if (scale_index >= kNumScales) scale_index = kNumScales - 1;
    if (scale_index < 0) scale_index = 0;

    stmlib::ParameterInterpolator rate(&frequency_, effective_frequency, size);
    stmlib::ParameterInterpolator bias(&shape_, shape, size); 
    stmlib::ParameterInterpolator shift_amt(&shift_, shift, size);

    for (size_t i = 0; i < size; ++i) {
      float current_frequency = rate.Next();
      float mutation_knob = bias.Next(); 
      float current_shift = shift_amt.Next();
      
      bool gate_rising = (gate_flags[i] & stmlib::GATE_FLAG_RISING);
      
      float dummy_pw = 0.5f;
      uint8_t step_flags = gate_rising ? stmlib::GATE_FLAG_RISING : stmlib::GATE_FLAG_LOW;
      
      ramp_generator_.Step<RAMP_MODE_LOOPING, OUTPUT_MODE_AMPLITUDE, RANGE_AUDIO, false>(
          current_frequency, &dummy_pw, step_flags, 0.0f);
      
      // 判定阈值放宽到 0.05f 以适应高频
      bool internal_cycle_complete = false;
      if (ramp_generator_.phase(0) < 0.05f && quantum_trigger_state_ == 1) {
        internal_cycle_complete = true;
        quantum_trigger_state_ = 0;
      } else if (ramp_generator_.phase(0) > 0.5f) {
        quantum_trigger_state_ = 1;
      }
      
      bool clocked = gate_rising || internal_cycle_complete;

      if (clocked) {
        // === 1. Turing Machine Update ===
        if (shift_register_ == 0) shift_register_ = 0x8000; // Anti-lock

        float mutation_prob = 0.0f;
        if (mutation_knob < 0.5f) {
            mutation_prob = mutation_knob * 0.4f; 
        } else {
            mutation_prob = 0.2f + (mutation_knob - 0.5f) * 1.6f; 
        }
        
        uint8_t lsb = shift_register_ & 1;
        bool flip = stmlib::Random::GetFloat() < mutation_prob;
        uint8_t new_bit = flip ? !lsb : lsb;
        shift_register_ = (shift_register_ >> 1) | (new_bit << 31);
        
        uint32_t scrambled = (shift_register_ ^ (shift_register_ >> 8) ^ (shift_register_ >> 16));
        float raw_val = static_cast<float>(scrambled & 0x7F) / 127.0f;
        
        float transposition = (ramp) ? (ramp[i] * 5.0f) : 0.0f;
        float raw_voltage = 2.0f + (raw_val * 4.0f) + transposition; 

        float probability_setting = GetGateProbability(); 

        // === 2. Main Channel ===
        if (stmlib::Random::GetFloat() < probability_setting) {
             quantum_target_[0] = Quantize(raw_voltage, scale_index); 
             quantum_target_[1] = 8.0f; 
        } else {
             quantum_target_[1] = 0.0f; 
        }

        // === 3. Sub Channel ===
        bool divider_pass = CheckSubDivider();
        if (divider_pass) {
             float harmony_offset = current_shift * 2.0f; 
             float potential_sub_cv = Quantize(raw_voltage + harmony_offset, scale_index);
             
             if (stmlib::Random::GetFloat() < probability_setting) {
                 quantum_target_[2] = potential_sub_cv;
                 quantum_target_[3] = 8.0f; 
             } else {
                 quantum_target_[3] = 0.0f; 
             }
        }
        
      } else {
        if (ramp_generator_.phase(0) > 0.5f) {
            quantum_target_[1] = 0.0f;
            quantum_target_[3] = 0.0f;
        }
      }

      for (int ch = 0; ch < 4; ++ch) {
        if (ch == 1 || ch == 3) {
            quantum_current_[ch] = quantum_target_[ch];
        } else {
            float error = quantum_target_[ch] - quantum_current_[ch];
            quantum_current_[ch] += error * slew_coeff;
        }
        out[i].channel[ch] = quantum_current_[ch]; 
      }
    }
    
    // ===【关键修复】还原成员变量 ===
    // 退出前，将成员变量恢复到原厂模式期望的范围。
    // 这能消除切换模式瞬间 ParameterInterpolator 产生的巨大数值跳变。
    
    // 1. 还原 pw
    pw_ = pw;
    
    // 2. 还原 shift (原厂期望 -1.0 到 1.0)
    shift_ = 2.0f * shift - 1.0f;
    
    // 3. 还原 frequency (原厂期望原始低频值)
    // 这样切回原厂时，插值器从正常值开始，而不是从 8 倍频开始跌落
    frequency_ = frequency;
  }

  // Member Variables
  float frequency_;
  float pw_;
  float shift_;
  float shape_;
  float fold_;
  
  stmlib::HysteresisQuantizer2 ratio_index_quantizer_;

  FeatureMode feature_mode_;
  float quantum_current_[num_channels];
  float quantum_target_[num_channels];
  int quantum_trigger_state_;
  
  uint32_t shift_register_;      
  uint32_t sub_clock_counter_;   
  int gate_probability_mode_;    
  int sub_divider_mode_;         

  RampGenerator<num_channels> ramp_generator_;

  RampShaper ramp_shaper_[num_channels];
  RampWaveshaper ramp_waveshaper_[num_channels];
  Filter<num_channels> filter_;
  
  static Ratio audio_ratio_table_[21][num_channels];
  static Ratio control_ratio_table_[21][num_channels];
  static RenderFn render_fn_table_[RAMP_MODE_LAST][OUTPUT_MODE_LAST][
      RANGE_LAST];

  DISALLOW_COPY_AND_ASSIGN(PolySlopeGenerator);
};

}  // namespace tides

#endif  // TIDES_POLY_SLOPE_GENERATOR_H_