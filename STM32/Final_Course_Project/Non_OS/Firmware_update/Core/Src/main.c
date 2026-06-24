
#include "main.h"
#include <math.h>
#include <string.h>

#define GPIOD_BASE_ADDR 	0x40020C00
#define USART1_BASE_ADDR	0x40011000
#define RCC_BASE_ADDR      	0x40023800
#define GPIOB_BASE_ADDR	   	0x40020400
#define I2C1_BASE_ADDR     	0x40005400
#define USART2_BASE_ADDR	0x40004400

void LedInit()
{
	uint32_t* RCC_AHB1ENR = (uint32_t*)(RCC_BASE_ADDR + 0x30);
	*RCC_AHB1ENR |= (1 << 3);			//Enable clock GPIOD

	uint32_t* GPIOD_MODER = (uint32_t*)(GPIOD_BASE_ADDR + 0x00);
	*GPIOD_MODER &= ~((0b11 << 24) | (0b11 << 26) | (0b11 << 28) | (0b11 << 30));
	*GPIOD_MODER |= ((0b01 << 24) | (0b01 << 26) | (0b01 << 28) | (0b01 << 30));		// PD12-15: Output
}
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
    if (value < 0)
    {
        UART_Transmit('-');
        value = -value;
    }

    int int_part  = (int)value;
    int frac_part = (int)((value - int_part) * 100);

    UART_send_number(int_part);
    UART_Transmit('.');

    if (frac_part < 10)
        UART_Transmit('0');  // In số 0 nếu < 10

    UART_send_number(frac_part);
    UART_Transmit('\r');
    UART_Transmit('\n');
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
    *CR1 |= (1 << 10);  // BẬT lại ACK
    *CR1 |= (1 << 0);      // Bật I2C
}

void I2C_master_transmit_accelerometer_init()
{
    volatile uint32_t* CR1 = (uint32_t*)(I2C1_BASE_ADDR + 0x00);
    volatile uint32_t* SR1 = (uint32_t*)(I2C1_BASE_ADDR + 0x14);
    volatile uint32_t* SR2 = (uint32_t*)(I2C1_BASE_ADDR + 0x18);
    volatile uint32_t* DR  = (uint32_t*)(I2C1_BASE_ADDR + 0x10);

    // --- Cấu hình CTRL_REG1_A (0x20) = 0x57 ---
    *CR1 |= (1 << 8);                          // START
    while (((*SR1 >> 0) & 1) == 0);            // SB = 1

    *DR = (0x19 << 1);                         // Địa chỉ accelerometer (ghi)
    while (((*SR1 >> 1) & 1) == 0);            // ADDR = 1
    (void)*SR2;

    while (((*SR1 >> 7) & 1) == 0);            // TxE = 1
    *DR = 0x20;                                // Gửi địa chỉ CTRL_REG1_A

    while (((*SR1 >> 7) & 1) == 0);
    *DR = 0x57;                                // 100Hz, enable XYZ

    while (((*SR1 >> 2) & 1) == 0);            // BTF = 1
    *CR1 |= (1 << 9);                          // STOP
    while (*CR1 & (1 << 9));					// Wait until STOP is cleared

    // --- Cấu hình CTRL_REG4_A (0x23) = 0x00 (±2g) ---
    *CR1 |= (1 << 8);                          // START
    while (((*SR1 >> 0) & 1) == 0);            // SB = 1

    *DR = (0x19 << 1);                         // Gửi địa chỉ accelerometer (ghi)
    while (((*SR1 >> 1) & 1) == 0);            // ADDR = 1
    (void)*SR2;

    while (((*SR1 >> 7) & 1) == 0);
    *DR = 0x23;                                // Gửi địa chỉ CTRL_REG4_A

    while (((*SR1 >> 7) & 1) == 0);
    *DR = 0x00;                                // ±2g, high-res mode

    while (((*SR1 >> 2) & 1) == 0);            // BTF = 1
    *CR1 |= (1 << 9);                          // STOP
    while (*CR1 & (1 << 9));					// Wait until STOP is cleared


}

void I2C1_Master_ReadMulti(uint8_t slave_addr, uint8_t reg_start, uint8_t* buf, uint8_t len)
{
    volatile uint32_t* CR1 = (uint32_t*)(I2C1_BASE_ADDR + 0x00);
    volatile uint32_t* SR1 = (uint32_t*)(I2C1_BASE_ADDR + 0x14);
    volatile uint32_t* SR2 = (uint32_t*)(I2C1_BASE_ADDR + 0x18);
    volatile uint32_t* DR  = (uint32_t*)(I2C1_BASE_ADDR + 0x10);

    // --- Gửi START + địa chỉ + reg ---
    *CR1 |= (1 << 8);                          // START
    while (!(*SR1 & (1 << 0)));                // SB
    *DR = slave_addr << 1;                     // WRITE
    while (!(*SR1 & (1 << 1)));                // ADDR
    (void)*SR2;

    while (!(*SR1 & (1 << 7)));                // TxE
    *DR = reg_start | 0x80;                    // auto-increment

    while (!(*SR1 & (1 << 7)));                 // BTF

    *CR1 |= (1 << 10);

    // --- Re-START + đọc ---
    *CR1 |= (1 << 8);                          // Re-START
    while (!(*SR1 & (1 << 0)));                // SB
    *DR = (slave_addr << 1) | 1;               // READ
    while (!(*SR1 & (1 << 1)));                // ADDR
    (void)*SR2;

    for (uint8_t i = 0; i < len; i++)
    {
        if (i == len - 1)
        {
            *CR1 &= ~(1 << 10); // NACK cho byte cuối
            *CR1 |= (1 << 9);   // STOP
        }
        while (!(*SR1 & (1 << 6))); // RxNE
        buf[i] = *DR;
    }
}


void LSM303AGR_Accelerometer_ReadXYZ(int16_t* x, int16_t* y, int16_t* z)
{
    uint8_t buf[6];
    I2C1_Master_ReadMulti(0x19, 0x28, buf, 6);  // auto-increment từ 0x28

    *x = (int16_t)((buf[1] << 8) | buf[0]);
    *y = (int16_t)((buf[3] << 8) | buf[2]);
    *z = (int16_t)((buf[5] << 8) | buf[4]);
}
void calculate_pitch_roll(float ax, float ay, float az, float* pitch, float* roll)
{
    // Đảm bảo az != 0 để tránh chia 0
    if (pitch != NULL)
    {
        *pitch = atan2f(-ax, sqrtf(ay * ay + az * az)) * 180.0f / M_PI;
    }

    if (roll != NULL)
    {
        *roll = atan2f(ay, az) * 180.0f / M_PI;
    }
}

float pitch = 0.0f, roll = 0.0f;
#define GPIOD_ODR  (*(volatile uint32_t*)(GPIOD_BASE_ADDR + 0x14))

void detect_tilt_direction(float pitch, float roll)
{
    static uint8_t led_toggle = 0;
    led_toggle ^= 1; // Đảo trạng thái mỗi lần gọi hàm

    // Tắt tất cả LED trước
    GPIOD_ODR &= ~((1 << 12) | (1 << 13) | (1 << 14) | (1 << 15));

    // Xác định hướng nghiêng & nhấp nháy LED tương ứng
    if (pitch > 3.0f && led_toggle)
    {
        GPIOD_ODR |= (1 << 13);  // LED trước (PD13)
        UART_send_float(pitch);
    }
    else if (pitch < -3.0f && led_toggle)
    {
        GPIOD_ODR |= (1 << 15);  // LED sau (PD15)
        UART_send_float(pitch);
    }
    else if (roll > 3.0f && led_toggle)
    {
        GPIOD_ODR |= (1 << 12);  // LED phải (PD14)
        UART_send_float(roll);
    }
    else if (roll < -3.0f && led_toggle)
    {
        GPIOD_ODR |= (1 << 14);  // LED trái (PD12)
        UART_send_float(roll);
    }

    // LED sẽ tắt ở lần kế tiếp → tạo hiệu ứng nhấp nháy
}
int main()
{
    HAL_Init();
    UART_init();
    LedInit();
    I2C1_Master_Init();                        // Khởi tạo I2C
    I2C_master_transmit_accelerometer_init();
    HAL_Delay(15);
    float ax_g, ay_g, az_g;
    while (1)
    {
        int16_t ax_raw, ay_raw, az_raw;

        float pitch, roll;

        LSM303AGR_Accelerometer_ReadXYZ(&ax_raw, &ay_raw, &az_raw);

        ax_g = ax_raw * 0.000061f;
        ay_g = ay_raw * 0.000061f;
        az_g = az_raw * 0.000061f;

        calculate_pitch_roll(ax_g, ay_g, az_g, &pitch, &roll);
        detect_tilt_direction(pitch, roll);

        HAL_Delay(100); // Nhấp nháy khoảng 10Hz
    }


}
