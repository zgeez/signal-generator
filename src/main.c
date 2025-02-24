// Wokwi Custom Chip - For docs and examples see:
// https://docs.wokwi.com/chips-api/getting-started
//
// SPDX-License-Identifier: MIT
// Copyright 2023 
//test fork

#include "wokwi-api.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct {
  // TODO: Put your chip variables here
  pin_t inpin;
  uart_dev_t uart0;
} chip_state_t;


void input_changed(void *user_data, pin_t pin, uint32_t value) {
  // value will either be HIGH or LOW
  chip_state_t* chip = (chip_state_t*)user_data;
  //ipasa mo ung TUPM code
}

static void on_uart_rx_data(void *user_data, uint8_t byte) {
  chip_state_t *chip = (chip_state_t*)user_data;
  // `byte` is the byte received on the "RX" pin
  printf("Incoming UART data: %d\n", byte);
  const char *data_out = "TUPM-20-2829";

  uart_write(chip->uart0, data_out, sizeof("TUPM-20-2829") - 1);
}

static uint8_t on_uart_write_done(void *user_data) {
  chip_state_t *chip = (chip_state_t*)user_data;
  // You can write the chunk of data to transmit here (by calling uart_write).
  printf("UART done\n");
}



void chip_init() {
  chip_state_t *chip = malloc(sizeof(chip_state_t));

  // pin_t inpin = pin_init("IN", INPUT_PULLDOWN);
  chip->inpin = pin_init("IN", INPUT_PULLDOWN);

  const pin_watch_config_t watch_input = {
    .edge = RISING,
    .pin_change = input_changed,
    .user_data = chip,
  };
  pin_watch(chip->inpin, &watch_input);

  const uart_config_t uart_config = {
    .tx = pin_init("TX", INPUT_PULLUP),
    .rx = pin_init("RX", INPUT),
    .baud_rate = 9600,
    .rx_data = on_uart_rx_data,
    .write_done = on_uart_write_done,
    .user_data = chip,
  };
  chip->uart0 = uart_init(&uart_config);


  // TODO: Initialize the chip, set up IO pins, create timers, etc.

  printf("Hello from custom chip!\n");
}
