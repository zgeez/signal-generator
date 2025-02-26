// Wokwi Custom SPI Chip Example
//
// This chip implements a simple ROT13 letter substitution cipher.
// It receives a string over SPI, and returns the same string with
// each alphabetic character replaced with its ROT13 substitution.
//
// For information and examples see:
// https://link.wokwi.com/custom-chips-alpha
//
// SPDX-License-Identifier: MIT
// Copyright (C) 2022 Uri Shaked / wokwi.com



//need ko burahin ung rot13
//gumawa ng lalagyan ng hexadecimal code

#include "wokwi-api.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct {
  pin_t    cs_pin;
  uint32_t spi;
  uint8_t  spi_buffer[1];
} chip_state_t;

static void chip_pin_change(void *user_data, pin_t pin, uint32_t value);
static void chip_spi_done(void *user_data, uint8_t *buffer, uint32_t count);

void chip_init(void) {
  chip_state_t *chip = malloc(sizeof(chip_state_t));
  
  chip->cs_pin = pin_init("CS", INPUT_PULLUP);

  const pin_watch_config_t watch_config = {
    .edge = BOTH,
    .pin_change = chip_pin_change,
    .user_data = chip,
  };
  pin_watch(chip->cs_pin, &watch_config);

  const spi_config_t spi_config = {
    .sck = pin_init("SCK", INPUT),
    .miso = pin_init("MISO", INPUT),
    .mosi = pin_init("MOSI", INPUT),
    .done = chip_spi_done,
    .user_data = chip,
  };
  chip->spi = spi_init(&spi_config);
  
  printf("SPI Chip initialized!\n");

}


void chip_pin_change(void *user_data, pin_t pin, uint32_t value) {
  chip_state_t *chip = (chip_state_t*)user_data;
  // Handle CS pin logic
  if (pin == chip->cs_pin) {
    if (value == LOW) {
      printf("SPI chip selected\n");
      chip->spi_buffer[0] = ' '; // Some dummy data for the first character
      spi_start(chip->spi, chip->spi_buffer, sizeof(chip->spi_buffer));
    } else {
      printf("SPI chip deselected\n");
      spi_stop(chip->spi);
    }
  }
}

void chip_spi_done(void *user_data, uint8_t *buffer, uint32_t count) {
  chip_state_t *chip = (chip_state_t*)user_data;
  if (!count) {
    // This means that we got here from spi_stop, and no data was received
    return;
  }

  // Apply the ROT13 transformation, and store the result in the buffer.
  // The result will be read back during the next SPI transfer.
  //sending data happens here
  buffer[0] = 0x4B;

  if (pin_read(chip->cs_pin) == LOW) {
    // Continue with the next character
    spi_start(chip->spi, chip->spi_buffer, sizeof(chip->spi_buffer));
  }
}
