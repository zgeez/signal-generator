// Wokwi Custom SPI Chip Example
// SPDX-License-Identifier: MIT
// Copyright (C) 2022 Uri Shaked / wokwi.com

#include "wokwi-api.h"
#include <stdio.h>
#include <stdlib.h>

// Define the sequence of hexadecimal data to send (simulated UID)
static const uint8_t data_sequence[] = {0x4B, 0x59, 0x4C, 0x45, 0x20, 0x4B, 0x00};
static const size_t data_length = sizeof(data_sequence) / sizeof(data_sequence[0]);

typedef struct {
  pin_t    cs_pin;           // Chip Select pin
  uint32_t spi;              // SPI device handle
  uint8_t  spi_buffer[1];    // Buffer for SPI transactions (1 byte)
  const uint8_t *data_sequence;  // Pointer to the data sequence
  size_t   data_length;      // Length of the data sequence
  size_t   uid_index;        // Current position in the sequence
} chip_state_t;

static void chip_pin_change(void *user_data, pin_t pin, uint32_t value);
static void chip_spi_done(void *user_data, uint8_t *buffer, uint32_t count);

void chip_init(void) {
  chip_state_t *chip = malloc(sizeof(chip_state_t));
  
  chip->cs_pin = pin_init("CS", INPUT_PULLUP);
  chip->data_sequence = data_sequence;
  chip->data_length = data_length;
  chip->uid_index = 0;

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
    .mode = 0,
    .done = chip_spi_done,
    .user_data = chip,
  };
  chip->spi = spi_init(&spi_config);
  
  printf("SPI Chip initialized!\n");
}

void chip_pin_change(void *user_data, pin_t pin, uint32_t value) {
  chip_state_t *chip = (chip_state_t*)user_data;
  if (pin == chip->cs_pin) {
    if (value == LOW) {
      printf("SPI chip selected\n");
      chip->uid_index = 0;
      // Start the first SPI transaction with the first byte if available
      if (chip->data_length > 0) {
        chip->spi_buffer[0] = chip->data_sequence[0];
      } else {
        chip->spi_buffer[0] = 0x00; // Default if no data
      }
      spi_start(chip->spi, chip->spi_buffer, sizeof(chip->spi_buffer));
    } else {
      printf("SPI chip deselected\n");
      spi_stop(chip->spi);
      chip->uid_index = 0; // Reset index for next CS LOW
    }
  }
}

void chip_spi_done(void *user_data, uint8_t *buffer, uint32_t count) {
  chip_state_t *chip = (chip_state_t*)user_data;
  if (count == 1) {
    // Increment index after the current byte has been sent
    chip->uid_index++;
    // Prepare the next byte
    if (chip->uid_index < chip->data_length) {
      buffer[0] = chip->data_sequence[chip->uid_index];
    } else {
      buffer[0] = 0x00; // Send 0x00 when no more data
    }
    // Continue transaction if CS is still LOW
    if (pin_read(chip->cs_pin) == LOW) {
      spi_start(chip->spi, chip->spi_buffer, sizeof(chip->spi_buffer));
    }
  }
}