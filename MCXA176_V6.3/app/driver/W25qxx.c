#include "W25qxx.h"

#include <string.h>

#include "MySpi.h"
#include "fsl_common.h"

#define W25Q_CMD_WRITE_ENABLE       (0x06U)
#define W25Q_CMD_READ_STATUS_1      (0x05U)
#define W25Q_CMD_WRITE_STATUS_1     (0x01U)
#define W25Q_CMD_READ_STATUS_2      (0x35U)
#define W25Q_CMD_READ_STATUS_3      (0x15U)
#define W25Q_CMD_PAGE_PROGRAM       (0x02U)
#define W25Q_CMD_FAST_READ          (0x0BU)
#define W25Q_CMD_SECTOR_ERASE       (0x20U)
#define W25Q_CMD_BLOCK_ERASE_32K    (0x52U)
#define W25Q_CMD_BLOCK_ERASE_64K    (0xD8U)
#define W25Q_CMD_MANUFACTURER_ID    (0x90U)
#define W25Q_CMD_JEDEC_ID           (0x9FU)
#define W25Q_CMD_READ_UNIQUE_ID     (0x4BU)
#define W25Q_CMD_ENABLE_RESET       (0x66U)
#define W25Q_CMD_RESET_DEVICE       (0x99U)
#define W25Q_CMD_CHIP_ERASE         (0xC7U)
#define W25Q_CMD_WRITE_DISABLE      (0x04U)
#define W25Q_CMD_ENTER_4BYTE_ADDRESS (0xB7U)

#define W25Q_PAGE_SIZE               (256U)
#define W25Q_READ_CHUNK_SIZE         (1024U)
#define W25Q_MAX_3BYTE_ADDRESS        (0xFFFFFFUL)
#define W25Q_STATUS_WRITE_TIMEOUT_MS  (500U)
#define W25Q_CHIP_ERASE_TIMEOUT_MS    (120000U)

extern uint32_t SystemCoreClock;

static W25Q_Information s_information = {
    .status = FLASH_INIT_NO_EXITS,
};

static void W25qxxDelayMs(uint32_t delayMs)
{
    while (delayMs-- != 0U)
    {
        SDK_DelayAtLeastUs(1000U, SystemCoreClock);
    }
}

static status_t W25qxxCommand(const uint8_t *command, size_t commandSize)
{
    return MY_SPI0_Transmit(command, commandSize, NULL, 0U);
}

static bool W25qxxIsAddressRangeValid(uint32_t address, uint32_t size)
{
    return (s_information.capacityBytes != 0U) && (address < s_information.capacityBytes) &&
           (size <= (s_information.capacityBytes - address));
}

static uint8_t W25qxxAddressByteCount(void)
{
    return s_information.ADS ? 4U : 3U;
}

static uint8_t W25qxxBuildAddressCommand(uint8_t *command, uint8_t opcode, uint32_t address)
{
    uint8_t index = 1U;

    command[0] = opcode;
    if (s_information.ADS)
    {
        command[index++] = (uint8_t)(address >> 24U);
    }
    command[index++] = (uint8_t)(address >> 16U);
    command[index++] = (uint8_t)(address >> 8U);
    command[index++] = (uint8_t)address;
    return index;
}

static bool W25qxxReadCommand(uint8_t opcode,
                               uint32_t address,
                               bool hasAddress,
                               uint8_t dummyBytes,
                               uint8_t *data,
                               size_t size)
{
    uint8_t command[8];
    size_t commandSize = 1U;
    size_t commandPrefixSize;

    commandPrefixSize = hasAddress ? ((size_t)W25qxxAddressByteCount() + 1U) : 1U;

    /*
     * 0x4B (Read Unique ID) has no address and uses four dummy bytes.  The
     * former fixed "- 5" test was sized for a four-byte address command and
     * rejected that valid 1 + 4 byte sequence, making W25qxxInit fail after
     * an otherwise valid JEDEC-ID read.
     */
    if ((data == NULL) || (size == 0U) ||
        ((size_t)dummyBytes > (sizeof(command) - commandPrefixSize)) ||
        (size > (MY_SPI0_MAX_TRANSACTION_BYTES - commandPrefixSize - dummyBytes)))
    {
        return false;
    }

    command[0] = opcode;
    if (hasAddress)
    {
        commandSize = W25qxxBuildAddressCommand(command, opcode, address);
    }
    while (dummyBytes-- != 0U)
    {
        command[commandSize++] = 0x00U;
    }

    return MY_SPI0_TransmitReceive(command, commandSize, data, size) == kStatus_Success;
}

static bool W25qxxReset(void)
{
    uint8_t command = W25Q_CMD_ENABLE_RESET;

    if (W25qxxCommand(&command, 1U) != kStatus_Success)
    {
        return false;
    }
    W25qxxDelayMs(1U);

    command = W25Q_CMD_RESET_DEVICE;
    if (W25qxxCommand(&command, 1U) != kStatus_Success)
    {
        return false;
    }
    W25qxxDelayMs(2U);
    return true;
}

bool W25qxxReadJedecId(uint8_t id[3])
{
    return (id != NULL) && W25qxxReadCommand(W25Q_CMD_JEDEC_ID, 0U, false, 0U, id, 3U);
}

uint16_t W25qxxReadID(void)
{
    const uint8_t command[4] = {W25Q_CMD_MANUFACTURER_ID, 0U, 0U, 0U};
    uint8_t id[2] = {0U, 0U};

    /* 0x90 requires three address/dummy bytes before the two response bytes. */
    if (MY_SPI0_TransmitReceive(command, sizeof(command), id, sizeof(id)) != kStatus_Success)
    {
        return 0U;
    }

    return ((uint16_t)id[0] << 8U) | id[1];
}

static bool W25qxxReadStatus(uint32_t reg, uint8_t *value)
{
    static const uint8_t statusCommand[3] = {
        W25Q_CMD_READ_STATUS_1, W25Q_CMD_READ_STATUS_2, W25Q_CMD_READ_STATUS_3};

    if ((value == NULL) || (reg < 1U) || (reg > 3U))
    {
        return false;
    }

    return W25qxxReadCommand(statusCommand[reg - 1U], 0U, false, 0U, value, 1U);
}

uint8_t W25qxxReadSR(uint32_t reg)
{
    uint8_t value = 0xFFU;

    (void)W25qxxReadStatus(reg, &value);
    return value;
}

static bool W25qxxWriteEnableChecked(void)
{
    const uint8_t command = W25Q_CMD_WRITE_ENABLE;
    uint8_t status;

    return (W25qxxCommand(&command, 1U) == kStatus_Success) &&
           W25qxxReadStatus(1U, &status) && ((status & 0x02U) != 0U);
}

bool W25qxxWriteStatus1(uint8_t value)
{
    const uint8_t command[2] = {W25Q_CMD_WRITE_STATUS_1, value};

    return (s_information.status == FLASH_INIT_SUCCESS) && W25qxxWriteEnableChecked() &&
           (W25qxxCommand(command, sizeof(command)) == kStatus_Success) &&
           W25qxxWaitBusy(W25Q_STATUS_WRITE_TIMEOUT_MS);
}

bool W25qxxWriteDisable(void)
{
    const uint8_t command = W25Q_CMD_WRITE_DISABLE;

    return W25qxxCommand(&command, sizeof(command)) == kStatus_Success;
}

void W25qxxWriteEnable(void)
{
    (void)W25qxxWriteEnableChecked();
}

bool W25qxxWaitBusy(uint32_t timeoutMs)
{
    do
    {
        uint8_t status;

        if (!W25qxxReadStatus(1U, &status))
        {
            return false;
        }
        if ((status & 0x01U) == 0U)
        {
            return true;
        }
        W25qxxDelayMs(1U);
    } while (timeoutMs-- != 0U);

    return false;
}

bool W25qxxGetBusy(bool *busy)
{
    uint8_t status;

    if ((busy == NULL) || (s_information.status != FLASH_INIT_SUCCESS) || !W25qxxReadStatus(1U, &status))
    {
        return false;
    }
    *busy = (status & 0x01U) != 0U;
    return true;
}

bool W25qxxInit(uint8_t mode)
{
    uint8_t jedecId[3];

    (void)mode; /* This board wires standard single-bit SPI only. */
    (void)memset(&s_information, 0, sizeof(s_information));
    s_information.status = FLASH_INIT_NO_EXITS;

    if (!W25qxxReset())
    {
        return false;
    }

    if (!W25qxxReadJedecId(jedecId) || (jedecId[0] == 0x00U) || (jedecId[0] == 0xFFU))
    {
        s_information.status = FLASH_INIT_NO_EXITS;
        return false;
    }

    s_information.MF = jedecId[0];
    s_information.ID = jedecId[2];
    s_information.capacityBytes = (jedecId[2] < 32U) ? (1UL << jedecId[2]) : 0U;
    if ((s_information.capacityBytes == 0U) || (s_information.capacityBytes > (W25Q_MAX_3BYTE_ADDRESS + 1UL)))
    {
        const uint8_t command = W25Q_CMD_ENTER_4BYTE_ADDRESS;

        if ((s_information.capacityBytes == 0U) || (W25qxxCommand(&command, sizeof(command)) != kStatus_Success))
        {
            s_information.status = FLASH_INIT_NO_EXITS;
            return false;
        }
        s_information.ADS = true;
    }
    else
    {
        s_information.ADS = false;
    }
    s_information.mode = FLASH_SPI_MODE;
    s_information.status = FLASH_INIT_SUCCESS;

    /* The UID command needs four dummy bytes after its opcode. */
    if (!W25qxxReadCommand(W25Q_CMD_READ_UNIQUE_ID, 0U, false, 4U,
                            s_information.uniqueID, FLASH_UNIQUEID_BYTE_SIZE))
    {
        s_information.status = FLASH_INIT_NO_EXITS;
        return false;
    }

    return true;
}

bool W25qxxRead(uint32_t address, uint8_t *data, uint32_t size)
{
    uint32_t chunk;

    if ((s_information.status != FLASH_INIT_SUCCESS) || ((data == NULL) && (size != 0U)) ||
        !W25qxxIsAddressRangeValid(address, size))
    {
        return false;
    }

    while (size != 0U)
    {
        chunk = (size > W25Q_READ_CHUNK_SIZE) ? W25Q_READ_CHUNK_SIZE : size;
        if (!W25qxxReadCommand(W25Q_CMD_FAST_READ, address, true, 1U, data, chunk))
        {
            return false;
        }
        address += chunk;
        data += chunk;
        size -= chunk;
    }

    return true;
}

bool W25qxxPageProgram(uint32_t address, const uint8_t *data, uint32_t size)
{
    uint8_t command[5];
    uint32_t chunk;
    uint32_t pageRemaining;

    if ((s_information.status != FLASH_INIT_SUCCESS) || ((data == NULL) && (size != 0U)) ||
        !W25qxxIsAddressRangeValid(address, size))
    {
        return false;
    }

    while (size != 0U)
    {
        pageRemaining = W25Q_PAGE_SIZE - (address & (W25Q_PAGE_SIZE - 1U));
        chunk = (size < pageRemaining) ? size : pageRemaining;

        if (!W25qxxWriteEnableChecked())
        {
            return false;
        }
        if (MY_SPI0_Transmit(command, W25qxxBuildAddressCommand(command, W25Q_CMD_PAGE_PROGRAM, address), data, chunk) != kStatus_Success)
        {
            return false;
        }
        if (!W25qxxWaitBusy(10U))
        {
            return false;
        }

        address += chunk;
        data += chunk;
        size -= chunk;
    }

    return true;
}

bool W25qxxEraseSector(uint32_t address)
{
    return W25qxxStartEraseSector(address) && W25qxxWaitBusy(500U);
}

bool W25qxxStartEraseSector(uint32_t address)
{
    uint8_t command[5];

    if ((s_information.status != FLASH_INIT_SUCCESS) || !W25qxxIsAddressRangeValid(address, 1U) ||
        !W25qxxWriteEnableChecked())
    {
        return false;
    }
    return W25qxxCommand(command, W25qxxBuildAddressCommand(command, W25Q_CMD_SECTOR_ERASE,
                                                              address & ~(NOR_FLASH_SECTOR_SIZE - 1UL))) == kStatus_Success;
}

bool W25qxxEraseBlock(uint32_t address, bool erase64K)
{
    uint8_t command[5];
    uint32_t blockSize = erase64K ? NOR_FLASH_BLOCK_64K : NOR_FLASH_BLOCK_32K;
    uint8_t opcode = erase64K ? W25Q_CMD_BLOCK_ERASE_64K : W25Q_CMD_BLOCK_ERASE_32K;

    if ((s_information.status != FLASH_INIT_SUCCESS) || !W25qxxIsAddressRangeValid(address, 1U))
    {
        return false;
    }

    if (!W25qxxWriteEnableChecked())
    {
        return false;
    }
    return (W25qxxCommand(command, W25qxxBuildAddressCommand(command, opcode,
                                                                address & ~(blockSize - 1UL))) == kStatus_Success) &&
           W25qxxWaitBusy(2500U);
}

bool W25qxxChipErase(void)
{
    const uint8_t command = W25Q_CMD_CHIP_ERASE;

    return (s_information.status == FLASH_INIT_SUCCESS) && W25qxxWriteEnableChecked() &&
           (W25qxxCommand(&command, sizeof(command)) == kStatus_Success) &&
           W25qxxWaitBusy(W25Q_CHIP_ERASE_TIMEOUT_MS);
}

const W25Q_Information *W25qxxGetInformation(void)
{
    return &s_information;
}
