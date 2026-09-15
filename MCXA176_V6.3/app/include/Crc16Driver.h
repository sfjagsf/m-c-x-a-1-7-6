/* CRC0-backed CRC-16/MODBUS service. */
#ifndef APP_CRC16_DRIVER_H_
#define APP_CRC16_DRIVER_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CRC16_MODBUS_INITIAL_VALUE (0xFFFFU)
#define CRC16_MODBUS_CHECK_VALUE   (0x4B37U)
#define CRC16_MODBUS_MAX_DATA_SIZE (254U)
/* Small frames are faster through the re-entrant flash-resident lookup table. */
#define CRC16_MODBUS_SOFTWARE_THRESHOLD (16U)

/*
 * Gives this module ownership of CRC0 and selects CRC-16/MODBUS parameters.
 * BOARD_InitBootPeripherals() must be called first.
 */
void Crc16Driver_Init(void);

/*
 * Calculates CRC-16/MODBUS over data[0..length-1]. Empty input is valid and
 * produces 0xFFFF. Returns false for invalid pointers or an oversized payload.
 */
bool Crc16Driver_Calculate(const uint8_t *data, size_t length, uint16_t *result);

/* Always uses the re-entrant software lookup-table implementation. */
bool Crc16Driver_CalculateSoftware(const uint8_t *data, size_t length, uint16_t *result);

/*
 * Verifies a frame whose final two bytes are Modbus wire order: CRC low byte,
 * then CRC high byte. The frame must contain at least the two CRC bytes.
 */
bool Crc16Driver_VerifyFrame(const uint8_t *frame, size_t frameLength);

/* Standard check vector: ASCII "123456789" must produce 0x4B37. */
bool Crc16Driver_SelfTest(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_CRC16_DRIVER_H_ */
