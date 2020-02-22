#include "main.h"
#include "can.hpp"
#include "slcan.hpp"
#include "led.h"
#include "usb_device.h"
#include "usbd_cdc_if.h"

extern CAN_HandleTypeDef hcan;
CAN_FilterTypeDef filter;
uint32_t prescaler = 2;
enum can_bus_state bus_state;

CAN_RxHeaderTypeDef rx_header;
uint8_t rx_payload[CAN_MTU];
uint32_t status;
uint8_t msg_buf[SLCAN_MTU];

/*
 void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
 {
 status = can_rx(&rx_header, rx_payload);
 if (status == HAL_OK)
 {
 status = slcan_parse_frame((uint8_t *) &msg_buf, &rx_header, rx_payload);

 //serial.write(msg_buf, status);
 CDC_Transmit_FS(msg_buf, status);

 }
 led_process();
 }
 */

void can_init(void)
{
    // default to 500 kbit/s
    prescaler = 4;
    hcan.Instance = CAN;
    bus_state = OFF_BUS;
}

void can_set_filter(uint32_t id, uint32_t mask)
{
    // see page 825 of RM0091 for details on filters
    // set the standard ID part
    filter.FilterIdHigh = (id & 0x7FF) << 5;
    // add the top 5 bits of the extended ID
    filter.FilterIdHigh += (id >> 24) & 0xFFFF;
    // set the low part to the remaining extended ID bits
    filter.FilterIdLow += ((id & 0x1FFFF800) << 3);

    // set the standard ID part
    filter.FilterMaskIdHigh = (mask & 0x7FF) << 5;
    // add the top 5 bits of the extended ID
    filter.FilterMaskIdHigh += (mask >> 24) & 0xFFFF;
    // set the low part to the remaining extended ID bits
    filter.FilterMaskIdLow += ((mask & 0x1FFFF800) << 3);

    filter.FilterMode = CAN_FILTERMODE_IDMASK;
    filter.FilterScale = CAN_FILTERSCALE_32BIT;
    filter.FilterBank = 0;
    filter.FilterFIFOAssignment = CAN_FILTER_FIFO0;
    filter.SlaveStartFilterBank = 0;
    filter.FilterActivation = ENABLE;

    if (bus_state == ON_BUS)
    {
        HAL_CAN_ConfigFilter(&hcan, &filter);
    }
}

void can_enable(void)
{
    if (bus_state == OFF_BUS)
    {
        hcan.Instance = CAN;
        hcan.Init.Prescaler = prescaler;
        hcan.Init.Mode = CAN_MODE_NORMAL;
        hcan.Init.SyncJumpWidth = CAN_SJW_1TQ;
        hcan.Init.TimeSeg1 = CAN_BS1_15TQ;
        hcan.Init.TimeSeg2 = CAN_BS2_2TQ;
        hcan.Init.TimeTriggeredMode = DISABLE;
        hcan.Init.AutoBusOff = ENABLE;
        hcan.Init.AutoWakeUp = DISABLE;
        hcan.Init.AutoRetransmission = ENABLE;
        hcan.Init.ReceiveFifoLocked = DISABLE;
        hcan.Init.TransmitFifoPriority = DISABLE;
        HAL_CAN_Init(&hcan);
        bus_state = ON_BUS;
        can_set_filter(0, 0);

        /* Start the CAN peripheral */
        if (HAL_CAN_Start(&hcan) != HAL_OK)
        {
            /* Start Error */
            Error_Handler();
        }

#if 0
        /* Activate CAN RX notification */
        if (HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK)
        {
          /* Notification Error */
          Error_Handler();
        }
#endif
    }

    GPIOB->BSRR = GPIO_BSRR_BS_6;
}

void can_disable(void)
{
    if (bus_state == ON_BUS)
    {
        // do a bxCAN reset (set RESET bit to 1)
        hcan.Instance->MCR |= CAN_MCR_RESET;
        bus_state = OFF_BUS;
    }

    GPIOB->BSRR = GPIO_BSRR_BR_6;
    GPIOB->BSRR = GPIO_BSRR_BR_4;
}

void can_set_bitrate(enum can_bitrate bitrate)
{
    if (bus_state == ON_BUS)
    {
        // cannot set bitrate while on bus
        return;
    }

    switch (bitrate)
    {
    case CAN_BITRATE_10K:
        prescaler = 450;
        break;
    case CAN_BITRATE_20K:
        prescaler = 225;
        break;
    case CAN_BITRATE_50K:
        prescaler = 90;
        break;
    case CAN_BITRATE_100K:
        prescaler = 45;
        break;
    case CAN_BITRATE_125K:
        prescaler = 16;//36;
        break;
    case CAN_BITRATE_250K:
        prescaler = 8;//18;
        break;
    case CAN_BITRATE_500K:
        prescaler = 4;//9;
        break;
    case CAN_BITRATE_750K:
        prescaler = 3;//6;
        break;
    case CAN_BITRATE_1000K:
        prescaler = 2;//4;
        break;
    }
}

void can_set_silent(uint8_t silent)
{
    if (bus_state == ON_BUS)
    {
        // cannot set silent mode while on bus
        return;
    }
    if (silent)
    {
        hcan.Init.Mode = CAN_MODE_SILENT;
    }
    else
    {
        hcan.Init.Mode = CAN_MODE_NORMAL;
    }
}

uint32_t can_tx(CAN_TxHeaderTypeDef *tx_header, uint8_t (&buf)[CAN_MTU])
{
    uint32_t status;

    // transmit can frame
    //hcan.pTxMsg = tx_msg;
    //status = HAL_CAN_Transmit(&hcan, timeout);

    uint32_t tx_mailbox;
    status = HAL_CAN_AddTxMessage(&hcan, tx_header, buf, &tx_mailbox);

    led_on();
    return status;
}

uint32_t can_rx(CAN_RxHeaderTypeDef *rx_header, uint8_t (&buf)[CAN_MTU])
{
    uint32_t status;

    //hcan.pRxMsg = rx_msg;
    //status = HAL_CAN_Receive(&hcan, CAN_FIFO0, timeout);

    status = HAL_CAN_GetRxMessage(&hcan, CAN_RX_FIFO0, rx_header, buf);

    led_on();
    return status;
}

uint8_t is_can_msg_pending(uint8_t fifo)
{
    if (bus_state == OFF_BUS)
    {
        return 0;
    }
    return (HAL_CAN_GetRxFifoFillLevel(&hcan, fifo) > 0);
}

