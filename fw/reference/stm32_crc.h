#ifndef STM32_CRC_H
#define STM32_CRC_H

#include <stdint.h>
#include <stddef.h>

// Initial CRC value for STM32 hardware CRC
#define STM32_CRC_INITIAL_VALUE 0xFFFFFFFF

/**
 * @brief Updates a CRC value with a buffer of data using a precomputed table.
 *        This function should be used for subsequent chunks of data.
 * @param crc The initial or previous CRC value.
 * @param data Pointer to the data buffer.
 * @param len Length of the data buffer in bytes.
 * @return The updated CRC32 value.
 */
uint32_t stm32_crc32_update(uint32_t crc, const uint8_t *data, size_t len);

/**
 * @brief Calculates the CRC of a buffer of data from scratch.
 *        This function should be used for the first chunk of data.
 * @param data Pointer to the data buffer.
 * @param len Length of the data buffer in bytes.
 * @return The calculated CRC32 value.
 */
uint32_t stm32_crc32_calculate(const uint8_t *data, size_t len);

#endif // STM32_CRC_H
