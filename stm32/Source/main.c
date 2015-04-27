/* Includes ------------------------------------------------------------------*/
#include "main.h"

#include <stdio.h>
#include <stdbool.h>

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* UART handler declaration */
UART_HandleTypeDef g_uartHandle;

TIM_HandleTypeDef  g_pwmTimHandle;

TIM_HandleTypeDef  g_pwmOutputTimHandle;

/* Captured Value */
__IO uint32_t            g_pwmInputDutyCompare = 0;
__IO uint32_t            g_pwmInputFreqCompare = 0;

__IO uint32_t            g_dutyCycle    = 0;
__IO uint32_t            g_frequency    = 0;

__IO bool                g_bConversionInProgress = false;
__IO uint8_t 		 ReadValue;

/* Private function prototypes -----------------------------------------------*/
static void SetupSystemClock(void);
static void ErrorHandler(void);

static void SetupUart(void);
static void SetupPwmInputCapture(void);
static void SetupPwmTest();

static void StartPwmConversion(void);
static void StopPwmConversion(void);

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Main program
  * @param  None
  * @retval None
  */
int main(void)
{
    HAL_Init();

    /* Configure LED3 and LED4 */
    BSP_LED_Init(LED3);
    BSP_LED_Init(LED4);

    /* Configure the system clock to 48 Mhz */
    SetupSystemClock();

    /* Configure the peripherals */
    SetupUart();
    SetupPwmInputCapture();
    SetupPwmTest();

    uint32_t frequency_hz;
    uint16_t dutyCycle_percent_div100;

    uint8_t const msgSize = sizeof(frequency_hz)+sizeof(dutyCycle_percent_div100)+1;
    uint8_t msg[msgSize];


    StartPwmConversion();
    while(1)
    {
        BSP_LED_Off(LED4);

        StartPwmConversion();
        HAL_Delay(50);

        // Check the conversion progress flag to see if PWM input is disabled
        // Conversion will still be in progress if interrupt handler hasn't been called yet
        if (!g_bConversionInProgress)
        {
            // Interrupt handler should have disabled PWM input capture
            frequency_hz = g_frequency;
            dutyCycle_percent_div100 = (uint8_t)g_dutyCycle;
        }
        else
        {
            // Interrupt handler should have disabled PWM input capture
	    // If output is high, then this is likely 100% duty cycle (no edges for timer to capture)
	    ReadValue = HAL_GPIO_ReadPin(GPIO_PORT, GPIO_PIN_CHANNEL2);
	    if (ReadValue)
	    {
	    	// If it's low then it is 0% duty cycle (again... no edges for timer to capture)
            	frequency_hz = 0;
            	dutyCycle_percent_div100 = 10000;
	    }
	    else
            {
            	frequency_hz = 0;
            	dutyCycle_percent_div100 = 0;
	    }
        }
        msg[0] = (frequency_hz >> 24) & 0xff;
        msg[1] = (frequency_hz >> 16) & 0xff;
        msg[2] = (frequency_hz >> 8) & 0xff;
        msg[3] = (frequency_hz) & 0xff;
        msg[4] = (dutyCycle_percent_div100 >> 8) & 0xff;
        msg[5] = (dutyCycle_percent_div100 & 0xff);
        msg[6] = '\r';

        BSP_LED_On(LED4);
        HAL_UART_Transmit(&g_uartHandle, (uint8_t*)msg, msgSize, 5000);
        HAL_Delay(50);
        BSP_LED_Off(LED4);

        HAL_Delay(50);
    }
}

void SetupUart(void)
{
    /* Put the USART peripheral in the Asynchronous mode (UART Mode) */
    /* UART configured as follows:
      - Word Length = 8 Bits
      - Stop Bit = One Stop bit
      - Parity = None
      - BaudRate = 9600 baud
      - Hardware flow control disabled (RTS and CTS signals) */
    g_uartHandle.Instance        = USARTx;

    g_uartHandle.Init.BaudRate   = 9600;
    g_uartHandle.Init.WordLength = UART_WORDLENGTH_8B;
    g_uartHandle.Init.StopBits   = UART_STOPBITS_1;
    g_uartHandle.Init.Parity     = UART_PARITY_NONE;
    g_uartHandle.Init.HwFlowCtl  = UART_HWCONTROL_NONE;
    g_uartHandle.Init.Mode       = UART_MODE_TX_RX;
    g_uartHandle.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

    if( HAL_UART_DeInit(&g_uartHandle) != HAL_OK)
    {
        ErrorHandler();
    }
    if( HAL_UART_Init(&g_uartHandle) != HAL_OK)
    {
        ErrorHandler();
    }
}

void SetupPwmInputCapture(void)
{
    g_pwmTimHandle.Instance = TIMx_PWM_INPUT;

    g_pwmTimHandle.Init.Period            = 0xFFFF;
    g_pwmTimHandle.Init.Prescaler         = 0;
    g_pwmTimHandle.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    g_pwmTimHandle.Init.CounterMode       = TIM_COUNTERMODE_UP;
    g_pwmTimHandle.Init.RepetitionCounter = 0;
    if (HAL_TIM_IC_Init(&g_pwmTimHandle) != HAL_OK)
    {
        /* Initialization Error */
        ErrorHandler();
    }

    TIM_IC_InitTypeDef sConfig;

    /* Configure the Input Capture channels */
    /* Common configuration */
    sConfig.ICPrescaler = TIM_ICPSC_DIV1;
    sConfig.ICFilter = 0;

    /* Configure the Input Capture of channel 1 */
    // Used for duty cycle
    //
    sConfig.ICPolarity = TIM_ICPOLARITY_FALLING;
    sConfig.ICSelection = TIM_ICSELECTION_INDIRECTTI;
    if (HAL_TIM_IC_ConfigChannel(&g_pwmTimHandle, &sConfig, TIM_CHANNEL_1) != HAL_OK)
    {
        /* Configuration Error */
        ErrorHandler();
    }

    /* Configure the Input Capture of channel 2 */
    // Used for frequency calculation
    sConfig.ICPolarity = TIM_ICPOLARITY_RISING;
    sConfig.ICSelection = TIM_ICSELECTION_DIRECTTI;
    if (HAL_TIM_IC_ConfigChannel(&g_pwmTimHandle, &sConfig, TIM_CHANNEL_2) != HAL_OK)
    {
        /* Configuration Error */
        ErrorHandler();
    }
    /*##-3- Configure the slave mode ###########################################*/
    /* Select the slave Mode: Reset Mode  */
    TIM_SlaveConfigTypeDef sSlaveConfig;

    sSlaveConfig.SlaveMode        = TIM_SLAVEMODE_RESET;
    sSlaveConfig.InputTrigger     = TIM_TS_TI2FP2;
    sSlaveConfig.TriggerPolarity  = TIM_TRIGGERPOLARITY_NONINVERTED;
    sSlaveConfig.TriggerPrescaler = TIM_TRIGGERPRESCALER_DIV8;
    sSlaveConfig.TriggerFilter    = 0;
    if (HAL_TIM_SlaveConfigSynchronization(&g_pwmTimHandle, &sSlaveConfig) != HAL_OK)
    {
        /* Configuration Error */
        ErrorHandler();
    }
    GPIO_InitTypeDef   GPIO_InitStructure;

    /* Configure PA.00 pin as input floating */
    GPIO_InitStructure.Mode = GPIO_MODE_INPUT;
    GPIO_InitStructure.Pull = GPIO_NOPULL;
    GPIO_InitStructure.Pin = GPIO_PIN_CHANNEL2;
    //HAL_GPIO_Init(GPIO_PORT, &GPIO_InitStructure);
}

void SetupPwmTest()
{
    g_pwmOutputTimHandle.Instance = TIMx_PWM_OUTPUT;

    g_pwmOutputTimHandle.Init.Period            = 2400 / 4;
    g_pwmOutputTimHandle.Init.Prescaler         = 0;
    g_pwmOutputTimHandle.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    g_pwmOutputTimHandle.Init.CounterMode       = TIM_COUNTERMODE_UP;
    g_pwmOutputTimHandle.Init.RepetitionCounter = 0;
    if (HAL_TIM_PWM_Init(&g_pwmOutputTimHandle) != HAL_OK)
    {
        /* Initialization Error */
        ErrorHandler();
    }

    TIM_OC_InitTypeDef sConfig;

    sConfig.OCMode       = TIM_OCMODE_PWM1;
    sConfig.OCPolarity   = TIM_OCPOLARITY_HIGH;
    sConfig.OCFastMode   = TIM_OCFAST_DISABLE;
    sConfig.OCNPolarity  = TIM_OCNPOLARITY_HIGH;
    sConfig.OCIdleState  = TIM_OCIDLESTATE_RESET;
    sConfig.OCNIdleState = TIM_OCNIDLESTATE_RESET;

    sConfig.Pulse = 24 / 4;

    if (HAL_TIM_PWM_ConfigChannel(&g_pwmOutputTimHandle, &sConfig, TIM_CHANNEL_1) != HAL_OK)
    {
      /* Configuration Error */
      ErrorHandler();
    }

    HAL_TIM_PWM_Start(&g_pwmOutputTimHandle, TIM_CHANNEL_1);
}

void StartPwmConversion()
{
    g_bConversionInProgress = true;
    if (HAL_TIM_IC_Start_IT(&g_pwmTimHandle, TIM_CHANNEL_2) != HAL_OK)
    {
        /* Starting Error */
        ErrorHandler();
    }

    if (HAL_TIM_IC_Start_IT(&g_pwmTimHandle, TIM_CHANNEL_1) != HAL_OK)
    {
        /* Starting Error */
        ErrorHandler();
    }

    HAL_TIM_IC_Start(&g_pwmTimHandle, TIM_CHANNEL_1);
    HAL_TIM_IC_Start(&g_pwmTimHandle, TIM_CHANNEL_2);


}

void StopPwmConversion()
{
    HAL_TIM_IC_Stop(&g_pwmTimHandle, TIM_CHANNEL_2);
    HAL_TIM_IC_Stop(&g_pwmTimHandle, TIM_CHANNEL_1);
    HAL_TIM_IC_Stop_IT(&g_pwmTimHandle, TIM_CHANNEL_2);
    HAL_TIM_IC_Stop_IT(&g_pwmTimHandle, TIM_CHANNEL_1);
    g_pwmTimHandle.Instance->CNT = 0;
}

/**
  * @brief  System Clock Configuration
  *         The system Clock is configured as follow : 
  *            System Clock source            = PLL (HSE)
  *            SYSCLK(Hz)                     = 48000000
  *            HCLK(Hz)                       = 48000000
  *            AHB Prescaler                  = 1
  *            APB1 Prescaler                 = 1
  *            HSE Frequency(Hz)              = 8000000
  *            PREDIV                         = 1
  *            PLLMUL                         = 6
  *            Flash Latency(WS)              = 1
  * @param  None
  * @retval None
  */
static void SetupSystemClock(void)
{
    RCC_ClkInitTypeDef RCC_ClkInitStruct;
    RCC_OscInitTypeDef RCC_OscInitStruct;

    /* Select HSE Oscillator as PLL source */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PREDIV = RCC_PREDIV_DIV1;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL6;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct)!= HAL_OK)
    {
        ErrorHandler();
    }

    /* Select PLL as system clock source and configure the HCLK and PCLK1 clocks dividers */
    RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1);
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1)!= HAL_OK)
    {
        ErrorHandler();
    }
}

/**
  * @brief  UART error callbacks
  * @param  UartHandle: UART handle
  * @note   This example shows a simple way to report transfer error, and you can
  *         add your own implementation.
  * @retval None
  */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *UartHandle)
{
    ErrorHandler();
}

/**
  * @brief  Input Capture callback in non blocking mode
  * @param  htim : TIM IC handle
  * @retval None
  */
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{

    if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2)
    {
        uint32_t timFreq = HAL_RCC_GetHCLKFreq() / (htim->Init.Prescaler ? htim->Init.Prescaler + 1 : 1);
        /* Get the Input Capture value */
        g_pwmInputDutyCompare = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);
        g_pwmInputFreqCompare = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_2);

        StopPwmConversion();

        if (g_pwmInputFreqCompare != 0)
        {
            /* Duty cycle computation */
            g_dutyCycle = (g_pwmInputDutyCompare * 10000) / g_pwmInputFreqCompare;

            /* g_frequency computation */
            g_frequency = timFreq / g_pwmInputFreqCompare;
        }
        else
        {
            g_dutyCycle = 0;
            g_frequency = 0;
        }

        g_bConversionInProgress = false;
    }
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @param  None
  * @retval None
  */
static void ErrorHandler(void)
{
    /* Turn LED3 on */
    BSP_LED_On(LED3);
    while(1)
    {
        /* Error if LED3 is slowly blinking (1 sec. period) */
        BSP_LED_Toggle(LED3);
        HAL_Delay(1000);
    }
}



#ifdef  USE_FULL_ASSERT

/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t* file, uint32_t line)
{ 
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

  /* Infinite loop */
  while (1)
  {
  }
}
#endif

/**
  * @}
  */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
