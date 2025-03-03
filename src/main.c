// Wokwi Custom SPI Chip for Simulating MFRC522 RFID Reader
// SPDX-License-Identifier: MIT
// Copyright (C) 2022 Uri Shaked / wokwi.com

#include "wokwi-api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Predefined UID (4 bytes for MIFARE Classic 1K)
static const uint8_t uid[] = {0x4B, 0x59, 0x4C, 0x45};
static const size_t uid_length = sizeof(uid) / sizeof(uid[0]);

// ATQA for MIFARE Classic 1K
static const uint8_t atqa[] = {0x04, 0x00};
static const size_t atqa_length = sizeof(atqa) / sizeof(atqa[0]);

// SAK for MIFARE Classic 1K (single UID complete)
static const uint8_t sak = 0x08;

// Simulated register values
static uint8_t com_irq_reg = 0x00; // ComIrqReg
static uint8_t div_irq_reg = 0x00; // DivIrqReg
static uint8_t fifo_level_reg = 0x00; // FIFOLevelReg

typedef struct {
  pin_t    idScanpin;
  pin_t    cs_pin;          // Chip Select pin
  uint32_t spi;             // SPI device handle
  uint8_t  spi_buffer[1];   // SPI buffer for 1-byte transactions
  const uint8_t *uid;       // UID sequence
  size_t   uid_length;
  size_t   uid_index;
  const uint8_t *atqa;      // ATQA sequence
  size_t   atqa_length;
  size_t   atqa_index;
  uint8_t  next_data;       // Data to send in the next transaction
  bool     is_reading_fifo; // Flag for reading FIFODataReg
  bool     card_detected;   // Flag for card detection
  bool     card_selected;   // Flag for card selection
  uint8_t  last_command;    // Last command written to FIFO/CommandReg
} chip_state_t;

static void chip_pin_change(void *user_data, pin_t pin, uint32_t value);
static void chip_spi_done(void *user_data, uint8_t *buffer, uint32_t count);

void input_changed(void *user_data, pin_t pin, uint32_t value) {
  chip_state_t* chip = (chip_state_t*)user_data;
  printf("HELLO - Pin value: %u\n", value);
}

void chip_init(void) {
  chip_state_t *chip = malloc(sizeof(chip_state_t));
  
  chip->cs_pin = pin_init("CS", INPUT_PULLUP);
  chip->uid = uid;
  chip->uid_length = uid_length;
  chip->uid_index = 0;
  chip->atqa = atqa;
  chip->atqa_length = atqa_length;
  chip->atqa_index = 0;
  chip->next_data = 0x00;
  chip->is_reading_fifo = false;
  chip->card_detected = false;
  chip->card_selected = false;
  chip->last_command = 0x00;

  chip->idScanpin = pin_init("ID", INPUT_PULLDOWN);
  const pin_watch_config_t watch_input = {
    .edge = RISING,
    .pin_change = input_changed,
    .user_data = chip,
  };
  bool success = pin_watch(chip->idScanpin, &watch_input);
  printf("Pin id success: %d\n", success);


  const pin_watch_config_t watch_config = {
    .edge = BOTH,
    .pin_change = chip_pin_change,
    .user_data = chip,
  };
  pin_watch(chip->cs_pin, &watch_config);
  


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

static void chip_pin_change(void *user_data, pin_t pin, uint32_t value) {
  chip_state_t *chip = (chip_state_t*)user_data;
  if (pin == chip->cs_pin) {
    if (value == LOW) {
      printf("SPI chip selected\n");
      chip->spi_buffer[0] = chip->next_data;
      spi_start(chip->spi, chip->spi_buffer, sizeof(chip->spi_buffer));
    } else {
      printf("SPI chip deselected\n");
      spi_stop(chip->spi);
      chip->is_reading_fifo = false;
      chip->uid_index = 0;
      chip->atqa_index = 0;
      chip->card_detected = false;
      chip->card_selected = false;
    }
  }
}

static void chip_spi_done(void *user_data, uint8_t *buffer, uint32_t count) {
  chip_state_t *chip = (chip_state_t*)user_data;
  if (count == 1) {
    uint8_t received_byte = buffer[0];
    printf("Received byte: 0x%02X\n", received_byte);

    // Handle SPI commands
    if ((received_byte & 0x80) == 0) { // Write command
      uint8_t reg = (received_byte >> 1) & 0x3F;
      printf("Write to register 0x%02X\n", reg);

      if (reg == 0x09) { // FIFODataReg
        chip->last_command = received_byte; // Store for context
      } else if (reg == 0x01 && chip->last_command == 0x12) { // CommandReg after FIFO write
        if (received_byte == 0x0C) { // PICC_CMD_REQA
          chip->card_detected = true;
          chip->atqa_index = 0;
          com_irq_reg |= 0x20; // Set RxIRq
          fifo_level_reg = 2;  // 2 bytes in FIFO (ATQA)
          printf("REQA command detected, setting RxIRq\n");
        } else if (received_byte == 0x93) { // PICC_CMD_SEL_CL1
          chip->card_selected = true;
          com_irq_reg |= 0x20; // Set RxIRq
          fifo_level_reg = 5;  // UID + SAK
          printf("Card selection detected, setting RxIRq\n");
        }
        chip->last_command = 0x00; // Reset context
      }
      chip->next_data = 0x00; // Default response after write
    } else { // Read command
      uint8_t reg = (received_byte & 0x7E) >> 1;
      switch (reg) {
        case 0x37: // VersionReg (0xEE)
          chip->next_data = 0x91; // v1.0
          printf("Reading VersionReg, sending: 0x%02X\n", chip->next_data);
          break;
        case 0x04: // ComIrqReg (0x88)
          chip->next_data = com_irq_reg;
          printf("Reading ComIrqReg, sending: 0x%02X\n", chip->next_data);
          break;
        case 0x05: // DivIrqReg (0x8A)
          chip->next_data = div_irq_reg;
          printf("Reading DivIrqReg, sending: 0x%02X\n", chip->next_data);
          break;
        case 0x0A: // FIFOLevelReg (0x94)
          chip->next_data = fifo_level_reg;
          printf("Reading FIFOLevelReg, sending: 0x%02X\n", chip->next_data);
          break;
        case 0x09: // FIFODataReg (0x92)
          chip->is_reading_fifo = true;
          if (!chip->card_selected && chip->card_detected) {
            if (chip->atqa_index < chip->atqa_length) {
              chip->next_data = chip->atqa[chip->atqa_index];
              printf("Reading FIFODataReg, sending ATQA byte: 0x%02X\n", chip->next_data);
              chip->atqa_index++;
              if (chip->atqa_index >= chip->atqa_length) {
                com_irq_reg &= ~0x20; // Clear RxIRq after ATQA sent
              }
            } else {
              chip->next_data = 0x00;
            }
          } else if (chip->card_selected) {
            if (chip->uid_index < chip->uid_length) {
              chip->next_data = chip->uid[chip->uid_index];
              printf("Reading FIFODataReg, sending UID byte: 0x%02X\n", chip->next_data);
              chip->uid_index++;
            } else if (chip->uid_index == chip->uid_length) {
              chip->next_data = sak;
              printf("Reading FIFODataReg, sending SAK: 0x%02X\n", chip->next_data);
              chip->uid_index++;
              com_irq_reg &= ~0x20; // Clear RxIRq after SAK sent
            } else {
              chip->next_data = 0x00;
            }
          } else {
            chip->next_data = 0x00;
          }
          break;
        default:
          chip->next_data = 0x00;
          printf("Unknown read command, sending: 0x00\n");
          break;
      }
    }

    // Update buffer for next transaction
    buffer[0] = chip->next_data;

    // Continue SPI transaction if CS is still low
    if (pin_read(chip->cs_pin) == LOW) {
      spi_start(chip->spi, chip->spi_buffer, sizeof(chip->spi_buffer));
    }
  }
}