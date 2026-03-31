/*
 * Application 1: 0x08040000 128Kb
	-Đọc và trả về giá trị nhiệt độ chip qua ADC (mỗi 1 giây), hiện giá trị trên UART
	-Truyền tín hiệu ON LED  và OFF LED qua UART_interrupt để bật tắt led
 *
 */
#include "main.h"
#include <string.h>

#define ADC1_BASE_ADDR		0x40012000
#define RCC_BASE_ADDR		0x40023800
#define GPIOB_BASE_ADDR		0x40020400
#define USART1_BASE_ADDR	0X40011000
#define DMA2_BASE_ADDR		0x40026400
#define GPIOD_BASE_ADDR 	0x40020C00
#define USART2_BASE_ADDR	0x40004400


void LedInit()
{
	uint32_t* RCC_AHB1ENR = (uint32_t*)(RCC_BASE_ADDR + 0x30);
	*RCC_AHB1ENR |= (1 << 3);			//Enable clock GPIOD

	uint32_t* GPIOD_MODER = (uint32_t*)(GPIOD_BASE_ADDR + 0x00);
	*GPIOD_MODER &= ~((0b11 << 24) | (0b11 << 26) | (0b11 << 28) | (0b11 << 30));
	*GPIOD_MODER |= ((0b01 << 24) | (0b01 << 26) | (0b01 << 28) | (0b01 << 30));		// PD12-15: Output
}

void LedCtrl(int on_off)
{
	uint32_t* GPIOD_BSRR = (uint32_t*)(GPIOD_BASE_ADDR + 0x18);
    if(on_off == 1)
    {
        *GPIOD_BSRR = (1 << 12) | (1 << 13) | (1 << 14) | (1 << 15);         // PD12-15 ON
    }
    else
    {
        *GPIOD_BSRR = (1 << (12 + 16)) | (1 << (13 + 16)) | (1 << (14 + 16)) | (1 << (15 + 16));  // PD12-15 OFF
    }
}

char data[100];  //tạo mảng để các ký tự dữ liệu nhận được ghi lần lượt vào các phần tử mảng
int idx;	// tạo biến i để lấy thứ tự các phần tử trong mảng
void UART_init()
{
	uint32_t* RCC_AHB1ENR = (uint32_t*)(RCC_BASE_ADDR + 0x30);
	*RCC_AHB1ENR |= (1 << 0);			// Enable clock GPIOA

	uint32_t* RCC_APB1ENR = (uint32_t*)(RCC_BASE_ADDR + 0x40);
	*RCC_APB1ENR |= (1 << 17);			// Enable clock USART2

	uint32_t* GPIOA_MODER = (uint32_t*)(0x40020000 + 0x00);
	uint32_t* GPIOA_AFRL  = (uint32_t*)(0x40020000 + 0x20);	// AFRL vì PA2/PA3 < 8

	*GPIOA_MODER &= ~(0b11 << (2 * 2));
	*GPIOA_MODER |=  (0b10 << (2 * 2));			// PA2: Alternate Function
	*GPIOA_AFRL  &= ~(0xF << (2 * 4));
	*GPIOA_AFRL  |=  (0x7 << (2 * 4));			// AF7 cho PA2

	*GPIOA_MODER &= ~(0b11 << (3 * 2));
	*GPIOA_MODER |=  (0b10 << (3 * 2));			// PA3: Alternate Function
	*GPIOA_AFRL  &= ~(0xF << (3 * 4));
	*GPIOA_AFRL  |=  (0x7 << (3 * 4));			// AF7 cho PA3

	uint32_t* USART2_BRR = (uint32_t*)(USART2_BASE_ADDR + 0x08);
	*USART2_BRR &= ~(0xFFFF);
	*USART2_BRR |= (104 << 4) | (3 << 0);	// Baudrate 9600

	uint32_t* USART2_CR1 = (uint32_t*)(USART2_BASE_ADDR + 0x0C);
	*USART2_CR1 &= ~(1 << 13);			// Disable UART
	*USART2_CR1 &= ~(1 << 15);			// Oversampling = 16
	*USART2_CR1 &= ~(1 << 12);			// 8-bit
	*USART2_CR1 &= ~(1 << 10);			// No parity
	*USART2_CR1 &= ~(1 << 9);
	*USART2_CR1 |= (1 << 5);			// RXNE interrupt
	*USART2_CR1 |= (1 << 2);			// Enable receive
	*USART2_CR1 |= (1 << 3);			// Enable transmit
	*USART2_CR1 |= (1 << 13);			// Enable UART

	uint32_t* NVIC_ISER1 = (uint32_t*)(0xE000E100 + 0x04);
	*NVIC_ISER1 |= (1 << 6);			// USART2 = position 38 = ISER1, bit 6
}
void UART_Transmit(uint8_t data)
{
	uint32_t* USART_DR= (uint32_t*)(USART2_BASE_ADDR + 0x04);
	uint32_t* USART_SR= (uint32_t*)(USART2_BASE_ADDR + 0x00);
	while((*USART_SR & (1 << 7)) == 0);
	*USART_DR = data;
	while((*USART_SR & (1 << 6)) == 0);
}

char UART_recieve()
{
	uint32_t *UART_DR = (uint32_t*)(USART2_BASE_ADDR + 0x04);
	return *UART_DR;
}

void UART_print_log(char *msg)
{
	int msg_len = strlen(msg);
	for( int i = 0 ; i < msg_len ; i++)
	{
		UART_Transmit((uint8_t)msg[i]);
	}
}

void USART2_IRQHandler(void)
{
	if (idx < sizeof(data) - 1)
		data[idx++] = UART_recieve();
	else
	{
		idx = 0;
		memset(data, 0, sizeof(data));  // 🔄 Reset buffer nếu đầy
	}

	if (strstr(data, "LED ON"))
	{
		LedCtrl(1);
		UART_print_log(" - ON_OK\r\n");
		memset(data, 0, sizeof(data));
		idx = 0;
	}
	else if (strstr(data, "LED OFF"))
	{
		LedCtrl(0);
		UART_print_log(" - OFF_OK\r\n");
		memset(data, 0, sizeof(data));
		idx = 0;
	}
}

void UART_send_number(int num)
{
    char buf[10];
    int i = 0;
    if (num == 0)
    {
        UART_Transmit('0');
        return;
    }

    if (num < 0)
    {
        UART_Transmit('-');
        num = -num;
    }

    while (num > 0)
    {
        buf[i++] = (num % 10) + '0';
        num /= 10;
    }

    // In ngược lại
    for (int j = i - 1; j >= 0; j--)
    {
        UART_Transmit(buf[j]);
    }
}
void UART_send_float(float value)
{
    int int_part = (int)value;  // Phần nguyên
    int frac_part = (int)((value - int_part) * 100);  // Phần thập phân, giữ 2 số

    UART_send_number(int_part);
    UART_Transmit('.');
    if (frac_part < 10) UART_Transmit('0');  // In số 0 nếu < 10
    UART_send_number(frac_part);
    UART_Transmit('\r');
    UART_Transmit('\n');
}

void Temp_sensor_init()
{
	uint32_t* RCC_APB2ENR = (uint32_t*)(RCC_BASE_ADDR + 0x44);
	*RCC_APB2ENR |= (1 << 8);				//Bật clock ADC

	uint32_t* ADC_CR1	= (uint32_t*)(ADC1_BASE_ADDR + 0x04);
	uint32_t* ADC_CR2	= (uint32_t*)(ADC1_BASE_ADDR + 0x08);
	uint32_t* ADC_JSQR	= (uint32_t*)(ADC1_BASE_ADDR + 0x38);
	uint32_t* ADC_CCR	= (uint32_t*)(ADC1_BASE_ADDR + 0x300 + 0x04);
	uint32_t* ADC_SMPR1	= (uint32_t*)(ADC1_BASE_ADDR + 0x0C);

	*ADC_JSQR	|= (16 << 15);				//1 input
	*ADC_CCR	|= (1 << 23);				//Bật temperature sensor
	*ADC_SMPR1	|= (0b110 << 18);			// 84 cycles
	*ADC_CR1	&= ~(0b11 << 24);			//12 bit phân giải
	*ADC_CR2	|= (1 << 0);				//Enable ADC
}

float Temp_sensor_read()
{
	uint32_t* ADC_CR2	= (uint32_t*)(ADC1_BASE_ADDR + 0x08);
	uint32_t* ADC_SR	= (uint32_t*)(ADC1_BASE_ADDR + 0x00);
	uint32_t* ADC_JDR1	= (uint32_t*)(ADC1_BASE_ADDR + 0x3C);

	*ADC_CR2 |= (1 << 22);					//Start ADC
	while (((*ADC_SR >> 2) &1) ==0);		//Chờ đo xong
	*ADC_SR &= ~(1 << 2);					//Xóa cờ đo

	uint16_t data_raw = *ADC_JDR1;			//Đọc thanh ghi JDR1
	float Vin = (data_raw * 3.0)/4095.0;	//tính điện áp đo được
	float temperature = ((Vin - 0.76) /0.0025) +25.0 ;		//Chuyển điện áp thành nhiệt độ
	return temperature;
}
float temp;

int main()
{
    HAL_Init();
    Temp_sensor_init();
    LedInit();
    UART_init();
    while(1)
    {
    	temp = Temp_sensor_read();
    	UART_print_log("Temp: ");
    	UART_send_float(temp);
    	HAL_Delay(1200);

    }
}

