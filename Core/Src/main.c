#include "main.h"

/* Sensor outputs D0...D7 are connected to PE0...PE7. */
#define SENSOR_PORT GPIOE
#define SENSOR_MASK 0x00FFU
#define SENSOR_ACTIVE_LOW 1

/* TB6612 connections: PWMA=PB0, PWMB=PB1, AIN1/AIN2=PD0/PD1, BIN1/BIN2=PD2/PD3, STBY=PD4. */
#define MOTOR_PWM_PERIOD 4199U
#define MOTOR_SPEED_FAST 420
#define MOTOR_SPEED_SLOW 220

static TIM_HandleTypeDef htim3;

static void Error_Stop(void)
{
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_4, GPIO_PIN_RESET);
  while (1) { }
}

static void SystemClock_Config(void)
{
  RCC_OscInitTypeDef osc = {0};
  RCC_ClkInitTypeDef clk = {0};
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  osc.HSEState = RCC_HSE_ON;
  osc.PLL.PLLState = RCC_PLL_ON;
  osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  osc.PLL.PLLM = 8;
  osc.PLL.PLLN = 336;
  osc.PLL.PLLP = RCC_PLLP_DIV2;
  osc.PLL.PLLQ = 7;
  if (HAL_RCC_OscConfig(&osc) != HAL_OK) Error_Stop();
  clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
  clk.APB1CLKDivider = RCC_HCLK_DIV4;
  clk.APB2CLKDivider = RCC_HCLK_DIV2;
  if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_5) != HAL_OK) Error_Stop();
}

static void GPIO_Init_All(void)
{
  GPIO_InitTypeDef gpio = {0};
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();

  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
  gpio.Pin = LED_Pin;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_GPIO_Port, &gpio);

  gpio.Pin = SENSOR_MASK;
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(SENSOR_PORT, &gpio);

  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4, GPIO_PIN_RESET);
  gpio.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOD, &gpio);

  gpio.Pin = GPIO_PIN_0 | GPIO_PIN_1;
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  gpio.Alternate = GPIO_AF2_TIM3;
  HAL_GPIO_Init(GPIOB, &gpio);
}

static void PWM_Init(void)
{
  TIM_OC_InitTypeDef pwm = {0};
  __HAL_RCC_TIM3_CLK_ENABLE();
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 0;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = MOTOR_PWM_PERIOD;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK) Error_Stop();
  pwm.OCMode = TIM_OCMODE_PWM1;
  pwm.Pulse = 0;
  pwm.OCPolarity = TIM_OCPOLARITY_HIGH;
  pwm.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &pwm, TIM_CHANNEL_3) != HAL_OK) Error_Stop();
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &pwm, TIM_CHANNEL_4) != HAL_OK) Error_Stop();
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_4);
}

static uint16_t SpeedToPwm(int speed)
{
  if (speed < 0) speed = -speed;
  if (speed > 1000) speed = 1000;
  return (uint16_t)((speed * MOTOR_PWM_PERIOD) / 1000);
}

static void Motor_Set(int left, int right)
{
  if (left == 0 && right == 0) {
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_4, GPIO_PIN_RESET);
  } else {
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_4, GPIO_PIN_SET);
  }
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_0, left > 0 ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_1, left < 0 ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, right > 0 ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_3, right < 0 ? GPIO_PIN_SET : GPIO_PIN_RESET);
  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, SpeedToPwm(left));
  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, SpeedToPwm(right));
}

static uint8_t ReadSensors(void)
{
  uint8_t raw = (uint8_t)(SENSOR_PORT->IDR & SENSOR_MASK);
  return SENSOR_ACTIVE_LOW ? (uint8_t)~raw : raw;
}

static void FollowStraightLine(void)
{
  uint8_t sensor = ReadSensors();
  const uint8_t left = 0x07U;
  const uint8_t center = 0x18U;
  const uint8_t right = 0xE0U;

  if (sensor & center) {
    Motor_Set(MOTOR_SPEED_FAST, MOTOR_SPEED_FAST);
  } else if (sensor & left) {
    Motor_Set(MOTOR_SPEED_SLOW, MOTOR_SPEED_FAST);
  } else if (sensor & right) {
    Motor_Set(MOTOR_SPEED_FAST, MOTOR_SPEED_SLOW);
  } else {
    Motor_Set(0, 0);
  }
}

int main(void)
{
  uint32_t led_tick = 0;
  HAL_Init();
  SystemClock_Config();
  GPIO_Init_All();
  PWM_Init();
  Motor_Set(0, 0);
  while (1) {
    FollowStraightLine();
    if (HAL_GetTick() - led_tick >= 500U) {
      led_tick = HAL_GetTick();
      HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
    }
    HAL_Delay(5);
  }
}

void Error_Handler(void) { Error_Stop(); }
