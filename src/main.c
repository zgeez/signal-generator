// Wokwi Custom SPI Chip for Simulating MFRC522 RFID Reader
// SPDX-License-Identifier: MIT
// Copyright (C) 2022 Uri Shaked / wokwi.com

#include "wokwi-api.h"
#include <stdio.h>
#include <stdlib.h>

// Predefined UID to simulate an RFID tag (7 bytes)
static const uint8_t uid[] = {0x4B, 0x59, 0x4C, 0x45, 0x20, 0x4B, 0x00};
static const size_t uid_length = sizeof(uid) / sizeof(uid[0]);

typedef struct {
  pin_t    cs_pin;          // Chip Select pin
  uint32_t spi;             // SPI device handle
  uint8_t  spi_buffer[1];   // SPI buffer for 1-byte transactions
  const uint8_t *uid;       // Pointer to UID sequence
  size_t   uid_length;      // Length of UID sequence
  size_t   uid_index;       // Index of next UID byte to send
  uint8_t  next_data;       // Data to send in the next transaction
  bool     is_reading_fifo; // Flag to indicate if reading from FIFODataReg
} chip_state_t;

// Function prototypes
static void chip_pin_change(void *user_data, pin_t pin, uint32_t value);
static void chip_spi_done(void *user_data, uint8_t *buffer, uint32_t count);

// Initialize the custom chip
void chip_init(void) {
  chip_state_t *chip = malloc(sizeof(chip_state_t));
  
  chip->cs_pin = pin_init("CS", INPUT_PULLUP);
  chip->uid = uid;
  chip->uid_length = uid_length;
  chip->uid_index = 0;
  chip->next_data = 0x00;
  chip->is_reading_fifo = false;

  // Watch CS pin for changes
  const pin_watch_config_t watch_config = {
    .edge = BOTH,
    .pin_change = chip_pin_change,
    .user_data = chip,
  };
  pin_watch(chip->cs_pin, &watch_config);

  // Initialize SPI
  const spi_config_t spi_config = {
    .sck = pin_init("SCK", INPUT),
    .miso = pin_init("MISO", OUTPUT),
    .mosi = pin_init("MOSI", INPUT),
    .mode = 0,
    .done = chip_spi_done,
    .user_data = chip,
  };
  chip->spi = spi_init(&spi_config);
  
  printf("SPI Chip initialized!\n");
}

// Handle CS pin changes
static void chip_pin_change(void *user_data, pin_t pin, uint32_t value) {
  chip_state_t *chip = (chip_state_t*)user_data;
  if (pin == chip->cs_pin) {
    if (value == LOW) {
      printf("SPI chip selected\n");
      // Start SPI transaction with the next data
      chip->spi_buffer[0] = chip->next_data;
      spi_start(chip->spi, chip->spi_buffer, sizeof(chip->spi_buffer));
    } else {
      printf("SPI chip deselected\n");
      spi_stop(chip->spi);
      // Reset reading flag and UID index when CS goes high
      chip->is_reading_fifo = false;
      chip->uid_index = 0;
    }
  }
}

// Handle completed SPI transactions
static void chip_spi_done(void *user_data, uint8_t *buffer, uint32_t count) {
  chip_state_t *chip = (chip_state_t*)user_data;
  if (count == 1) {
    uint8_t received_byte = buffer[0];
    printf("received byte: %d", received_byte);

    // Interpret the command byte
    if (received_byte == 0x92) { // Read FIFODataReg (0x09): (0x09 << 1) | 0x80 = 0x92
      chip->is_reading_fifo = true;
      chip->uid_index = 0; // Start sending UID from the beginning
    } else if (received_byte == 0x88) { // Example: Read VersionReg (0x44): (0x44 << 1) | 0x80 = 0x88
      chip->next_data = 0x1B; // MFRC522 version (example response)
      printf("if byte 0x88, sending: %d\n", chip->next_data);
    }

    // Prepare the next byte to send
    if (chip->is_reading_fifo && chip->uid_index < chip->uid_length) {
      chip->next_data = chip->uid[chip->uid_index];
      printf("if fifo true, sending: %d\n", chip->next_data);
      chip->uid_index++;
    } else if (received_byte != 0x88) {
      chip->next_data = 0x00; // Default response for other commands
      printf("if fifo not true, sending: %d\n", chip->next_data);
    }

    // Update buffer for the next transaction
    buffer[0] = chip->next_data;

    // Continue SPI transaction if CS is still low
    if (pin_read(chip->cs_pin) == LOW) {
      spi_start(chip->spi, chip->spi_buffer, sizeof(chip->spi_buffer));
    }
  }
}