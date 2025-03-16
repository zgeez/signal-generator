// Wokwi Custom Chip - For docs and examples see:
// https://docs.wokwi.com/chips-api/getting-started
//
// SPDX-License-Identifier: MIT
// Copyright 2023 Ziad Ghanem

#include "wokwi-api.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define PI 3.14159

typedef struct {
  pin_t pin_out;
  uint32_t amplitude_attr;
  uint32_t frequency_attr;
  uint32_t offset_attr;
  uint32_t frequency_exp_attr;
  uint32_t delay_attr;
} chip_state_t;

static void chip_timer_event(void *user_data);

void chip_init(void) {
  chip_state_t *chip = malloc(sizeof(chip_state_t));
  chip->pin_out = pin_init("OUT", ANALOG);
  chip->amplitude_attr = attr_init_float("amplitude", 1.0);
  chip->frequency_attr = attr_init_float("frequency", 1.0);
  chip->offset_attr = attr_init_float("offset", 2.5);
  chip->frequency_exp_attr= attr_init_float("frequency_exp",0.0);
  chip->delay_attr= attr_init_float("delay",0.0);

  const timer_config_t timer_config = {
    .callback = chip_timer_event,
    .user_data = chip,
  };
  timer_t timer_id = timer_init(&timer_config);
  timer_start(timer_id, 100, true);
}

void chip_timer_event(void *user_data) {
  chip_state_t *chip = (chip_state_t*)user_data;
  float amplitude = attr_read_float(chip->amplitude_attr);
  float frequency = attr_read_float(chip->frequency_attr);
  float frequency_exp = attr_read_float(chip->frequency_exp_attr);
  float offset = attr_read_float(chip->offset_attr);
  float delay_degree = attr_read_float(chip->delay_attr);
  float delay_rad = delay_degree/180.0*2.0*PI;

  float t = get_sim_nanos()/1e9;
  float voltage = amplitude*sin(2.0*PI*frequency*pow(10,frequency_exp)*t+delay_rad)+offset;
  
  pin_dac_write(chip->pin_out, voltage);
}
