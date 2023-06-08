#include "main.h"
#include "adc.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"
#include "rcc.h"
#include "lcd.h"
#include "i2c_hal.h"

#define Kadc 3.3/4095.0f

void key_proc(void);
void led_proc(void);
void lcd_proc(void);
void pwm_proc(void);
void adc_proc(void);
void uart_proc(void);

//for delay
__IO uint32_t uwTick_key, uwTick_led, uwTick_lcd, uwTick_pwm, uwTick_adc, uwTick_uart, uwTick_read_uart;

//for key
uint8_t key_down, key_up, key_val, key_old;
uint8_t del_flag;
uint8_t have_del_flag;
uint32_t del_count;

//for eep
uint8_t eep_write[4] = {1, 1, 'f'}; //Y  X
uint8_t eep_read[4];


//for led 
uint8_t led;

//for adc
float AO1_V;
float AO1_buf[200];
float AO1_A;
float AO1_T;
float AO1_H;

float AO2_V;
float AO2_buf[200];
float AO2_A;
float AO2_T;
float AO2_H;

uint8_t N_1;
uint8_t N_2;

uint8_t which_ch = 4;
uint8_t get_val_once;

//for lcd
uint8_t string_lcd[21];
uint8_t which_index; // 0--1--2

//for pwm
uint32_t pwm_in_t;
uint32_t pwm_out_t;
uint8_t pwm_mode;

//for uart
char rx_buf[1];
char tx_buf[50];
char Read_buf[10];
uint8_t uart_count;

//for tasks
uint8_t X = 1;
uint8_t Y = 1;

uint8_t X_real = 1;
uint8_t Y_real = 1;

int main(void)
{

  HAL_Init();
	//HAL_Delay(1);
  SystemClock_Config();

  MX_GPIO_Init();
	
  MX_ADC2_Init();
	
  MX_TIM2_Init();
	HAL_TIM_Base_Start(&htim2);
	HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_2);
	
  MX_TIM3_Init();
	HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
	
  MX_USART1_UART_Init();
	HAL_UART_Receive_IT(&huart1, (uint8_t *)rx_buf, 1);
	
	I2CInit();
	eeprom_read(eep_read, 0, 3);
	if(eep_read[2] != 'f')
		eeprom_write(eep_write, 0, 3);
	else 
	{
		Y_real = eep_read[0];
		X_real = eep_read[1];
		Y = Y_real;
		X = X_real;
	}
	
	LCD_Init();
	LCD_SetTextColor(White);
	LCD_SetBackColor(Black);
	LCD_Clear(Black);

  while (1)
  {
		key_proc();
		lcd_proc();
		adc_proc();
		pwm_proc();
		uart_proc();
  }

}

void key_proc(void)
{
	if(uwTick - uwTick_key<100)	return;
	uwTick_key = uwTick;

	key_val = read_key();
	key_down = key_val & (key_val ^ key_old);
	key_up  = ~key_val & (key_val ^ key_old);	
	key_old = key_val;
	
	if(key_down == 1)
	{
		which_index++;
		if(which_index == 3)
			which_index = 0;
		LCD_Clear(Black);
		
		if(which_index == 2)
			which_ch = 4;
		
		Y_real = Y;
		X_real = X;
		eep_write[0] = Y_real;
		eep_write[1] = X_real;
		eeprom_write(eep_write, 0, 2);
		
	}
	

	else if(key_down == 2)
	{
		if(which_index == 1)
		{
			X+=1;
			if(X>=5)
				X = 1;
		}
	}
	
	else if(key_down == 3)
	{
		if(which_index == 1)
		{
			Y+=1;
			if(Y>=5)
				Y = 1;
		}
	}
	
	else if(key_down == 4)
	{
		if(which_index == 0)
		{
			get_val_once = 1;
		}
		
		else if(which_index == 1)
		{
			pwm_mode ^= 1;  //0--mul 1//div
		}
	}
	
	else if(key_up == 4)
	{
		if((which_index == 2) && (have_del_flag == 0))
		{
			if(which_ch == 4)
				which_ch = 5;
			else
				which_ch = 4;
		}		
	
		else if((which_index == 2) && (have_del_flag == 1))		
			have_del_flag = 0;
		
	}
	
	if(which_index == 2)
	{
		switch(key_val)
		{
			case 4:
				del_count++;
				if(del_count == 11)
					del_flag = 1;
				break;
				
			default:
				del_count = 0;
				break;
		}
	}
	
}


void led_proc(void)
{
	if(uwTick - uwTick_led<50)	return;
	uwTick_led = uwTick;
	
	led_disp(led);
}

void pwm_proc(void)
{
	if(uwTick - uwTick_pwm<50)	return;
	uwTick_pwm = uwTick;
	
	if(pwm_mode == 0)
	{
		__HAL_TIM_SetAutoreload(&htim3, pwm_in_t/X_real);
		__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, __HAL_TIM_GetAutoreload(&htim3)*0.5);
	}
	
	else 
	{
		__HAL_TIM_SetAutoreload(&htim3, pwm_in_t*X_real);
		__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, __HAL_TIM_GetAutoreload(&htim3)*0.5);	
	}
	
}


void adc_proc(void)
{
	int i;
	float sum_AO1;
	float sum_AO2;
	
	if(del_flag == 1)
	{
		if(which_ch == 4)
		{
			N_1 = 0;
			AO1_A = 0;
			AO1_T = 0;
			AO1_H = 0;		
		}
		
		else 
		{
			N_2 = 0;
			AO2_A = 0;
			AO2_T = 0;
			AO2_H = 0;				
		}

		del_flag = 0;
		have_del_flag = 1;
	}
	
	if(get_val_once == 1)
	{
		HAL_Delay(1);
		get_AO1();
		HAL_Delay(1);
		AO1_V = Kadc*(float)get_AO1();
		
		HAL_Delay(1);
		get_AO2();
		HAL_Delay(1);
		AO2_V = Kadc*(float)get_AO2();	
	
		AO1_buf[N_1] = AO1_V;
		AO2_buf[N_2] = AO2_V;

		if(N_1!=0)
		{
			if(AO1_A < AO1_V)
				AO1_A = AO1_V;
			if(AO1_T > AO1_V)
				AO1_T = AO1_V;
	
			for(i=0; i<=N_1; i++)
			{
				sum_AO1+=AO1_buf[i];
			}		

			AO1_H = sum_AO1/(N_1+1);
		}
		
		else 
		{
			AO1_A = AO1_V;
			AO1_T = AO1_V;
			
			AO1_H = AO1_V;
		}
		
		
		if(N_2!=0)
		{
			if(AO2_A < AO2_V)
				AO2_A = AO2_V;
			if(AO2_T > AO2_V)
				AO2_T = AO2_V;
	
			for(i=0; i<=N_2; i++)
			{
				sum_AO2+=AO2_buf[i];
			}	

			AO2_H = sum_AO2/(N_2+1);
		}
		
		else 
		{
			AO2_A = AO2_V;
			AO2_T = AO2_V;
			
			AO2_H = AO2_V;
		}
		
		
		N_1++;
		N_2++;
		
		get_val_once = 0;
	}

}


void lcd_proc(void)
{
	if(uwTick - uwTick_lcd<50)	return;
	uwTick_lcd = uwTick;

	if(which_index == 0)
	{
		sprintf((char *)string_lcd, "        DATA");
		LCD_DisplayStringLine(Line1, string_lcd);
		
		sprintf((char *)string_lcd, "     PA4=%.2f  ", AO1_V);
		LCD_DisplayStringLine(Line3, string_lcd);
		
		sprintf((char *)string_lcd, "     PA5=%.2f  ", AO2_V);
		LCD_DisplayStringLine(Line4, string_lcd);
		
		sprintf((char *)string_lcd, "     PA1=%d    ", (1000000/pwm_in_t));
		LCD_DisplayStringLine(Line5, string_lcd);	
	}
	
	else if(which_index == 1)
	{
		sprintf((char *)string_lcd, "        PARA");
		LCD_DisplayStringLine(Line1, string_lcd);
		
		sprintf((char *)string_lcd, "     X=%d  ", (uint32_t)X);
		LCD_DisplayStringLine(Line3, string_lcd);	
	
		sprintf((char *)string_lcd, "     Y=%d  ", (uint32_t)Y);
		LCD_DisplayStringLine(Line4, string_lcd);		
	}
	
	else 
	{
		sprintf((char *)string_lcd, "        REC-PA%d", (uint32_t)which_ch);
		LCD_DisplayStringLine(Line1, string_lcd);
		
		if(which_ch == 4)
		{
			sprintf((char *)string_lcd, "     N=%d  ", (uint32_t)N_1);
			LCD_DisplayStringLine(Line3, string_lcd);	
		
			sprintf((char *)string_lcd, "     A=%.2f   ", AO1_A);
			LCD_DisplayStringLine(Line4, string_lcd);	
			
			sprintf((char *)string_lcd, "     T=%.2f   ", AO1_T);
			LCD_DisplayStringLine(Line5, string_lcd);	
			
			sprintf((char *)string_lcd, "     H=%.2f   ", AO1_H);
			LCD_DisplayStringLine(Line6, string_lcd);				
		}

		else 
		{
			sprintf((char *)string_lcd, "     N=%d  ", (uint32_t)N_2);
			LCD_DisplayStringLine(Line3, string_lcd);	
		
			sprintf((char *)string_lcd, "     A=%.2f   ", AO2_A);
			LCD_DisplayStringLine(Line4, string_lcd);	
			
			sprintf((char *)string_lcd, "     T=%.2f   ", AO2_T);
			LCD_DisplayStringLine(Line5, string_lcd);	
			
			sprintf((char *)string_lcd, "     H=%.2f   ", AO2_H);
			LCD_DisplayStringLine(Line6, string_lcd);						
		}
		
	}
		

}



void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
	if(htim->Instance == TIM2)
	{
		if(htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2)
		{
			pwm_in_t = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_2);	
		}
	}
}


void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	
	if(uart_count == 0)
		uwTick_read_uart = uwTick;
	
	Read_buf[uart_count] = rx_buf[0];
	uart_count++;

	HAL_UART_Receive_IT(&huart1, (uint8_t *)rx_buf, 1);
}

void uart_proc(void)
{
	if(uwTick - uwTick_read_uart < 100)	return;
	
	if(uart_count!=0)
	{
		int i;
		if(Read_buf[0] == 'X')
		{
			sprintf(tx_buf, "X:%d", (uint32_t)X_real);
			HAL_UART_Transmit(&huart1, (uint8_t *)tx_buf, strlen(tx_buf), 50);	
		}
		
		else if(Read_buf[0] == 'Y')
		{
			sprintf(tx_buf, "Y:%d", (uint32_t)Y_real);
			HAL_UART_Transmit(&huart1, (uint8_t *)tx_buf, strlen(tx_buf), 50);	
		}
		
		else if(Read_buf[0] == 'P' && Read_buf[1] == 'A' && Read_buf[2] == '1')
		{
			sprintf(tx_buf, "PA1:%d", (1000000/pwm_in_t));
			HAL_UART_Transmit(&huart1, (uint8_t *)tx_buf, strlen(tx_buf), 50);	
		}

		else if(Read_buf[0] == 'P' && Read_buf[1] == 'A' && Read_buf[2] == '4')
		{
			sprintf(tx_buf, "PA4:%.2f", AO1_V);
			HAL_UART_Transmit(&huart1, (uint8_t *)tx_buf, strlen(tx_buf), 50);	
		}

		else if(Read_buf[0] == 'P' && Read_buf[1] == 'A' && Read_buf[2] == '5')
		{
			sprintf(tx_buf, "PA5:%.2f", AO2_V);
			HAL_UART_Transmit(&huart1, (uint8_t *)tx_buf, strlen(tx_buf), 50);	
		}		
		
		else if(Read_buf[0] == '#')
		{
			if(which_ch == 4)
				which_ch = 5;
			else
				which_ch = 4;
		}
				
		else 
		{
			//uwTick_read_uart = uwTick;
		}
		
		for(i=0; i<uart_count; i++)
		{
			Read_buf[i] = 0;
		}
		
		uart_count = 0;	
	}
}



void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

