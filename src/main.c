// Wokwi Custom SPI Chip Example
//
// For information and examples see:
// https://link.wokwi.com/custom-chips-alpha
//
// SPDX-License-Identifier: MIT

#include "wokwi-api.h"
#include <stdio.h>
#include <stdlib.h>

// Define the sequence of hexadecimal data to send
static const uint8_t data_sequence[] = {0x4B, 0x59, 0x4C, 0x45, 0x20, 0x4B, 0x00};
static const size_t data_length = sizeof(data_sequence) / sizeof(data_sequence[0]);

typedef struct {
  pin_t    cs_pin;           // Chip Select pin
  uint32_t spi;              // SPI device handle
  uint8_t  spi_buffer[1];    // Buffer for SPI transactions
  const uint8_t *data_sequence;  // Pointer to the data sequence
  size_t   data_length;      // Length of the data sequence
  size_t   current_index;    // Current position in the sequence
} chip_state_t;

static void chip_pin_change(void *user_data, pin_t pin, uint32_t value);
static void chip_spi_done(void *user_data, uint8_t *buffer, uint32_t count);

void chip_init(void) {
  chip_state_t *chip = malloc(sizeof(chip_state_t));
  
  chip->cs_pin = pin_init("CS", INPUT_PULLUP);
  chip->data_sequence = data_sequence;  // Assign the sequence
  chip->data_length = data_length;      // Set the sequence length
  chip->current_index = 0;              // Start at the beginning

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
  if (pin == chip->cs_pin) {
    if (value == LOW) {
      printf("SPI chip selected\n");
      // Reset to the start of the sequence
      chip->current_index = 0;
      if (chip->current_index < chip->data_length) {
        chip->spi_buffer[0] = chip->data_sequence[chip->current_index];
        spi_start(chip->spi, chip->spi_buffer, sizeof(chip->spi_buffer));
      }
    } else {
      printf("SPI chip deselected\n");
      spi_stop(chip->spi);
    }
  }
}

void chip_spi_done(void *user_data, uint8_t *buffer, uint32_t count) {
  chip_state_t *chip = (chip_state_t*)user_data;
  if (!count) {
    // spi_stop was called, no data transferred
    return;
  }

  // Move to the next byte in the sequence
  chip->current_index++;
  if (chip->current_index < chip->data_length) {
    // Update the buffer with the next byte
    buffer[0] = chip->data_sequence[chip->current_index];
    if (pin_read(chip->cs_pin) == LOW) {
      // Continue sending the next byte
      spi_start(chip->spi, chip->spi_buffer, sizeof(chip->spi_buffer));
    }
  } else {
    // Sequence is complete
    printf("Sequence complete\n");
  }
}