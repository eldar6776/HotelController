#include "stm32_crc.h"

#define CRC32_POLYNOMIAL 0x04C11DB7

/**
 * @brief Updates the CRC by processing a single 32-bit word, mimicking the hardware.
 */
static uint32_t crc_update_word(uint32_t crc, uint32_t word) {
    crc ^= word;
    for (int i = 0; i < 32; i++) {
        if (crc & 0x80000000) {
            crc = (crc << 1) ^ CRC32_POLYNOMIAL;
        } else {
            crc <<= 1;
        }
    }
    return crc;
}

/**
 * @brief Updates a CRC value with a buffer of data.
 * This implementation correctly simulates the STM32 hardware behavior where
 * each byte written to the DR register is processed as a 32-bit word.
 */
uint32_t stm32_crc32_update(uint32_t crc, const uint8_t *data, size_t len) {
    while (len--) {
        // The STM32 CRC hardware processes each byte as a 32-bit word.
        crc = crc_update_word(crc, (uint32_t)*data++);
    }
    return crc;
}

/**
 * @brief Calculates the CRC of a buffer from scratch.
 */
uint32_t stm32_crc32_calculate(const uint8_t *data, size_t len) {
    uint32_t crc = STM32_CRC_INITIAL_VALUE;
    return stm32_crc32_update(crc, data, len);
}
