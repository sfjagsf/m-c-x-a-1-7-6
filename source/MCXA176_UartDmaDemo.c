/*
 * Copyright 2016-2026 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file    MCXA176_UartDmaDemo.c
 * @brief   Variable-length LPUART1 RX/TX demo using one eDMA channel.
 */
#include <stdbool.h>
#include <stdint.h>
#include "board.h"
#include "peripherals.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "fsl_gpio.h"
#include "fsl_port.h"
#include "fsl_spc.h"
#include "main.h"

#define UART1_DMA_CHANNEL       LPUART1_RX_DMA_CHANNEL
#define UART1_RX_REQUEST        kDma0RequestLPUART1Rx
#define UART1_TX_REQUEST        kDma0RequestLPUART1Tx
#define UART1_RX_BUFFER_SIZE    256U

/* 12-bit DAC, ref = VDDA. Probe J25-3 (DAC0_OUT), GND on J25-2/4.
 * Do not probe J25-1 (divider ≈ 1/3). Do not configure P2_2 as GPIO. */
#define DAC0_TEST_CODE          2048U

typedef enum
{
    kUart1Rx,
    kUart1RxFrameReady,
    kUart1Tx,
    kUart1TxWaitComplete
} uart1_state_t;

static uint8_t s_uart1RxBuffer[UART1_RX_BUFFER_SIZE];
static volatile uint32_t s_uart1RxLength;
static volatile bool s_uart1FrameReady;
static volatile uart1_state_t s_uart1State;

/* The generated project configures channel 0 for RX.  TX reuses that same
 * channel by changing only the request mux while the channel is stopped. */
static bool UART1_SubmitDmaTransfer(const edma_transfer_config_t *transfer)
{
    return EDMA_SubmitTransfer(&LPUART1_RX_Handle, transfer) == kStatus_Success;
}

static void UART1_StartReceive(void);
static void UART1_StartTransmit(uint8_t *data, uint32_t length);

static void UART1_DmaCallback(edma_handle_t *handle,
                              void *userData,
                              bool transferDone,
                              uint32_t tcds)
{
    (void)userData;
    (void)tcds;

    if (!transferDone)
    {
        return;
    }

    if (s_uart1State == kUart1Rx)
    {
        LPUART_EnableRxDMA(LPUART1_PERIPHERAL, false);
        EDMA_AbortTransfer(handle);
        s_uart1RxLength = UART1_RX_BUFFER_SIZE;
        s_uart1FrameReady = true;
        s_uart1State = kUart1RxFrameReady;
    }
    else if (s_uart1State == kUart1Tx)
    {
        LPUART_EnableTxDMA(LPUART1_PERIPHERAL, false);
        s_uart1State = kUart1TxWaitComplete;
        LPUART_EnableInterrupts(LPUART1_PERIPHERAL,
                                 kLPUART_TransmissionCompleteInterruptEnable);
    }
}

static void UART1_StartReceive(void)
{
    edma_transfer_config_t transfer;

    LPUART_DisableInterrupts(LPUART1_PERIPHERAL,
                              kLPUART_IdleLineInterruptEnable |
                                  kLPUART_RxOverrunInterruptEnable |
                                  kLPUART_TransmissionCompleteInterruptEnable);
    LPUART_EnableTxDMA(LPUART1_PERIPHERAL, false);
    LPUART_EnableRxDMA(LPUART1_PERIPHERAL, false);
    EDMA_AbortTransfer(&LPUART1_RX_Handle);
    /* An IDLE flag can be left pending when a full-buffer transfer ends. */
    (void)LPUART_ClearStatusFlags(LPUART1_PERIPHERAL,
                                   kLPUART_IdleLineFlag | kLPUART_RxOverrunFlag);
    EDMA_SetChannelMux(DMA0_DMA_BASEADDR, UART1_DMA_CHANNEL, UART1_RX_REQUEST);
    EDMA_PrepareTransfer(&transfer,
                         (void *)(uintptr_t)LPUART_GetDataRegisterAddress(LPUART1_PERIPHERAL),
                         sizeof(uint8_t),
                         s_uart1RxBuffer,
                         sizeof(uint8_t),
                         sizeof(uint8_t),
                         UART1_RX_BUFFER_SIZE,
                         kEDMA_PeripheralToMemory);
    if (!UART1_SubmitDmaTransfer(&transfer))
    {
        s_uart1State = kUart1RxFrameReady;
        s_uart1RxLength = 0U;
        s_uart1FrameReady = true;
        return;
    }

    s_uart1State = kUart1Rx;
    EDMA_StartTransfer(&LPUART1_RX_Handle);
    LPUART_EnableRxDMA(LPUART1_PERIPHERAL, true);
    LPUART_EnableInterrupts(LPUART1_PERIPHERAL,
                             kLPUART_IdleLineInterruptEnable |
                             kLPUART_RxOverrunInterruptEnable);
}

static void UART1_StartTransmit(uint8_t *data, uint32_t length)
{
    edma_transfer_config_t transfer;

    if ((data == NULL) || (length == 0U))
    {
        UART1_StartReceive();
        return;
    }

    LPUART_DisableInterrupts(LPUART1_PERIPHERAL,
                             kLPUART_IdleLineInterruptEnable |
                             kLPUART_RxOverrunInterruptEnable);
    LPUART_EnableRxDMA(LPUART1_PERIPHERAL, false);
    LPUART_EnableTxDMA(LPUART1_PERIPHERAL, false);
    EDMA_AbortTransfer(&LPUART1_RX_Handle);
    EDMA_SetChannelMux(DMA0_DMA_BASEADDR, UART1_DMA_CHANNEL, UART1_TX_REQUEST);
    EDMA_PrepareTransfer(&transfer,
                         data,
                         sizeof(uint8_t),
                         (void *)(uintptr_t)LPUART_GetDataRegisterAddress(LPUART1_PERIPHERAL),
                         sizeof(uint8_t),
                         sizeof(uint8_t),
                         length,
                         kEDMA_MemoryToPeripheral);
    if (!UART1_SubmitDmaTransfer(&transfer))
    {
        /* Do not enable TX DMA if the descriptor could not be installed. */
        EDMA_SetChannelMux(DMA0_DMA_BASEADDR, UART1_DMA_CHANNEL, UART1_RX_REQUEST);
        UART1_StartReceive();
        return;
    }

    s_uart1State = kUart1Tx;
    EDMA_StartTransfer(&LPUART1_RX_Handle);
    LPUART_EnableTxDMA(LPUART1_PERIPHERAL, true);
}

//void LPUART1_IRQHandler(void)
void MyUart_IRQHandler(void)
{
    uint32_t status = LPUART_GetStatusFlags(LPUART1_PERIPHERAL);
    uint32_t enabled = LPUART_GetEnabledInterrupts(LPUART1_PERIPHERAL);

    if (((status & kLPUART_RxOverrunFlag) != 0U) &&
        (s_uart1State == kUart1Rx))
    {
        (void)LPUART_ClearStatusFlags(LPUART1_PERIPHERAL, kLPUART_RxOverrunFlag);
        LPUART_EnableRxDMA(LPUART1_PERIPHERAL, false);
        EDMA_AbortTransfer(&LPUART1_RX_Handle);
        s_uart1State = kUart1RxFrameReady;
        s_uart1RxLength = 0U;
        s_uart1FrameReady = true;
    }

    if (((status & kLPUART_IdleLineFlag) != 0U) &&
        ((enabled & kLPUART_IdleLineInterruptEnable) != 0U) &&
        (s_uart1State == kUart1Rx))
    {
        uint32_t remaining;

        LPUART_EnableRxDMA(LPUART1_PERIPHERAL, false);
        remaining = EDMA_GetRemainingMajorLoopCount(DMA0_DMA_BASEADDR,
                                                     UART1_DMA_CHANNEL);
        EDMA_AbortTransfer(&LPUART1_RX_Handle);
        (void)LPUART_ClearStatusFlags(LPUART1_PERIPHERAL, kLPUART_IdleLineFlag);

        if (remaining <= UART1_RX_BUFFER_SIZE)
        {
            s_uart1RxLength = UART1_RX_BUFFER_SIZE - remaining;
        }
        else
        {
            s_uart1RxLength = 0U;
        }

        if (s_uart1RxLength != 0U)
        {
            s_uart1FrameReady = true;
            s_uart1State = kUart1RxFrameReady;
        }
        else
        {
            UART1_StartReceive();
        }
    }

    if (((status & kLPUART_TransmissionCompleteFlag) != 0U) &&
        ((enabled & kLPUART_TransmissionCompleteInterruptEnable) != 0U) &&
        (s_uart1State == kUart1TxWaitComplete))
    {
        LPUART_DisableInterrupts(LPUART1_PERIPHERAL,
                                  kLPUART_TransmissionCompleteInterruptEnable);
        UART1_StartReceive();
    }

    SDK_ISR_EXIT_BARRIER;
}

//int main(void)
//{
//    BOARD_InitBootPins();
//    BOARD_InitBootClocks();
//    BOARD_InitBootPeripherals();
//
//    DAC_SetData(DAC0_PERIPHERAL, DAC0_TEST_CODE);
//
//    EDMA_SetCallback(&LPUART1_RX_Handle, UART1_DmaCallback, NULL);
//    UART1_StartReceive();
//
//    while (1)
//    {
//        if (s_uart1FrameReady)
//        {
//            s_uart1FrameReady = false;
//            UART1_StartTransmit(s_uart1RxBuffer, s_uart1RxLength);
//        }
//    }
//}

int main(void)
{
    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitBootPeripherals();


    while (1)
    {
        GPIO_PinWrite(BOARD_INITPINS_ADC_PWR_GPIO, BOARD_INITPINS_ADC_PWR_PIN, 1);

        DAC_SetData(DAC0_PERIPHERAL, 4095U);
        DAC_DoSoftwareTriggerFIFO(DAC0_PERIPHERAL);

    }
}
