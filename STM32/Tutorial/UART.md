# Quy trình cấu hình bằng Thanh ghi (Ví dụ USART1 trên STM32F103)

Các thanh ghi cốt lõi:
- **RCC_APB2ENR** hoặc **RCC_APB1ENR**: Thanh ghi cấp xung clock hệ thống cho bộ UART và các chân GPIO liên quan.
- **USART_BRR** (Baud Rate Register): Thiết lập tốc độ Baud sử dụng giá trị chia dạng số thực (Mantissa và Fraction).
- **USART_CR1** (Control Register 1): Bật UART (UE), định nghĩa độ dài từ (M), bật bộ truyền/nhận (TE/RE), và bật ngắt.
- **USART_CR2** & **USART_CR3**: Cấu hình bit Stop, các tính năng nâng cao như DMA, phần cứng kiểm soát luồng (Hardware flow control).
- **USART_SR** (Status Register - trên F1) hoặc **USART_ISR** (trên dòng mới): Chứa các cờ trạng thái như TXE (Bộ đệm truyền trống), TC (Truyền hoàn tất), RXNE (Bộ nhận có dữ liệu).
- **USART_DR** (Data Register - trên F1) hoặc USART_RDR/USART_TDR (trên dòng mới): Nơi ghi/đọc dữ liệu.


Dưới đây là mô tả các bước can thiệp thanh ghi để cấu hình USART1 (TX=PA9, RX=PA10), Baudrate 115200, 8N1:
```c
void USART1_Init(void) {
    // 1. Cấp xung Clock cho GPIOA và USART1
    RCC->APB2ENR |= (1 << 2);  // Bật Clock Port A (Bit số 2)
    RCC->APB2ENR |= (1 << 14); // Bật Clock USART1 (Bit số 14)

    // 2. Cấu hình chân GPIO (PA9 là Alternate Function Push-Pull, PA10 là Input Floating)
    // Đối với STM32F1, ta cấu hình qua thanh ghi CRH
    GPIOA->CRH &= ~(0xFF << 4); // Xóa cấu hình chân PA9 và PA10
    GPIOA->CRH |= (0x0B << 4);  // PA9 (TX): Alternate function output Push-Pull, 50MHz (1011b)
    GPIOA->CRH |= (0x04 << 8);  // PA10 (RX): Input floating (0100b)

    // 3. Cấu hình Tốc độ Baud (Ví dụ: PCLK2 = 72MHz, Baud = 115200)
    // Công thức: USARTDIV = 72000000 / (16 * 115200) = 39.0625
    // Phần nguyên (Mantissa) = 39 = 0x27
    // Phần thập phân (Fraction) = 0.0625 * 16 = 1 = 0x1
    // Ghi vào thanh ghi BRR giá trị: 0x271
    USART1->BRR = 0x271;

    // 4. Cấu hình các tham số truyền nhận trong CR1
    // M = 0 (8 data bits), PCE = 0 (No parity)
    USART1->CR1 |= (1 << 3); // Bật bộ truyền (TE - Transmit Enable, nằm ở Bit 3)
    USART1->CR1 |= (1 << 2); // Bật bộ nhận (RE - Receive Enable, nằm ở Bit 2)
    
    // 5. Cấu hình Stop bit trong CR2
    // Trường STOP chiếm 2 bit là bit 12 và bit 13. Xóa cả 2 bit về 0 tương ứng với 1 Stop bit.
    USART1->CR2 &= ~(3 << 12); // (3 hệ thập phân = 11 hệ nhị phân), dịch đến vị trí 12 rồi xóa

    // 6. Kích hoạt ngoại vi USART1
    USART1->CR1 |= (1 << 13); // Bật UART (UE - USART Enable, nằm ở Bit 13)
}

void USART1_Transmit(char c) {
    // Chờ cờ TXE (Transmit data register empty - Nằm ở Bit 7) lên 1
    // Khi bit 7 bằng 0 nghĩa là bộ đệm chưa trống, vòng lặp while sẽ chờ.
    while (!(USART1->SR & (1 << 7)));
    
    // Ghi dữ liệu vào thanh ghi DR
    USART1->DR = c;
}

char USART1_Receive(void) {
    // Chờ cờ RXNE (Read data register not empty - Nằm ở Bit 5) lên 1
    // Khi bit 5 bằng 0 nghĩa là chưa có dữ liệu mới đến, vòng lặp while sẽ chờ.
    while (!(USART1->SR & (1 << 5)));
    
    // Đọc dữ liệu từ thanh ghi DR
    return (char)(USART1->DR & 0xFF);
}
```
Tóm tắt các vị trí bit để bạn tiện làm tài liệu:
APB2ENR Bit 2: Bật I/O Port A.
APB2ENR Bit 14: Bật USART1.
CR1 Bit 3: TE (Transmit Enable).
CR1 Bit 2: RE (Receive Enable).
CR1 Bit 13: UE (USART Enable).
CR2 Bit 12, 13: STOP bits (00 = 1 stop bit).
SR Bit 7: TXE (Transmit Data Register Empty).
SR Bit 5: RXNE (Read Data Register Not Empty).
___

# Quy trình cấu hình thanh ghi cho USART2 trên STM32G0B1 NUCLEO

Các thanh ghi cốt lõi trên STM32G0:
- **RCC->IOPENR**: Thanh ghi bật xung clock cho các Port GPIO (Dòng G0 dùng IOPENR thay vì AHBENR/APB2ENR cho GPIO).
- **RCC->APBENR1**: Thanh ghi bật xung clock cho ngoại vi USART2.
- **GPIOA->MODER** / **GPIOA->AFR**: Thiết lập chân PA2, PA3 sang chế độ Chức năng mở rộng (Alternate Function - AF1 cho USART2).
- **USART2->BRR**: Thanh ghi cấu hình Baud rate.
- **USART2->CR1**: Bật UART (UE), bật bộ truyền/nhận (TE/RE), cấu hình độ dài dữ liệu.
- **USART2->ISR**: Thanh ghi chứa các cờ trạng thái (TXE, TC, RXNE).
- **USART2->TDR** / **USART2->RDR**: Thanh ghi truyền và nhận dữ liệu độc lập.


System clock = 16MHz, Baudrate 115200, Khung dữ liệu 8N1 (8 data bits, No parity, 1 stop bit).
(Mặc định chân TX là PA2, RX là PA3).
```c
#include "stm32g0xx.h"

void USART2_Init(void) {
    // 1. Cấp xung Clock cho GPIOA và USART2
    RCC->IOPENR  |= (1 << 0);  // Bật clock cho Port A (Bit số 0)
    RCC->APBENR1 |= (1 << 17); // Bật clock cho ngoại vi USART2 (Bit số 17)

    // 2. Cấu hình chân PA2 (TX) và PA3 (RX) thành Alternate Function
    // Mỗi chân chiếm 2 bit trong thanh ghi MODER. 
    // PA2 bắt đầu từ bit số 4 (2*2). PA3 bắt đầu từ bit số 6 (3*2).
    GPIOA->MODER &= ~((3 << 4) | (3 << 6)); // Xóa cấu hình cũ (11b tương đương 3) của PA2, PA3
    GPIOA->MODER |=  ((2 << 4) | (2 << 6)); // Ghi giá trị 10b (tương đương 2) cho PA2, PA3

    // 3. Ánh xạ chân PA2, PA3 sang chức năng USART2 (Alternate Function 1 - AF1)
    // Mỗi chân chiếm 4 bit trong thanh ghi AFR. 
    // PA2 bắt đầu từ bit số 8 (2*4). PA3 bắt đầu từ bit số 12 (3*4).
    // 15 hệ thập phân tương đương 0xF hay 1111b trong nhị phân.
    GPIOA->AFR[0] &= ~((15 << 8) | (15 << 12)); // Xóa cấu hình AF cũ 
    GPIOA->AFR[0] |=  ((1 << 8)  | (1 << 12));  // Ghi giá trị 1 (0001b - AF1) cho PA2 và PA3

    // 4. Cấu hình Tốc độ Baud (Baudrate = 115200)
    // Công thức: USART2->BRR = 16,000,000 / 115200 = 138.888 -> Làm tròn thành 139
    USART2->BRR = 139;

    // 5. Cấu hình khung truyền dữ liệu trong CR1
    USART2->CR1 |= (1 << 3); // Enable Bộ truyền (TE - Transmit Enable, Bit số 3)
    USART2->CR1 |= (1 << 2); // Enable Bộ nhận (RE - Receive Enable, Bit số 2)

    // 6. Cấu hình Stop bit trong CR2
    // Trường STOP nằm ở bit 12 và 13. Giá trị 00b = 1 Stop bit.
    USART2->CR2 &= ~(3 << 12); // Dịch tới vị trí 12, xóa cả 2 bit bằng mask 3 (11b)

    // 7. Kích hoạt ngoại vi USART2
    // Chú ý: Trên STM32G0, bit UE nằm ở vị trí số 0 (Khác với F1 nằm ở bit 13)
    USART2->CR1 |= (1 << 0); // USART Enable (UE - Bit số 0)
}

void USART2_Transmit(char c) {
    // Chờ cho đến khi thanh ghi đệm truyền trống
    // Trên STM32G0, cờ TXE nằm ở Bit số 7 của thanh ghi ISR
    while (!(USART2->ISR & (1 << 7)));
    
    // Ghi dữ liệu vào thanh ghi TDR (Transmit Data Register)
    USART2->TDR = c;
}

char USART2_Receive(void) {
    // Chờ cho đến khi nhận đủ dữ liệu
    // Cờ RXNE nằm ở Bit số 5 của thanh ghi ISR
    while (!(USART2->ISR & (1 << 5)));
    
    // Đọc dữ liệu ra từ thanh ghi RDR (Receive Data Register)
    return (char)(USART2->RDR & 0xFF);
}

void USART2_SendString(char* str) {
    while (*str) {
        USART2_Transmit(*str++);
    }
}
```
Tóm tắt các vị trí bit cho STM32G0 (USART2):

IOPENR Bit 0: Bật I/O Port A.
APBENR1 Bit 17: Bật USART2.
CR1 Bit 3: TE (Transmit Enable).
CR1 Bit 2: RE (Receive Enable).
CR1 Bit 0: UE (USART Enable) - Đây là điểm khác biệt lớn so với F1.
CR2 Bit 12, 13: STOP bits (00 = 1 stop bit).
ISR Bit 7: TXE (Transmit Data Register Empty).
ISR Bit 5: RXNE (Read Data Register Not Empty).
___

# STM32G0B1 (Sử dụng thư viện STM32Cube HAL)
*Thư viện HAL (Hardware Abstraction Layer) của STMicroelectronics quản lý cấu hình thông qua một Struct (Cấu trúc dữ liệu) cấu hình và một Handle (Con trỏ quản lý) ngoại vi.*

**Lưu ý**: *Để đúng chuẩn HAL, việc cấp xung clock cho UART và cấu hình chân GPIO (PA2, PA3) sẽ được tách riêng ra một hàm callback hệ thống tên là **HAL_UART_MspInit**.*

```c
#include "stm32g0xx_hal.h"

// Định nghĩa Handle cho UART2
UART_HandleTypeDef huart2;

// 1. Hàm khởi tạo cấu hình UART2
void MX_USART2_UART_Init(void) {
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

    // Gọi hàm khởi tạo của HAL, hàm này sẽ tự động tính toán thanh ghi BRR và CR
    if (HAL_UART_Init(&huart2) != HAL_OK) {
        // Xử lý lỗi nếu cấu hình thất bại
        while(1);
    }
}

// 2. Hàm cấu hình phần cứng cấp thấp (Được tự động gọi bởi HAL_UART_Init)
void HAL_UART_MspInit(UART_HandleTypeDef* uartHandle) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    if(uartHandle->Instance == USART2) {
        // Bật Clock cho ngoại vi
        __HAL_RCC_USART2_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        /** Cấu hình chân GPIO cho USART2
        PA2     ------> USART2_TX
        PA3     ------> USART2_RX
        */
        GPIO_InitStruct.Pin = GPIO_PIN_PA2|GPIO_PIN_PA3;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = GPIO_AF1_USART2; // AF1 cho dòng G0
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    }
}

int main(void) {
    // Khởi tạo hệ thống HAL
    HAL_Init();
    
    // Khởi tạo UART2
    MX_USART2_UART_Init();

    char tx_data[] = "UART STM32 HAL Ready!\r\n";
    // Truyền dữ liệu (Blocking mode) - Chờ tối đa 100ms
    HAL_UART_Transmit(&huart2, (uint8_t*)tx_data, sizeof(tx_data)-1, 100);

    char rx_data;
    while (1) {
        // Nhận dữ liệu (Blocking mode) - Chờ vô thời hạn (HAL_MAX_DELAY) cho đến khi có 1 byte đến
        if (HAL_UART_Receive(&huart2, (uint8_t*)&rx_data, 1, HAL_MAX_DELAY) == HAL_OK) {
            // Echo dữ liệu lại cho người gửi
            HAL_UART_Transmit(&huart2, (uint8_t*)&rx_data, 1, 100);
        }
    }
}
```