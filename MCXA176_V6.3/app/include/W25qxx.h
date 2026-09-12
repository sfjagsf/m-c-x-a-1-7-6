#ifndef APP_INCLUDE_W25QXX_H_
#define APP_INCLUDE_W25QXX_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WINBOND_SERIAL_FLASH   (0xEFU)
#define NOR_FLASH_SECTOR_SIZE  (4096UL)
#define NOR_FLASH_BLOCK_32K    (32768UL)
#define NOR_FLASH_BLOCK_64K    (65536UL)
#define FLASH_UNIQUEID_BYTE_SIZE (8U)

typedef struct
{
    uint8_t MF;
    uint8_t ID;
    bool ADS;
    uint8_t uniqueID[FLASH_UNIQUEID_BYTE_SIZE];
    uint8_t mode;
    uint8_t status;
} W25Q_Information;

#define FLASH_SPI_MODE       (0U)
#define FLASH_INIT_SUCCESS   (0U)
#define FLASH_INIT_NO_EXITS  (3U)

bool W25qxxInit(uint8_t mode);
uint16_t W25qxxReadID(void);
bool W25qxxReadJedecId(uint8_t id[3]);
uint8_t W25qxxReadSR(uint32_t reg);
void W25qxxWriteEnable(void);
bool W25qxxWaitBusy(uint32_t timeoutMs);
bool W25qxxRead(uint32_t address, uint8_t *data, uint32_t size);
bool W25qxxPageProgram(uint32_t address, const uint8_t *data, uint32_t size);
bool W25qxxEraseSector(uint32_t address);
bool W25qxxEraseBlock(uint32_t address, bool erase64K);
const W25Q_Information *W25qxxGetInformation(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_INCLUDE_W25QXX_H_ */
