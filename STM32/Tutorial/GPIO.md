# Quy trình cấu hình bằng Thanh ghi (Ví dụ GPIO trên STM32F103)
Các thanh ghi cốt lõi:

**RCC_APB2ENR**: Thanh ghi cấp xung clock hệ thống cho các Port GPIO (GPIOA, GPIOB, GPIOC, v.v.).
**GPIOx_CRL** / **GPIOx_CRH** (Control Register Low/High): Thanh ghi cấu hình chế độ (Mode) và tốc độ (Speed) cho các chân GPIO. CRL quản lý các chân từ 0-7, CRH quản lý các chân từ 8-15.
**GPIOx_IDR** (Input Data Register): Thanh ghi chứa dữ liệu đầu vào. Đọc thanh ghi này để biết trạng thái mức logic của chân (0 hoặc 1).
**GPIOx_ODR** (Output Data Register): Thanh ghi chứa dữ liệu đầu ra. Ghi vào thanh ghi này để xuất mức logic (0 hoặc 1).
**GPIOx_BSRR** (Bit Set/Reset Register): Thanh ghi cho phép Set (lên 1) hoặc Reset (xuống 0) từng chân một cách độc lập mà không ảnh hưởng đến các chân khác (Atomic operation).

Dưới đây là mô tả các bước can thiệp thanh ghi để cấu hình chân PC13 làm ngõ ra (Điều khiển LED) và PA0 làm ngõ vào (Nút nhấn):

```c
void GPIO_Init(void) {
    // 1. Cấp xung Clock cho Port A và Port C
    RCC->APB2ENR |= (1 << 2); // Bật Clock Port A (Bit số 2)
    RCC->APB2ENR |= (1 << 4); // Bật Clock Port C (Bit số 4)

    // 2. Cấu hình chân PC13 làm Output Push-Pull, tốc độ 50MHz
    // PC13 thuộc nửa cao của Port C, nên ta cấu hình qua thanh ghi CRH
    GPIOC->CRH &= ~(0xF << 20); // Xóa cấu hình cũ của chân PC13 (Bit 20-23)
    GPIOC->CRH |=  (0x3 << 20); // Ghi 0011b (Mode = 11: Output 50MHz, CNF = 00: Push-Pull)

    // 3. Cấu hình chân PA0 làm Input Pull-up/Pull-down
    // PA0 thuộc nửa thấp của Port A, nên ta cấu hình qua thanh ghi CRL
    GPIOA->CRL &= ~(0xF << 0); // Xóa cấu hình cũ của chân PA0 (Bit 0-3)
    GPIOA->CRL |=  (0x8 << 0); // Ghi 1000b (Mode = 00: Input, CNF = 10: Pull-up/Pull-down)
    
    // Chọn chế độ Pull-up cho PA0 bằng cách set bit 0 trong thanh ghi ODR lên 1
    GPIOA->ODR |= (1 << 0); 
}

void LED_On(void) {
    // Kéo chân PC13 xuống mức 0 (Thường LED trên board kích mức thấp)
    // Dùng nửa trên của BSRR (Bit 16-31) để Reset (BR - Bit Reset)
    GPIOC->BSRR = (1 << (13 + 16)); // Hoặc GPIOC->BRR = (1 << 13);
}

void LED_Off(void) {
    // Kéo chân PC13 lên mức 1
    // Dùng nửa dưới của BSRR (Bit 0-15) để Set (BS - Bit Set)
    GPIOC->BSRR = (1 << 13); 
}

uint8_t Button_Read(void) {
    // Đọc trạng thái chân PA0 từ thanh ghi IDR
    if (GPIOA->IDR & (1 << 0)) {
        return 1; // Nút chưa được nhấn (Mức cao do Pull-up)
    } else {
        return 0; // Nút đang được nhấn (Bị kéo xuống mức thấp GND)
    }
}
```


___

# Quy trình cấu hình thanh ghi cho GPIO trên STM32G0B1 NUCLEO

Các thanh ghi cốt lõi trên STM32G0:

**RCC->IOPENR**: Thanh ghi bật xung clock cho các Port GPIO (Kiến trúc G0 tối ưu, đưa GPIO vào bus IOPORT thay vì APB).
**GPIOx->MODER** (Mode Register): Thiết lập hướng của chân (Input, Output, Alternate Function, Analog).
**GPIOx->OTYPER** (Output Type Register): Cấu hình kiểu ngõ ra (Push-Pull hoặc Open-Drain).
**GPIOx->OSPEEDR** (Output Speed Register): Cấu hình tốc độ chuyển trạng thái của ngõ ra (Low, Medium, High, Very High).
**GPIOx->PUPDR** (Pull-up/Pull-down Register): Bật điện trở kéo lên hoặc kéo xuống nội bộ.
**GPIOx->IDR** / **GPIOx->ODR**: Thanh ghi đọc (Input) và ghi (Output) dữ liệu.
**GPIOx->BSRR**: Thanh ghi Set/Reset bit nhanh (Atomic).

Ví dụ cấu hình chân PA5 làm ngõ ra (LED trên Nucleo) và PC13 làm ngõ vào (Nút nhấn User trên Nucleo):
```c
#include "stm32g0xx.h"

void GPIO_Init(void) {
    // 1. Cấp xung Clock cho Port A và Port C
    RCC->APB2ENR |= (1 << 2); // Bật Clock Port A (Bit số 2)
    RCC->APB2ENR |= (1 << 4); // Bật Clock Port C (Bit số 4)

    // 2. Cấu hình chân PA5 (Output Push-Pull)
    // Mỗi chân chiếm 2 bit trong thanh ghi MODER, giá trị 01b là Output mode
    GPIOA->MODER &= ~(3U << (5 * 2)); // Xóa cấu hình cũ PA5
    GPIOA->MODER |=  (1U << (5 * 2)); // Thiết lập Output mode (01b)
    
    // (Tùy chọn) OTYPER mặc định là 0 (Push-pull), OSPEEDR mặc định là Low speed
    // Có thể cấu hình lại nếu cần thiết, ở đây ta dùng mặc định.

    // 3. Cấu hình chân PC13 (Input, No Pull / Hoặc Pull-up tùy mạch cứng)
    // Nút nhấn trên Nucleo G0 đã có sẵn điện trở pull-up bên ngoài, ta để mode Input, No Pull.
    // MODER giá trị 00b là Input mode
    GPIOC->MODER &= ~(3U << (13 * 2)); // Thiết lập Input mode (00b) cho PC13
    
    // (Tùy chọn) Nếu mạch chưa có trở kéo, ta bật Pull-up nội bằng thanh ghi PUPDR (Giá trị 01b)
    // GPIOC->PUPDR &= ~(3U << (13 * 2));
    // GPIOC->PUPDR |=  (1U << (13 * 2));
}

void LED_Toggle(void) {
    // Đảo trạng thái LED PA5 bằng cách dùng phép XOR (^) lên thanh ghi ODR
    GPIOA->ODR ^= (1 << 5);
}

uint8_t Button_Read(void) {
    // Đọc trạng thái chân PC13 (Bit thứ 13 của IDR)
    if (GPIOC->IDR & (1 << 13)) {
        return 1; // Nút chưa nhấn (Mức cao)
    } else {
        return 0; // Nút đã nhấn (Mức thấp)
    }
}
```

# STM32G0B1 (Sử dụng thư viện STM32Cube HAL)
*Thư viện HAL quản lý cấu hình GPIO thông qua Struct GPIO_InitTypeDef và các hàm API tiêu chuẩn, giúp lập trình viên không cần nhớ địa chỉ và các bit của thanh ghi.*

Lưu ý: HAL cho GPIO rất đơn giản, không cần Handle (như UART) mà chỉ truyền trực tiếp địa chỉ Port (VD: GPIOA, GPIOC) và các thông số cấu hình vào hàm `HAL_GPIO_Init()`.

```c
#include "stm32g0xx_hal.h"

// Hàm khởi tạo cấu hình GPIO
void MX_GPIO_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // 1. Bật Clock cho các Port GPIO
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    // 2. Cấu hình chân LED (PA5)
    // Xóa trạng thái mặc định, đặt mức thấp lúc khởi động
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin = GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;   // Ngõ ra Push-Pull
    GPIO_InitStruct.Pull = GPIO_NOPULL;           // Không cần điện trở kéo
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;  // Tốc độ thấp (đủ để nháy LED)
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);       // Áp dụng cấu hình

    // 3. Cấu hình chân Nút nhấn (PC13)
    GPIO_InitStruct.Pin = GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;       // Ngõ vào
    GPIO_InitStruct.Pull = GPIO_NOPULL;           // Mạch Nucleo đã có sẵn Pull-up phần cứng
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);       // Áp dụng cấu hình
}

int main(void) {
    // Khởi tạo hệ thống HAL
    HAL_Init();
    
    // Khởi tạo ngoại vi GPIO
    MX_GPIO_Init();

    while (1) {
        // Đọc trạng thái nút nhấn trên PC13
        // Trả về GPIO_PIN_RESET (0) hoặc GPIO_PIN_SET (1)
        if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET) {
            // Nếu nút được nhấn, bật LED PA5
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
            HAL_Delay(100); // Chống dội phím (Debounce) đơn giản
        } else {
            // Nếu không nhấn, tắt LED PA5
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
        }

        // Tùy chọn: Hàm đảo trạng thái LED
        // HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
    }
}
```