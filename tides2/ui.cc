// Copyright 2017 Emilie Gillet.
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
// User interface.

#include "tides2/ui.h"

#include <algorithm>

#include "stmlib/system/system_clock.h"

#include "tides2/factory_test.h"

using namespace std;
using namespace stmlib;

const int32_t kLongPressDuration = 1200;

namespace tides {

/* static */
const LedColor Ui::palette_[4] = {
  LED_COLOR_GREEN,
  LED_COLOR_YELLOW,
  LED_COLOR_RED,
  LED_COLOR_OFF
};

// Modified Init to accept generator pointer
void Ui::Init(Settings* settings, PolySlopeGenerator* generator, FactoryTest* factory_test) {
  leds_.Init();
  switches_.Init();
  
  system_clock.Init();
  
  settings_ = settings;
  generator_ = generator;
  factory_test_ = factory_test;
  mode_ = UI_MODE_NORMAL;
  feature_mode_ = PolySlopeGenerator::FEATURE_MODE_TIDES;
  
  if (switches_.pressed_immediate(SWITCH_SHIFT)) {
    State* state = settings_->mutable_state();
    if (state->color_blind == 1) {
      state->color_blind = 0;
    } else {
      state->color_blind = 1;
    }
    settings_->SaveState();
  }
  
  queue_.Init();
  
  fill(&press_time_[0], &press_time_[SWITCH_LAST], 0);
  fill(&ignore_release_[0], &ignore_release_[SWITCH_LAST], false);
}

void Ui::Poll() {
  system_clock.Tick();
  UpdateLEDs();
  
  switches_.Debounce();
  
  for (int i = 0; i < SWITCH_LAST; ++i) {
    Switch s = Switch(i);
    if (switches_.just_pressed(s)) {
      queue_.AddEvent(CONTROL_SWITCH, i, 0);
      press_time_[i] = system_clock.milliseconds();
      ignore_release_[i] = false;
    }
    if (switches_.pressed(s) && !ignore_release_[i]) {
      int32_t pressed_time = system_clock.milliseconds() - press_time_[i];
      if (pressed_time > kLongPressDuration) {
        queue_.AddEvent(CONTROL_SWITCH, i, pressed_time);
        ignore_release_[i] = true;
      }
    }
    if (switches_.released(s) && !ignore_release_[i]) {
      queue_.AddEvent(
          CONTROL_SWITCH,
          i,
          system_clock.milliseconds() - press_time_[i] + 1);
      ignore_release_[i] = true;
    }
  }
}

LedColor Ui::MakeColor(uint8_t value, bool color_blind) {
  LedColor color = palette_[value];
  if (color_blind) {
    uint8_t pwm_counter = system_clock.milliseconds() & 15;
    uint8_t triangle = (system_clock.milliseconds() >> 5) & 31;
    triangle = triangle < 16 ? triangle : 31 - triangle;

    if (value == 0) {
      color = pwm_counter < (4 + (triangle >> 2))
          ? LED_COLOR_GREEN
          : LED_COLOR_OFF;
    } else if (value == 1) {
      color = LED_COLOR_YELLOW;
    } else if (value == 2) {
      color = pwm_counter == 0 ? LED_COLOR_RED : LED_COLOR_OFF;
    }
  }
  return color;
}

void Ui::UpdateLEDs() {
  leds_.Clear();
  
  bool blink = system_clock.milliseconds() & 256;
  
  switch (mode_) {
    case UI_MODE_NORMAL:
      {
        const State& s = settings_->state();
        bool color_blind = s.color_blind == 1;

        if (feature_mode_ == PolySlopeGenerator::FEATURE_MODE_QUANTUM) {
          // --- Quantum Mode LED Scheme ---
          int sub_div = generator_->sub_divider_mode();
          LedColor col_top = LED_COLOR_OFF;
          if (sub_div == 1) col_top = LED_COLOR_GREEN;
          else if (sub_div == 2) col_top = LED_COLOR_YELLOW;
          else if (sub_div == 3) col_top = LED_COLOR_RED;
          leds_.set(LED_RANGE, col_top);

          int gate_prob = generator_->gate_probability_mode();
          LedColor col_mid = LED_COLOR_OFF;
          if (gate_prob == 1) col_mid = LED_COLOR_GREEN;
          else if (gate_prob == 2) col_mid = LED_COLOR_YELLOW;
          else if (gate_prob == 3) col_mid = LED_COLOR_RED;
          leds_.set(LED_MODE, col_mid);

          leds_.set(LED_SHIFT, blink ? LED_COLOR_RED : LED_COLOR_GREEN);
          
        } else {
          // --- Normal Tides LED Scheme ---
          leds_.set(LED_MODE, MakeColor(s.mode, color_blind));
          leds_.set(LED_RANGE, MakeColor(s.range, color_blind));
          leds_.set(LED_SHIFT, MakeColor((s.output_mode + 3) % 4, color_blind));
        }
      }
      break;
      
  case UI_MODE_CALIBRATION_C1:
    leds_.set(LED_RANGE, blink ? LED_COLOR_YELLOW : LED_COLOR_OFF);
    break;
      
  case UI_MODE_CALIBRATION_C3:
    leds_.set(LED_SHIFT, blink ? LED_COLOR_YELLOW : LED_COLOR_OFF);
    break;
    
  case UI_MODE_FACTORY_TEST:
    {
      size_t counter = (system_clock.milliseconds() >> 8) % 3;
      for (size_t i = 0; i < 3; ++i) {
        leds_.set(Led(i), palette_[counter]);
      }
    }
    break;
  }
  leds_.Write();
}

void Ui::OnSwitchPressed(const Event& e) {
  // Empty
}

void Ui::OnSwitchReleased(const Event& e) {
  if (mode_ == UI_MODE_NORMAL) {
    if (e.data >= kLongPressDuration) {
      // --- Long Press Handlers ---
      
      // Original Calib Combo (Keep this active regardless of mode for safety)
      if ((e.control_id == SWITCH_RANGE && switches_.pressed(SWITCH_SHIFT)) ||
          (e.control_id == SWITCH_SHIFT && switches_.pressed(SWITCH_RANGE))) {
        mode_ = UI_MODE_CALIBRATION_C1;
        factory_test_->Calibrate(0, 1.0f, 3.0f);
        ignore_release_[SWITCH_RANGE] = ignore_release_[SWITCH_SHIFT] = true;
      }
      // NEW: Long Press (Output/Shift) -> Toggle Quantum Mode
      else if (e.control_id == SWITCH_SHIFT && !switches_.pressed(SWITCH_RANGE)) {
        if (feature_mode_ == PolySlopeGenerator::FEATURE_MODE_TIDES) {
          feature_mode_ = PolySlopeGenerator::FEATURE_MODE_QUANTUM;
        } else {
          feature_mode_ = PolySlopeGenerator::FEATURE_MODE_TIDES;
        }
        generator_->set_feature_mode(feature_mode_);
        ignore_release_[SWITCH_SHIFT] = true; 
      }
    } else {
      // --- Short Press Handlers ---
      
      // CRITICAL FIX: IF/ELSE structure to prevent Fall-Through to Standard Logic
      if (feature_mode_ == PolySlopeGenerator::FEATURE_MODE_QUANTUM) {
          // --- Quantum Mode Controls ---
          switch (e.control_id) {
            case SWITCH_RANGE: // Freq Button (Top)
              {
                int m = generator_->sub_divider_mode();
                m = (m + 1) % 4;
                generator_->set_sub_divider_mode(m);
              }
              break;
            case SWITCH_MODE: // Ramp Button (Mid)
              {
                int m = generator_->gate_probability_mode();
                m = (m + 1) % 4;
                generator_->set_gate_probability_mode(m);
              }
              break;
            case SWITCH_SHIFT: // Output Button (Bottom)
              // Do nothing on short press in Quantum Mode
              break;
          }
      } else {
          // --- Standard Tides Controls (Only runs when NOT in Quantum) ---
          State* s = settings_->mutable_state();
          switch (e.control_id) {
            case SWITCH_MODE: s->mode = (s->mode + 1) % 3; break;
            case SWITCH_RANGE: s->range = (s->range + 1) % 3; break;
            case SWITCH_SHIFT: s->output_mode = (s->output_mode + 1) % 4; break;
          }
          settings_->SaveState();
      }
    }
  } else if (mode_ == UI_MODE_CALIBRATION_C1) {
    factory_test_->Calibrate(1, 1.0f, 3.0f);
    mode_ = UI_MODE_CALIBRATION_C3;
  } else if (mode_ == UI_MODE_CALIBRATION_C3) {
    factory_test_->Calibrate(2, 1.0f, 3.0f);
    mode_ = UI_MODE_NORMAL;
  }
}

void Ui::DoEvents() {
  while (queue_.available()) {
    Event e = queue_.PullEvent();
    if (e.control_type == CONTROL_SWITCH) {
      if (e.data == 0) {
        OnSwitchPressed(e);
      } else {
        OnSwitchReleased(e);
      }
    }
  }
  
  if (queue_.idle_time() > 1000) {
    queue_.Touch();
  }
}

}  // namespace tides