/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2019 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under Ultimate Liberty license
 * SLA0044, the "License"; You may not use this file except in compliance with
 * the License. You may obtain a copy of the License at:
 *                             www.st.com/SLA0044
 *
 ******************************************************************************
 */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "slcan.hpp"
#include "led.h"
#include <array>
#include "usb_device.h"
#include "usbd_cdc_if.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define STOCK_SIZE 512
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
CAN_HandleTypeDef hcan;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_CAN_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

extern USBD_HandleTypeDef hUsbDeviceFS;
/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {
	/* USER CODE BEGIN 1 */

	/* USER CODE END 1 */

	/* MCU Configuration--------------------------------------------------------*/

	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();

	/* USER CODE BEGIN Init */

	/* USER CODE END Init */

	/* Configure the system clock */
	SystemClock_Config();

	/* USER CODE BEGIN SysInit */
	LL_SYSTICK_EnableIT();

	/* USER CODE END SysInit */

	/* Initialize all configured peripherals */
	MX_GPIO_Init();
	MX_USB_DEVICE_Init();
	//MX_CAN_Init();
	/* USER CODE BEGIN 2 */

	HAL_NVIC_SetPriority(USB_LP_IRQn, 1, 0);

	GPIOB->BSRR = GPIO_BSRR_BS_4 | GPIO_BSRR_BS_5 | GPIO_BSRR_BS_6
			| GPIO_BSRR_BS_7;
	HAL_Delay(100);
	GPIOA->BSRR = GPIO_BSRR_BS_10;
	HAL_Delay(500);
	GPIOB->BSRR = GPIO_BSRR_BR_4 | GPIO_BSRR_BR_5 | GPIO_BSRR_BR_6
			| GPIO_BSRR_BR_7;

	CAN_RxHeaderTypeDef rx_header;
	uint8_t rx_payload[CAN_MTU];
	uint32_t status;

	uint8_t msg_stock[SLCAN_MTU * STOCK_SIZE] = { };
	uint8_t stock_dlc[STOCK_SIZE] = { };
	uint16_t Write_DLC = 0;
	uint16_t Read_DLC = 0;
	uint16_t Write_DATA = 0;
	uint16_t Read_DATA = 0;

	can_init();
	//can_enable();

	//HAL_CAN_Start(&hcan);

	/* USER CODE END 2 */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	while (1) {
		/* USER CODE END WHILE */

		/* USER CODE BEGIN 3 */

		if (is_can_msg_pending(CAN_RX_FIFO0)) { // when can message receive
			led_process();
			status = can_rx(&rx_header, rx_payload);
			if (status == HAL_OK) {
				if (stock_dlc[Write_DLC] == 0) {
					stock_dlc[Write_DLC] = slcan_parse_frame((uint8_t *) &msg_stock[Write_DATA], &rx_header, rx_payload);
					Write_DLC++;
					Write_DATA += SLCAN_MTU;
					if (Write_DLC == STOCK_SIZE || Write_DATA == SLCAN_MTU * STOCK_SIZE){
						Write_DLC = 0;
						Write_DATA = 0;
					}
				} else {
					Error_Handler();// overflow
				}
			}
		}

		if (stock_dlc[Read_DLC] != 0) {
//              status = slcan_parse_frame((uint8_t *) &msg_buf, &rx_header, rx_payload);
			HAL_NVIC_DisableIRQ(USB_LP_IRQn);
			if (CDC_Transmit_FS(&msg_stock[Read_DATA], stock_dlc[Read_DLC])== USBD_OK) {
				msg_stock[Read_DATA] = 0;
				stock_dlc[Read_DLC] = 0;
				Read_DLC++;
				Read_DATA += SLCAN_MTU;
				if (Read_DLC == STOCK_SIZE || Read_DATA == SLCAN_MTU * STOCK_SIZE){
					Read_DLC = 0;
					Read_DATA = 0;
				}
			}
			HAL_NVIC_EnableIRQ(USB_LP_IRQn);
		}

		led_process();

	}
	/* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
	LL_FLASH_SetLatency(LL_FLASH_LATENCY_2);

	if (LL_FLASH_GetLatency() != LL_FLASH_LATENCY_2) {
		Error_Handler();
	}
	LL_RCC_HSE_Enable();

	/* Wait till HSE is ready */
	while (LL_RCC_HSE_IsReady() != 1) {

	}
	LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSE_DIV_1, LL_RCC_PLL_MUL_9);
	LL_RCC_PLL_Enable();

	/* Wait till PLL is ready */
	while (LL_RCC_PLL_IsReady() != 1) {

	}
	LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
	LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_2);
	LL_RCC_SetAPB2Prescaler(LL_RCC_APB1_DIV_1);
	LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);

	/* Wait till System clock is ready */
	while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL) {

	}
	LL_Init1msTick(72000000);
	LL_SYSTICK_SetClkSource(LL_SYSTICK_CLKSOURCE_HCLK);
	LL_SetSystemCoreClock(72000000);
	LL_RCC_SetUSBClockSource(LL_RCC_USB_CLKSOURCE_PLL_DIV_1_5);
}

/**
 * @brief CAN Initialization Function
 * @param None
 * @retval None
 */
static void MX_CAN_Init(void) {

	/* USER CODE BEGIN CAN_Init 0 */

	/* USER CODE END CAN_Init 0 */

	/* USER CODE BEGIN CAN_Init 1 */

	/* USER CODE END CAN_Init 1 */
	hcan.Instance = CAN;
	hcan.Init.Prescaler = 2;
	hcan.Init.Mode = CAN_MODE_NORMAL;
	hcan.Init.SyncJumpWidth = CAN_SJW_1TQ;
	hcan.Init.TimeSeg1 = CAN_BS1_14TQ;
	hcan.Init.TimeSeg2 = CAN_BS2_3TQ;
	hcan.Init.TimeTriggeredMode = DISABLE;
	hcan.Init.AutoBusOff = DISABLE;
	hcan.Init.AutoWakeUp = DISABLE;
	hcan.Init.AutoRetransmission = DISABLE;
	hcan.Init.ReceiveFifoLocked = DISABLE;
	hcan.Init.TransmitFifoPriority = DISABLE;
	if (HAL_CAN_Init(&hcan) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN CAN_Init 2 */

	/* USER CODE END CAN_Init 2 */

}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
	LL_GPIO_InitTypeDef GPIO_InitStruct = { 0 };

	/* GPIO Ports Clock Enable */
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA);
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOB);

	/**/
	LL_GPIO_ResetOutputPin(GPIOA, LL_GPIO_PIN_10);

	/**/
	LL_GPIO_ResetOutputPin(GPIOB,
	LL_GPIO_PIN_4 | LL_GPIO_PIN_5 | LL_GPIO_PIN_6 | LL_GPIO_PIN_7);

	/**/
	GPIO_InitStruct.Pin = LL_GPIO_PIN_10;
	GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
	GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
	GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
	LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/**/
	GPIO_InitStruct.Pin = LL_GPIO_PIN_4 | LL_GPIO_PIN_5 | LL_GPIO_PIN_6
			| LL_GPIO_PIN_7;
	GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
	GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
	GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
	LL_GPIO_Init(GPIOB, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
	/* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */

	/* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(char *file, uint32_t line)
{ 
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
