/*
 * Application 2: 0x08060000 128Kb
	-Xác định hướng NEWS (mỗi giây gửi giá trị về 1 lần qua UART)
	-Chứa code update firmware cho app1
	-Khi nhận được data (file bin cho update), sẽ nhảy vào IRQ và xóa app1, ghi lại firmware mới
 */

#include "main.h"
#include <math.h>
#include <string.h>

#define GPIOD_BASE_ADDR 	0x40020C00
#define USART1_BASE_ADDR	0x40011000
#define RCC_BASE_ADDR      	0x40023800
#define GPIOB_BASE_ADDR	   	0x40020400
#define I2C1_BASE_ADDR     	0x40005400
#define USART2_BASE_ADDR	0x40004400
#define FLASH_interface_BASE_ADDR 0x40023C00

#define MAX_FILE_SIZE 65536
uint8_t data[MAX_FILE_SIZE];
int idx = 0;

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
	*USART2_BRR |= (104 << 4) | (3 << 0);		// Baudrate 9600

	uint32_t* USART2_CR1 = (uint32_t*)(USART2_BASE_ADDR + 0x0C);
	*USART2_CR1 &= ~(1 << 13);			// Disable UART
	*USART2_CR1 &= ~(1 << 15);			// Oversampling = 16
	*USART2_CR1 &= ~(1 << 12);			// 8-bit
	*USART2_CR1 &= ~(1 << 10);			// No parity
	*USART2_CR1 &= ~(1 << 9);
	*USART2_CR1 |= (1 << 5);			// RXNE interrupt
	*USART2_CR1 |= (1 << 4);			// IDLE interrupt
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

uint8_t UART_recieve()
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
    int int_part = (int)value;						// Phần nguyên
    int frac_part = (int)((value - int_part) * 100);// Phần thập phân, giữ 2 số

    UART_send_number(int_part);
    UART_Transmit('.');
    if (frac_part < 10) UART_Transmit('0');			// In số 0 nếu < 10
    UART_send_number(frac_part);
    UART_Transmit('\r');
    UART_Transmit('\n');
}
void Flash_erase_sector (int sec_num)
{
	uint32_t* FLASH_CR = (uint32_t*)(FLASH_interface_BASE_ADDR + 0x10);
	uint32_t* FLASH_SR = (uint32_t*)(FLASH_interface_BASE_ADDR + 0x0C);
	uint32_t* FLASH_KEYR = (uint32_t*)(FLASH_interface_BASE_ADDR + 0x04);

	*FLASH_SR |= (1 << 5) | (1 << 6) | (1 << 7) | (1 << 8);//Xóa cờ lỗi

	while(((*FLASH_SR >> 16) &1) ==1);		//Đợi FLASH không bận

	*FLASH_KEYR = 0x45670123;				//Mở khóa
	*FLASH_KEYR = 0xCDEF89AB;

	*FLASH_CR &= ~(0b11 << 8);     			// Xóa PSIZE Chọn size ghi dữ liệu
	*FLASH_CR |=  (0b10 << 8);				// Gán lại 32-bit

	//Chọn sector
	if ((sec_num >= 0) && (sec_num <= 7))
	{
		*FLASH_CR &= ~(0b1111 << 3);			// Xóa SNB cũ
		*FLASH_CR |=  ((sec_num & 0xF) << 3); 	// Gán SNB mới
	}
	else if (sec_num > 7)
	{
		while (1);	//Lỗi sector
	}

	*FLASH_CR |= (1 << 1);				//Bật sector erase

	*FLASH_CR |= (1 << 16);				//Gửi lệnh xóa
	while(((*FLASH_SR >> 16) &1) ==1);	//Chờ xóa xong

	*FLASH_CR &= ~(1 << 1);				//Tắt chế độ xóa sector

	*FLASH_CR |= (1 << 31);				//Khóa Flash
}
void Flash_program(uint8_t* addr, uint8_t data)
{
	uint32_t* FLASH_CR   = (uint32_t*)(FLASH_interface_BASE_ADDR + 0x10);
	uint32_t* FLASH_SR   = (uint32_t*)(FLASH_interface_BASE_ADDR + 0x0C);
	uint32_t* FLASH_KEYR = (uint32_t*)(FLASH_interface_BASE_ADDR + 0x04);

	*FLASH_SR |= (1 << 5) | (1 << 6) | (1 << 7) | (1 << 8); // Xóa cờ lỗi

	while ((*FLASH_SR & (1 << 16)) != 0);	// Đợi FLASH không bận

	*FLASH_KEYR = 0x45670123;				//Mở khóa
	*FLASH_KEYR = 0xCDEF89AB;

	*FLASH_CR &= ~(0b11 << 8);     			// Xóa PSIZE
	*FLASH_CR |=  (0b00 << 8);     			// Chọn PSIZE = 0b00 (8-bit)

	*FLASH_CR |= (1 << 0); 					// Bật chế độ ghi

	*addr = data;							// Ghi 1 byte
	while (*FLASH_SR & (1 << 16));			// Đợi ghi xong

	*FLASH_CR &= ~(1 << 0);					// Tắt chế độ ghi

	*FLASH_CR |= (1 << 31);					// Khóa lại Flash
}

char msg[] = "hello";

void USART2_IRQHandler(void)
{
    uint32_t* USART2_SR = (uint32_t*)(USART2_BASE_ADDR + 0x00);

    // Nhận tất cả byte đang có trong FIFO
    while (*USART2_SR & (1 << 5))  // RXNE
    {
        if (idx < MAX_FILE_SIZE)
            data[idx++] = UART_recieve();
    }

    // Khi UART không còn nhận thêm byte → kết thúc
    if (*USART2_SR & (1 << 4))  // IDLE
    {
        (void)*(volatile uint32_t*)(USART2_BASE_ADDR + 0x04);  // Clear IDLE flag

        Flash_erase_sector(6);
        uint8_t* flash_ptr = (uint8_t*)0x08040000;

        for (int i = 0; i < idx; i++)
            Flash_program(flash_ptr + i, data[i]);

        idx = 0;
    }
}

void I2C1_Master_Init() // PB6=SCL, PB9=SDA
{
    // Clock GPIOB + I2C1
    volatile uint32_t* RCC_AHB1ENR = (uint32_t*)(RCC_BASE_ADDR + 0x30);
    volatile uint32_t* RCC_APB1ENR = (uint32_t*)(RCC_BASE_ADDR + 0x40);
    *RCC_AHB1ENR |= (1 << 1);
    *RCC_APB1ENR |= (1 << 21);

    // Cấu hình chân PB6 + PB9
    volatile uint32_t* MODER   = (uint32_t*)(GPIOB_BASE_ADDR + 0x00);
    volatile uint32_t* OTYPER  = (uint32_t*)(GPIOB_BASE_ADDR + 0x04);
    volatile uint32_t* OSPEEDR = (uint32_t*)(GPIOB_BASE_ADDR + 0x08);
    volatile uint32_t* PUPDR   = (uint32_t*)(GPIOB_BASE_ADDR + 0x0C);
    volatile uint32_t* AFRL    = (uint32_t*)(GPIOB_BASE_ADDR + 0x20);
    volatile uint32_t* AFRH    = (uint32_t*)(GPIOB_BASE_ADDR + 0x24);

    // AF mode
    *MODER &= ~((0b11 << (6 * 2)) | (0b11 << (9 * 2)));
    *MODER |=  ((0b10 << (6 * 2)) | (0b10 << (9 * 2)));

    // Open-drain
    *OTYPER |= (1 << 6) | (1 << 9);

    // High speed
    *OSPEEDR &= ~((0b11 << (6 * 2)) | (0b11 << (9 * 2)));
    *OSPEEDR |=  ((0b11 << (6 * 2)) | (0b11 << (9 * 2)));

    // Pull-up
    *PUPDR &= ~((0b11 << (6 * 2)) | (0b11 << (9 * 2)));
    *PUPDR |=  ((0b01 << (6 * 2)) | (0b01 << (9 * 2)));

    // AF4 cho PB6 + PB9
    *AFRL &= ~(0xF << (6 * 4));
    *AFRL |=  (0x4 << (6 * 4));
    *AFRH &= ~(0xF << ((9 - 8) * 4));
    *AFRH |=  (0x4 << ((9 - 8) * 4));

    // Cấu hình I2C1
    volatile uint32_t* CR1   = (uint32_t*)(I2C1_BASE_ADDR + 0x00);
    volatile uint32_t* CR2   = (uint32_t*)(I2C1_BASE_ADDR + 0x04);
    volatile uint32_t* CCR   = (uint32_t*)(I2C1_BASE_ADDR + 0x1C);
    volatile uint32_t* TRISE = (uint32_t*)(I2C1_BASE_ADDR + 0x20);

    *CR1 &= ~(1 << 0);     // Tắt I2C
    *CR2 = 16;             // Fpclk = 16MHz
    *CCR = 80;             // Speed 100kHz
    *TRISE = 17;           // TRISE = Fpclk / 1M + 1
    *CR1 |= (1 << 0);      // Bật I2C
}

void I2C_master_transmit_magnetometer_init()
{
    volatile uint32_t* CR1 = (uint32_t*)(I2C1_BASE_ADDR + 0x00);
    volatile uint32_t* SR1 = (uint32_t*)(I2C1_BASE_ADDR + 0x14);
    volatile uint32_t* SR2 = (uint32_t*)(I2C1_BASE_ADDR + 0x18);
    volatile uint32_t* DR  = (uint32_t*)(I2C1_BASE_ADDR + 0x10);

    *CR1 |= (1 << 8);                          // START
    while (((*SR1 >> 0) & 1) == 0);            // Đợi SB = 1

    *DR = (0x1E << 1)|0;                       // Gửi địa chỉ slave + Write
    while (((*SR1 >> 1) & 1) == 0);            // ADDR = 1
    (void)*SR2;

    while (((*SR1 >> 7) & 1) == 0);            // TxE = 1
    *DR = 0x60;                                // Gửi địa chỉ thanh ghi

    while (((*SR1 >> 7) & 1) == 0);            // TxE = 1
    *DR = 0x00;                                // Gửi dữ liệu

    while (((*SR1 >> 2) & 1) == 0);            // BTF = 1

    *CR1 |= (1 << 9);                          // STOP
}

uint8_t I2C1_Master_ReadRegister(uint8_t slave_addr, uint8_t reg_addr)
{
    volatile uint32_t* CR1 = (uint32_t*)(I2C1_BASE_ADDR + 0x00);
    volatile uint32_t* SR1 = (uint32_t*)(I2C1_BASE_ADDR + 0x14);
    volatile uint32_t* SR2 = (uint32_t*)(I2C1_BASE_ADDR + 0x18);
    volatile uint32_t* DR  = (uint32_t*)(I2C1_BASE_ADDR + 0x10);

    uint8_t data = 0;

    *CR1 |= (1 << 8);                          // START
    while (((*SR1 >> 0) & 1) == 0);            // SB

    *DR = (slave_addr << 1);                  // Addr + Write
    while (((*SR1 >> 1) & 1) == 0);            // ADDR
    (void)*SR2;

    while (((*SR1 >> 7) & 1) == 0);            // TxE
    *DR = reg_addr;                            // Gửi địa chỉ thanh ghi

    while (((*SR1 >> 2) & 1) == 0);            // BTF

    *CR1 |= (1 << 8);                          // Re-START
    while (((*SR1 >> 0) & 1) == 0);            // SB

    *DR = (slave_addr << 1) | 1;              // Addr + Read
    while (((*SR1 >> 1) & 1) == 0);            // ADDR
    *CR1 &= ~(1 << 10);                        // NACK sau 1 byte
    (void)*SR2;

    *CR1 |= (1 << 9);                          // STOP

    while (((*SR1 >> 6) & 1) == 0);            // RxNE
    data = *DR;

    return data;
}
void LSM303AGR_Magnetometer_ReadXYZ(int16_t* x, int16_t* y, int16_t* z)
{
    uint8_t xl = I2C1_Master_ReadRegister(0x1E, 0x68);
    uint8_t xh = I2C1_Master_ReadRegister(0x1E, 0x69);
    uint8_t yl = I2C1_Master_ReadRegister(0x1E, 0x6A);
    uint8_t yh = I2C1_Master_ReadRegister(0x1E, 0x6B);
    uint8_t zl = I2C1_Master_ReadRegister(0x1E, 0x6C);
    uint8_t zh = I2C1_Master_ReadRegister(0x1E, 0x6D);

    *x = (int16_t)((xh << 8) | xl);
    *y = (int16_t)((yh << 8) | yl);
    *z = (int16_t)((zh << 8) | zl);
}
int16_t mx, my, mz;
float heading;
void uart_print_navigation()
{
    if ((heading >= 0 && heading <= 20) || (heading >= 341 && heading <= 360))
        UART_print_log("North ");

    else if (heading >= 21 && heading <= 65)
        UART_print_log("North-East ");

    else if (heading >= 66 && heading <= 110)
        UART_print_log("East ");

    else if (heading >= 111 && heading <= 155)
        UART_print_log("South-East ");

    else if (heading >= 156 && heading <= 200)
        UART_print_log("South ");

    else if (heading >= 201 && heading <= 250)
        UART_print_log("South-West ");

    else if (heading >= 251 && heading <= 290)
        UART_print_log("West ");

    else if (heading >= 291 && heading <= 340)
        UART_print_log("North-West ");
}

int main()
{
    HAL_Init();
    HAL_Delay(20);
    UART_init();
    I2C1_Master_Init();                        // Khởi tạo I2C
    I2C_master_transmit_magnetometer_init();   // Bật chế độ continuous mode cho magnetometer

    while (1)
    {
        LSM303AGR_Magnetometer_ReadXYZ(&mx, &my, &mz);  // Đọc dữ liệu từ cảm biến

        float offset = 2.0f;  // Ví dụ lệch 15 độ so với Bắc thực tế

        heading = atan2f((float)my, (float)mx) * 180.0f / M_PI;
        heading = -heading;
        heading += offset;

        if (heading < 0.0f) heading += 360.0f;
        if (heading >= 360.0f) heading -= 360.0f;
        uart_print_navigation();
        UART_send_float(heading);
        HAL_Delay(150);
    }
}
