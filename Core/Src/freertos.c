/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * File Name          : freertos.c
 * Description        : Code for freertos applications
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
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
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stm32f4xx_hal.h"
#include "gpio.h"
#include "stm32f401xc.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define TURN_ON_AUTONOMUS 	(1<<0)	// 0b00000000000000000000000000000001	#01
#define TURN_OFF_AUTONOMUS	(1<<1)	// 0b00000000000000000000000000000010	#02
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
#define LED_ON				HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_RESET);
#define LED_OFF				HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_SET);
#define LED_TOGGLE			HAL_GPIO_TogglePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin);
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim2;

uint32_t IC_Val1 = 0;
uint32_t IC_Val2 = 0;
uint32_t Difference = 0;
uint8_t Is_First_Captured = 0;  // is the first value captured ?

uint32_t BUT_COUNT = 0;
uint32_t ERROR_COUNT = 0;

static uint32_t count_re = 0;
static uint32_t count_dir = 0;
static uint32_t count_esq = 0;
static uint32_t Distancia = 0xff;

static GPIO_PinState pin_dir = GPIO_PIN_SET;
static GPIO_PinState pin_esq = GPIO_PIN_SET;
static GPIO_PinState pin_re = GPIO_PIN_SET;

static GPIO_PinState pin_dir_old = GPIO_PIN_SET;
static GPIO_PinState pin_esq_old = GPIO_PIN_SET;
static GPIO_PinState pin_re_old = GPIO_PIN_SET;

/* USER CODE END Variables */
/* Definitions for Autonomus_Mode */
osThreadId_t Autonomus_ModeHandle;
const osThreadAttr_t Autonomus_Mode_attributes =
		{ .name = "Autonomus_Mode", .stack_size = 128 * 4, .priority =
				(osPriority_t) osPriorityBelowNormal, };
/* Definitions for Manual_Mode */
osThreadId_t Manual_ModeHandle;
const osThreadAttr_t Manual_Mode_attributes = { .name = "Manual_Mode",
		.stack_size = 128 * 4, .priority = (osPriority_t) osPriorityLow, };
/* Definitions for SensorsReading */
osThreadId_t SensorsReadingHandle;
const osThreadAttr_t SensorsReading_attributes = { .name = "SensorsReading",
		.stack_size = 64 * 4, .priority = (osPriority_t) osPriorityNormal, };
/* Definitions for Ultrasonic_Read */
osThreadId_t Ultrasonic_ReadHandle;
const osThreadAttr_t Ultrasonic_Read_attributes = { .name = "Ultrasonic_Read",
		.stack_size = 64 * 4, .priority = (osPriority_t) osPriorityNormal, };
/* Definitions for IDLE_Task */
osThreadId_t IDLE_TaskHandle;
const osThreadAttr_t IDLE_Task_attributes = { .name = "IDLE_Task", .stack_size =
		64 * 4, .priority = (osPriority_t) osPriorityLow, };
/* Definitions for Sensors_State */
osMessageQueueId_t Sensors_StateHandle;
const osMessageQueueAttr_t Sensors_State_attributes =
		{ .name = "Sensors_State" };

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
void HCSR04_Read(void);
/* USER CODE END FunctionPrototypes */

void Autonomus(void *argument);
void Manual(void *argument);
void Sensors(void *argument);
void Ultrasonic(void *argument);
void IDLE(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
 * @brief  FreeRTOS initialization
 * @param  None
 * @retval None
 */
void MX_FREERTOS_Init(void) {
	/* USER CODE BEGIN Init */

	/* USER CODE END Init */

	/* USER CODE BEGIN RTOS_MUTEX */
	/* add mutexes, ... */
	/* USER CODE END RTOS_MUTEX */

	/* USER CODE BEGIN RTOS_SEMAPHORES */
	/* add semaphores, ... */
	/* USER CODE END RTOS_SEMAPHORES */

	/* USER CODE BEGIN RTOS_TIMERS */
	/* start timers, add new ones, ... */
	/* USER CODE END RTOS_TIMERS */

	/* Create the queue(s) */
	/* creation of Sensors_State */
	Sensors_StateHandle = osMessageQueueNew(3, sizeof(uint8_t),
			&Sensors_State_attributes);

	/* USER CODE BEGIN RTOS_QUEUES */
	/* add queues, ... */
	if (Sensors_StateHandle == NULL) {
		Error_Handler();
	}
	/* USER CODE END RTOS_QUEUES */

	/* Create the thread(s) */
	/* creation of Autonomus_Mode */
	Autonomus_ModeHandle = osThreadNew(Autonomus, NULL,
			&Autonomus_Mode_attributes);

	/* creation of Manual_Mode */
	Manual_ModeHandle = osThreadNew(Manual, NULL, &Manual_Mode_attributes);

	/* creation of SensorsReading */
	SensorsReadingHandle = osThreadNew(Sensors, NULL,
			&SensorsReading_attributes);

	/* creation of Ultrasonic_Read */
	Ultrasonic_ReadHandle = osThreadNew(Ultrasonic, NULL,
			&Ultrasonic_Read_attributes);

	/* creation of IDLE_Task */
	IDLE_TaskHandle = osThreadNew(IDLE, NULL, &IDLE_Task_attributes);

	/* USER CODE BEGIN RTOS_THREADS */
	/* add threads, ... */

	osThreadSuspend(Autonomus_ModeHandle);
	osThreadSuspend(Manual_ModeHandle);
	/* USER CODE END RTOS_THREADS */

	/* USER CODE BEGIN RTOS_EVENTS */
	/* add events, ... */
	/* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_Autonomus */
/**
 * @brief  Function implementing the Autonomus_Mode thread.
 * @param  argument: Not used
 * @retval None
 */
/* USER CODE END Header_Autonomus */
void Autonomus(void *argument) {
	/* USER CODE BEGIN Autonomus */
	static uint8_t veloc = 15;
	/*	static GPIO_PinState pin_dir;
	 static GPIO_PinState pin_esq;
	 static GPIO_PinState pin_re;*/
	static uint8_t min_dist = 10;
	static uint32_t decisao;

	const uint8_t REFdebounce = 5;
	static uint8_t in1_0_dir = 0;
	static uint8_t in1_1_dir = 0;
	static uint8_t in1_0_esq = 0;
	static uint8_t in1_1_esq = 0;
	static uint8_t in1_0_re = 0;
	static uint8_t in1_1_re = 0;

	HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
	HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
	init_motors(veloc);
	frente(veloc);

	/* Infinite loop */
	for (;;) {
		/*		osMessageQueueGet(Sensors_StateHandle, &pin_dir, osPriorityNormal, 10U);
		 osMessageQueueGet(Sensors_StateHandle, &pin_esq, osPriorityNormal, 10U);
		 osMessageQueueGet(Sensors_StateHandle, &pin_re, osPriorityNormal, 10U);*/
		osMessageQueueGet(Sensors_StateHandle, &Distancia, osPriorityNormal,
				0U);
		if (Distancia <= min_dist) {
			stop();
			decisao = HAL_GetTick(); /*Consulta o tick atual: "Olha a hora"*/
			if ((decisao % 2) == 0) {
				re();
				osDelay(time_wait_ms() + 100);
				rot_esq();
				osDelay(time_wait_ms());
				frente(veloc);
			} else {
				re();
				osDelay(time_wait_ms() + 100);
				rot_dir(veloc);
				osDelay(time_wait_ms());
				frente(veloc);
			}

		}
		if (pin_re == GPIO_PIN_RESET) {

			in1_0_re++;
			in1_1_re = 0;
			if (in1_0_re >= REFdebounce) {
				in1_1_re = REFdebounce + 1;
				stop();
				frente(veloc);
				osDelay(time_wait_ms() + 100);
				count_re++;
			}

		} else {
			in1_0_re = 0;
			in1_1_re++;
			if (in1_1_re >= REFdebounce) {
				in1_1_re = REFdebounce + 1;
				frente(veloc);
			}
		}

		if (pin_dir == GPIO_PIN_RESET) {
			in1_0_dir++;
			in1_1_dir = 0;
			if (in1_0_dir >= REFdebounce) {
				in1_1_dir = REFdebounce + 1;

				/*Confirmado acionamento*/
				stop();
				re(veloc);
				osDelay(time_wait_ms() + 100);
				rot_esq(veloc);
				osDelay(time_wait_ms() + 100);
				frente(veloc);
				count_dir++;
			}
		} else {
			in1_0_dir = 0;
			in1_1_dir++;
			if (in1_1_dir >= REFdebounce) {
				in1_1_dir = REFdebounce + 1;
			}
		}

		if (pin_esq == GPIO_PIN_RESET) {
			in1_0_esq++;
			in1_1_esq = 0;
			if (in1_0_esq >= REFdebounce) {
				in1_1_esq = REFdebounce + 1;
				stop();
				re(veloc);
				osDelay(time_wait_ms() + 100);
				rot_dir(veloc);
				osDelay(time_wait_ms());
				frente(veloc);
				count_esq++;
			}
		} else {
			in1_0_esq = 0;
			in1_1_esq++;
			if (in1_1_esq >= REFdebounce) {
				in1_1_esq = REFdebounce + 1;
			}
		}

		if ((pin_dir && pin_esq) == GPIO_PIN_SET) {
			frente(veloc);
		}
		osDelay(10);
	}
	/* USER CODE END Autonomus */
}

/* USER CODE BEGIN Header_Manual */
/**
 * @brief Function implementing the Manual_Mode thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_Manual */
void Manual(void *argument) {
	/* USER CODE BEGIN Manual */
	/* Infinite loop */
	for (;;) {
		osDelay(1);
	}
	/* USER CODE END Manual */
}

/* USER CODE BEGIN Header_Sensors */
/**
 * @brief Function implementing the SensorsReading thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_Sensors */
void Sensors(void *argument) {
	/* USER CODE BEGIN Sensors */
	/*static GPIO_PinState pin_dir=GPIO_PIN_SET;
	 static GPIO_PinState pin_esq=GPIO_PIN_SET;
	 static GPIO_PinState pin_re=GPIO_PIN_SET;*/

	/*	osMessageQueuePut(Sensors_StateHandle, &pin_dir, osPriorityNormal, 2U);
	 osMessageQueuePut(Sensors_StateHandle, &pin_esq, osPriorityNormal, 2U);
	 osMessageQueuePut(Sensors_StateHandle, &pin_re, osPriorityNormal, 2U);*/
	/* Infinite loop */
	for (;;) {
		/*Lendo o Estado de sensores infravermelho*/
		pin_re = HAL_GPIO_ReadPin(INF_RE_GPIO_Port, INF_RE_Pin);
		pin_esq = HAL_GPIO_ReadPin(INF_ESQ_GPIO_Port, INF_ESQ_Pin);
		pin_dir = HAL_GPIO_ReadPin(INF_DIR_GPIO_Port, INF_DIR_Pin);

		/*Enviando estados de sensores infravermelho*/
		/*		osMessageQueuePut(Sensors_StateHandle, &pin_dir, osPriorityNormal, 5U);
		 osMessageQueuePut(Sensors_StateHandle, &pin_esq, osPriorityNormal, 5U);
		 osMessageQueuePut(Sensors_StateHandle, &pin_re, osPriorityNormal, 5U);*/
		osDelay(2);
	}
	/* USER CODE END Sensors */
}

/* USER CODE BEGIN Header_Ultrasonic */
/**
 * @brief Function implementing the Ultrasonic_Read thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_Ultrasonic */
void Ultrasonic(void *argument) {
	/* USER CODE BEGIN Ultrasonic */
	HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_1);

	/* Infinite loop */
	for (;;) {
		HCSR04_Read();
		osDelay(100);
	}
	/* USER CODE END Ultrasonic */
}

/* USER CODE BEGIN Header_IDLE */
/**
 * @brief Function implementing the IDLE_Task thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_IDLE */
void IDLE(void *argument) {
	/* USER CODE BEGIN IDLE */
	uint32_t flagsX;
	/* Infinite loop */
	for (;;) {
		flagsX = osThreadFlagsWait(TURN_ON_AUTONOMUS, osFlagsWaitAll,
				osWaitForever);
		osDelay(2000);
		osThreadResume(Autonomus_ModeHandle);
		osDelay(1);
	}
	/* USER CODE END IDLE */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
	if (GPIO_Pin == BUT_Pin) {
		stop();
		BUT_COUNT++;
		LED_OFF
		;
		osThreadFlagsSet(IDLE_TaskHandle, TURN_ON_AUTONOMUS);
	} else {
		ERROR_COUNT++;
	}
}

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim) {
	static uint8_t Distance = 0xff;
	if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1) // if the interrupt source is channel1
			{
		if (Is_First_Captured == 0) // if the first value is not captured
				{
			IC_Val1 = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1); // read the first value
			Is_First_Captured = 1;  // set the first captured as true
			// Now change the polarity to falling edge
			__HAL_TIM_SET_CAPTUREPOLARITY(htim, TIM_CHANNEL_1,
					TIM_INPUTCHANNELPOLARITY_FALLING);
		}

		else if (Is_First_Captured == 1)   // if the first is already captured
				{
			IC_Val2 = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1); // read second value
			__HAL_TIM_SET_COUNTER(htim, 0);  // reset the counter

			if (IC_Val2 > IC_Val1) {
				Difference = IC_Val2 - IC_Val1;
			}

			else if (IC_Val1 > IC_Val2) {
				Difference = (0xffff - IC_Val1) + IC_Val2;
			}

			Distance = Difference * .034 / 2;
			Is_First_Captured = 0; // set it back to false

			osMessageQueuePut(Sensors_StateHandle, &Distance, osPriorityNormal,
					0U);

			// set polarity to rising edge
			__HAL_TIM_SET_CAPTUREPOLARITY(htim, TIM_CHANNEL_1,
					TIM_INPUTCHANNELPOLARITY_RISING);
			__HAL_TIM_DISABLE_IT(&htim1, TIM_IT_CC1);
		}
	}
}

void HCSR04_Read(void) {
	HAL_GPIO_WritePin(TRIG_GPIO_Port, TRIG_Pin, GPIO_PIN_SET); // pull the TRIG pin HIGH
	osDelay(10);  // wait for 10 us
	HAL_GPIO_WritePin(TRIG_GPIO_Port, TRIG_Pin, GPIO_PIN_RESET); // pull the TRIG pin low

	__HAL_TIM_ENABLE_IT(&htim2, TIM_IT_CC1);
}
/* USER CODE END Application */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
