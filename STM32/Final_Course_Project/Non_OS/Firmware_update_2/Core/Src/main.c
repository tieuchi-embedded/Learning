#include "main.h"

#define RCC_BASE_ADDR       0x40023800
#define GPIOA_BASE_ADDR     0x40020000
#define GPIOE_BASE_ADDR     0x40021000
#define GPIOD_BASE_ADDR 	0x40020C00
#define SPI1_BASE_ADDR      0x40013000


int16_t gyro_x, gyro_y, gyro_z;
float dps_x, dps_y, dps_z;

void LedInit()
{
	uint32_t* RCC_AHB1ENR = (uint32_t*)(RCC_BASE_ADDR + 0x30);
	*RCC_AHB1ENR |= (1 << 3);			//Enable clock GPIOD

	uint32_t* GPIOD_MODER = (uint32_t*)(GPIOD_BASE_ADDR + 0x00);
	*GPIOD_MODER &= ~((0b11 << 24) | (0b11 << 26) | (0b11 << 28) | (0b11 << 30));
	*GPIOD_MODER |= ((0b01 << 24) | (0b01 << 26) | (0b01 << 28) | (0b01 << 30));		// PD12-15: Output
}
void Led_ctrl(uint8_t led_num, uint8_t state)
{
    volatile uint32_t* ODR = (uint32_t*)(GPIOD_BASE_ADDR + 0x14); // Output Data Register

    uint16_t led_pin = (1 << (12 + led_num)); // led_num: 0 → PD12, 1 → PD13, ...

    if (state)
        *ODR |= led_pin;     // Bật LED
    else
        *ODR &= ~led_pin;    // Tắt LED
}

void SPI_master_init()
{
    // 1. Enable clock cho GPIOA, GPIOE, SPI1
    volatile uint32_t* RCC_AHB1ENR  = (uint32_t*)(RCC_BASE_ADDR + 0x30);
    volatile uint32_t* RCC_APB2ENR  = (uint32_t*)(RCC_BASE_ADDR + 0x44);
    *RCC_AHB1ENR |= (1 << 0) | (1 << 4);   // GPIOA + GPIOE
    *RCC_APB2ENR |= (1 << 12);            // SPI1

    // 2. Cấu hình PA5 (SCK), PA6 (MISO), PA7 (MOSI)
    volatile uint32_t* MODER_A   = (uint32_t*)(GPIOA_BASE_ADDR + 0x00);
    volatile uint32_t* OTYPER_A  = (uint32_t*)(GPIOA_BASE_ADDR + 0x04);
    volatile uint32_t* OSPEEDR_A = (uint32_t*)(GPIOA_BASE_ADDR + 0x08);
    volatile uint32_t* PUPDR_A   = (uint32_t*)(GPIOA_BASE_ADDR + 0x0C);
    volatile uint32_t* AFRL_A    = (uint32_t*)(GPIOA_BASE_ADDR + 0x20);

    *MODER_A &= ~(0xFF << 8);
    *MODER_A |=  (0b10 << (5 * 2)) | (0b10 << (6 * 2)) | (0b10 << (7 * 2)); // AF
    *OTYPER_A &= ~((1 << 5) | (1 << 6) | (1 << 7));                         // Push-pull
    *OSPEEDR_A |= (0b11 << (5 * 2)) | (0b11 << (6 * 2)) | (0b11 << (7 * 2)); // High speed
    *PUPDR_A &= ~(0b11 << (6 * 2));
    *PUPDR_A |=  (0b01 << (6 * 2));                                        // Pull-up MISO
    *AFRL_A &= ~(0xFFF << 20);
    *AFRL_A |=  (5 << 20) | (5 << 24) | (5 << 28);                          // AF5

    // 3. Cấu hình PE3 làm CS
    volatile uint32_t* MODER_E   = (uint32_t*)(GPIOE_BASE_ADDR + 0x00);
    volatile uint32_t* OTYPER_E  = (uint32_t*)(GPIOE_BASE_ADDR + 0x04);
    volatile uint32_t* OSPEEDR_E = (uint32_t*)(GPIOE_BASE_ADDR + 0x08);
    volatile uint32_t* ODR_E     = (uint32_t*)(GPIOE_BASE_ADDR + 0x14);

    *MODER_E &= ~(0b11 << (3 * 2));
    *MODER_E |=  (0b01 << (3 * 2));         // Output
    *OTYPER_E &= ~(1 << 3);                // Push-pull
    *OSPEEDR_E |= (0b11 << (3 * 2));       // High speed
    *ODR_E |= (1 << 3);                    // CS = HIGH

    // 4. Cấu hình SPI1: master, fPCLK/16, mode 0
    volatile uint32_t* CR1 = (uint32_t*)(SPI1_BASE_ADDR + 0x00);
    volatile uint32_t* CR2 = (uint32_t*)(SPI1_BASE_ADDR + 0x04);

    *CR1 = 0;
    *CR2 = 0;
    *CR1 |= (1 << 2)     // MSTR = master
          | (0b011 << 3) // Baudrate = fPCLK/16
          | (1 << 8)     // SSI = 1
          | (1 << 9);    // SSM = software NSS
    *CR1 |= (1 << 6);    // SPE = enable SPI
}

void SPI_master_transmit(uint8_t data)
{
    volatile uint32_t* SR  = (uint32_t*)(SPI1_BASE_ADDR + 0x08);
    volatile uint32_t* DR  = (uint32_t*)(SPI1_BASE_ADDR + 0x0C);
    volatile uint32_t* ODR = (uint32_t*)(GPIOE_BASE_ADDR + 0x14); // 👉 PE3 làm CS

    // 1. Kéo CS (PE3) xuống LOW
    *ODR &= ~(1 << 3);

    // 2. Chờ TXE = 1 (TX buffer trống)
    while(((*SR >> 1) & 1) == 0);

    // 3. Gửi data
    *DR = data;

    // 4. Chờ TXE = 1 lần nữa
    while(((*SR >> 1) & 1) == 0);

    // 5. Chờ BSY = 0 (SPI không bận)
    while(((*SR >> 7) & 1) == 1);

    // 6. Kéo CS lên lại HIGH
    *ODR |= (1 << 3);

    // 7. Đọc DR và SR để clear OVR flag
    uint8_t temp = *DR;
    temp = *SR;
    (void)temp;
}


void I3G4250D_write_register(uint8_t reg, uint8_t data)
{
    volatile uint32_t* SR = (uint32_t*)(SPI1_BASE_ADDR + 0x08);
    volatile uint32_t* DR = (uint32_t*)(SPI1_BASE_ADDR + 0x0C);
    volatile uint32_t* ODR = (uint32_t*)(GPIOE_BASE_ADDR + 0x14); // PE3 là CS

    reg &= 0x3F; // bit 7=0 (write), bit 6=0 (1 byte)

    // CS LOW
    *ODR &= ~(1 << 3);

    // Gửi địa chỉ
    while(((*SR >> 1) & 1) == 0);
    *DR = reg;

    // Gửi dữ liệu
    while(((*SR >> 1) & 1) == 0);
    *DR = data;

    // Chờ truyền xong
    while(((*SR >> 7) & 1) == 1);

    // CS HIGH
    *ODR |= (1 << 3);

    // Clear overrun (nếu có)
    uint8_t temp = *DR;
    temp = *SR;
    (void)temp;
}
uint8_t I3G4250D_read_register(uint8_t reg)
{
    volatile uint32_t* SR = (uint32_t*)(SPI1_BASE_ADDR + 0x08);
    volatile uint32_t* DR = (uint32_t*)(SPI1_BASE_ADDR + 0x0C);
    volatile uint32_t* ODR = (uint32_t*)(GPIOE_BASE_ADDR + 0x14); // PE3 là CS

    reg |= 0x80; // bit 7 = 1 (read)

    // CS LOW
    *ODR &= ~(1 << 3);

    // Gửi địa chỉ cần đọc
    while(((*SR >> 1) & 1) == 0);
    *DR = reg;

    // Chờ nhận lại byte
    while(((*SR >> 1) & 1) == 0);
    *DR = 0xFF; // Dummy byte để clock ra dữ liệu

    // Chờ nhận xong
    while(((*SR >> 0) & 1) == 0);
    uint8_t data = *DR;

    // CS HIGH
    *ODR |= (1 << 3);

    return data;
}
void I3G4250D_Write_Register(uint8_t reg, uint8_t data)
{
    volatile uint32_t* SR = (uint32_t*)(SPI1_BASE_ADDR + 0x08);
    volatile uint32_t* DR = (uint32_t*)(SPI1_BASE_ADDR + 0x0C);
    volatile uint32_t* ODR = (uint32_t*)(GPIOE_BASE_ADDR + 0x14); // PE3 là CS

    reg &= 0x3F; // write, single byte

    *ODR &= ~(1 << 3); // CS LOW

    // --- Send address ---
    while(!((*SR >> 1) & 1));  // Wait TXE
    *DR = reg;
    while(!((*SR >> 0) & 1));  // Wait RXNE
    (void)*DR;                 // Read dummy

    // --- Send data ---
    while(!((*SR >> 1) & 1));
    *DR = data;
    while(!((*SR >> 0) & 1));
    (void)*DR;                 // Read dummy

    while((*SR >> 7) & 1);     // Wait BSY = 0

    *ODR |= (1 << 3);          // CS HIGH
}

uint8_t I3G4250D_Read_Register(uint8_t reg)
{
    volatile uint32_t* SR = (uint32_t*)(SPI1_BASE_ADDR + 0x08);
    volatile uint32_t* DR = (uint32_t*)(SPI1_BASE_ADDR + 0x0C);
    volatile uint32_t* ODR = (uint32_t*)(GPIOE_BASE_ADDR + 0x14); // PE3 là CS

    reg |= 0x80; // bit 7 = 1 (read)

    // CS LOW
    *ODR &= ~(1 << 3);

    // Gửi địa chỉ cần đọc
    while(((*SR >> 1) & 1) == 0);
    *DR = reg;

    // Chờ nhận lại byte
    while(((*SR >> 1) & 1) == 0);
    *DR = 0xFF; // Dummy byte để clock ra dữ liệu

    // Chờ nhận xong
    while(((*SR >> 0) & 1) == 0);
    uint8_t data = *DR;

    // CS HIGH
    *ODR |= (1 << 3);

    return data;
}

void I3G4250D_Init()
{
    // 2. Chọn FS = ±500 dps
    I3G4250D_write_register(0x23, 0x10); // CTRL_REG4

    // 3. ODR = 420Hz, enable XYZ, Power On
    I3G4250D_write_register(0x20, 0xAF); // CTRL_REG1
}
void I3G4250D_ReadMulti(uint8_t reg_start, uint8_t* buf, uint8_t len)
{
    volatile uint32_t* SR  = (uint32_t*)(SPI1_BASE_ADDR + 0x08);
    volatile uint32_t* DR  = (uint32_t*)(SPI1_BASE_ADDR + 0x0C);
    volatile uint32_t* ODR = (uint32_t*)(GPIOE_BASE_ADDR + 0x14); // PE3 = CS

    reg_start |= 0xC0; // bit 7 = Read, bit 6 = auto-increment

    // Kéo CS xuống để bắt đầu giao tiếp
    *ODR &= ~(1 << 3);

    // Gửi byte địa chỉ
    while(((*SR >> 1) & 1) == 0); // TXE
    *DR = reg_start;
    while(((*SR >> 0) & 1) == 0); // RXNE
    (void)*DR;                    // Đọc bỏ byte phản hồi rác

    // Đọc len byte dữ liệu
    for (uint8_t i = 0; i < len; i++) {
        while(((*SR >> 1) & 1) == 0); // TXE
        *DR = 0xFF;                   // Dummy byte để tạo xung clock

        while(((*SR >> 0) & 1) == 0); // RXNE
        buf[i] = *DR;
    }

    // Chờ SPI hoàn tất truyền
    while((*SR >> 7) & 1); // BSY

    // Nhả CS lên lại
    *ODR |= (1 << 3);
}
void I3G4250D_ReadGyroXYZ(int16_t* x, int16_t* y, int16_t* z)
{
    uint8_t buf[6];
    I3G4250D_ReadMulti(0x28, buf, 6);  // Đọc từ OUT_X_L auto-increment

    *x = (int16_t)((buf[1] << 8) | buf[0]); // OUT_X_H << 8 | OUT_X_L
    *y = (int16_t)((buf[3] << 8) | buf[2]); // OUT_Y_H << 8 | OUT_Y_L
    *z = (int16_t)((buf[5] << 8) | buf[4]); // OUT_Z_H << 8 | OUT_Z_L
}
void Check_Gyro_Shock(void)
{
    I3G4250D_ReadGyroXYZ(&gyro_x, &gyro_y, &gyro_z);

    dps_x = gyro_x * 0.0175f;
    dps_y = gyro_y * 0.0175f;
    dps_z = gyro_z * 0.0175f;

    // So sánh trị tuyệt đối thủ công
    if ((dps_x > 25.0f || dps_x < -25.0f) ||
        (dps_y > 25.0f || dps_y < -25.0f) ||
        (dps_z > 25.0f || dps_z < -25.0f))
    {
        for (int i = 0; i < 4; i++) {
            for (uint8_t j = 0; j < 4; j++) Led_ctrl(j, 1); // Bật all
            HAL_Delay(100);
            for (uint8_t j = 0; j < 4; j++) Led_ctrl(j, 0); // Tắt all
            HAL_Delay(100);
        }
    }
}


int main(void)
{
    HAL_Init();
    SPI_master_init();
    LedInit();
    HAL_Delay(1000);
    I3G4250D_Init();

    while (1)
    {
        Check_Gyro_Shock();
        HAL_Delay(50); // kiểm tra liên tục mỗi 50ms
    }
}

