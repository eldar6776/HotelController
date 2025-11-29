#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "stm32_crc.h"

int main() {
    const char *filename = "101_1.raw";
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Error opening file");
        return 1;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size < 1) {
        fprintf(stderr, "File is empty or could not get size.\n");
        fclose(file);
        return 1;
    }

    uint8_t *buffer = (uint8_t *)malloc(file_size);
    if (!buffer) {
        fprintf(stderr, "Failed to allocate memory.\n");
        fclose(file);
        return 1;
    }

    if (fread(buffer, 1, file_size, file) != file_size) {
        fprintf(stderr, "Failed to read file.\n");
        free(buffer);
        fclose(file);
        return 1;
    }
    fclose(file);

    uint32_t crc = STM32_CRC_INITIAL_VALUE;
    const size_t chunk_size = 512;
    size_t bytes_processed = 0;

    // --- Process chunk by chunk and print intermediate CRC values ---

    // 1. Process first chunk
    if (bytes_processed + chunk_size <= file_size) {
        crc = stm32_crc32_update(crc, buffer + bytes_processed, chunk_size);
        printf("CRC after first 512 bytes: 0x%08X\n", crc);
        bytes_processed += chunk_size;
    }

    // 2. Process second chunk
    if (bytes_processed + chunk_size <= file_size) {
        crc = stm32_crc32_update(crc, buffer + bytes_processed, chunk_size);
        printf("CRC after second 512 bytes: 0x%08X\n", crc);
        bytes_processed += chunk_size;
    }

    // 3. Process the rest of the file
    if (bytes_processed < file_size) {
        size_t remaining = file_size - bytes_processed;
        crc = stm32_crc32_update(crc, buffer + bytes_processed, remaining);
    }
    
    printf("Final CRC: 0x%08X\n", crc);

    free(buffer);
    return 0;
}