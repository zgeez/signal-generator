// Wokwi Custom SPI Chip Example
//
// For information and examples see:
// https://link.wokwi.com/custom-chips-alpha
//
// (c) 2025, James Balolong

#include "wokwi-api.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct {
  pin_t    cs_pin;           // Chip Select pin
  pin_t    idScanpin;        // Button pin
  uint32_t spi;              // SPI device handle
  uint32_t hexGen_attr;      // Attribute handle for "ID"
  uint8_t  spi_buffer[1];    // Buffer for SPI transactions
  uint8_t  uid[4];           // Array to hold the generated UID
  uint8_t  zero_sequence[4]; // Array to hold {0, 0, 0, 0}
  const uint8_t *data_sequence;  // Pointer to the data sequence (UID or zero_sequence)
  size_t   data_length;      // Length of the data sequence
  size_t   current_index;    // Current position in the sequence
} chip_state_t;

static void chip_pin_change(void *user_data, pin_t pin, uint32_t value);
static void chip_spi_done(void *user_data, uint8_t *buffer, uint32_t count);
static void start_sending(chip_state_t *chip);
static void id_pin_change(void *user_data, pin_t pin, uint32_t value);

void input_changed(void *user_data, pin_t pin, uint32_t value) {
  chip_state_t *chip = (chip_state_t*)user_data;
  printf("Button pressed - setting data_sequence to UID\n");

  uint32_t id_value = attr_read(chip->hexGen_attr);

  printf("id val: %d\n", id_value);

  uint32_t seed = id_value;
  for (int i = 0; i < 4; i++) {
    seed = (seed * 1103515245 + 12345) & 0xFFFFFFFF;
    chip->uid[i] = (seed >> 16) & 0xFF;
  }

  // Generate a random 4-byte UID
  printf("SPI Chip GENERATED! UID: %02X %02X %02X %02X\n",
         chip->uid[0], chip->uid[1], chip->uid[2], chip->uid[3]);

  chip->data_sequence = chip->uid;
}

void input_falling(void *user_data, pin_t pin, uint32_t value) {
  chip_state_t *chip = (chip_state_t*)user_data;
  printf("Button released - setting data_sequence to zeros\n");
  chip->data_sequence = chip->zero_sequence;
}

void chip_init(void) {
  chip_state_t *chip = malloc(sizeof(chip_state_t));
  
  // Initialize Chip Select pin
  chip->cs_pin = pin_init("CS", INPUT_PULLUP);

  // Initialize button for ID
  chip->idScanpin = pin_init("ID", INPUT_PULLDOWN);

  // Configure a single pin watch for ID pin with BOTH edges
  const pin_watch_config_t watch_id = {
    .edge = BOTH,
    .pin_change = id_pin_change,
    .user_data = chip,
  };
  bool success = pin_watch(chip->idScanpin, &watch_id);
  printf("Pin id watch success: %d\n", success);
  
  // Initialize the "HexGen" attribute with default value 1
  chip->hexGen_attr = attr_init("HexGen", 1);

  // Read the current value of hexGen_attr
  uint32_t id_value = attr_read(chip->hexGen_attr);
  
  // Seed the random number generator with id_value
  srand(id_value);
  
  // Generate a random 4-byte UID
  for (int i = 0; i < 4; i++) {
    chip->uid[i] = rand() % 256;  // Values from 0 to 255 for each byte
  }
  
  
  // Initialize zero_sequence
  for (int i = 0; i < 4; i++) {
    chip->zero_sequence[i] = 0;
  }
  
  // Set the initial data sequence to zero_sequence
  chip->data_sequence = chip->zero_sequence;
  chip->data_length = 4;      // Sequence is 4 bytes
  chip->current_index = 0;    // Start at the beginning

  // Configure pin watching for CS
  const pin_watch_config_t watch_config = {
    .edge = BOTH,
    .pin_change = chip_pin_change,
    .user_data = chip,
  };
  pin_watch(chip->cs_pin, &watch_config);

  // Configure SPI
  const spi_config_t spi_config = {
    .sck = pin_init("SCK", INPUT),
    .miso = pin_init("MISO", INPUT),
    .mosi = pin_init("MOSI", INPUT),
    .done = chip_spi_done,
    .user_data = chip,
  };
  chip->spi = spi_init(&spi_config);
  
  printf("SPI Chip initialized! UID: %02X %02X %02X %02X\n",
         chip->uid[0], chip->uid[1], chip->uid[2], chip->uid[3]);
}

static void id_pin_change(void *user_data, pin_t pin, uint32_t value) {
  if (value == 1) {
    input_changed(user_data, pin, value);  // Rising edge
  } else {
    input_falling(user_data, pin, value);  // Falling edge
  }
}

static void start_sending(chip_state_t *chip) {
  chip->current_index = 0;
  if (chip->current_index < chip->data_length) {
    chip->spi_buffer[0] = chip->data_sequence[chip->current_index];
    spi_start(chip->spi, chip->spi_buffer, sizeof(chip->spi_buffer));
  }
}

void chip_pin_change(void *user_data, pin_t pin, uint32_t value) {
  chip_state_t *chip = (chip_state_t*)user_data;
  if (pin == chip->cs_pin) {
    if (value == LOW) {
      // printf("SPI chip selected - starting to send data\n");
      start_sending(chip);
    } else {
      // printf("SPI chip deselected\n");
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
    chip->data_sequence = chip->zero_sequence;
    // printf("Sequence complete\n");
  }
}