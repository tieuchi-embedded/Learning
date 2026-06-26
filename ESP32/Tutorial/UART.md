# Cấu hình thanh ghi UART cho ESP32 

ESP32 có 3 bộ UART phần cứng (UART0, UART1, UART2). Giao tiếp ngoại vi của ESP32 cực kỳ linh hoạt nhờ bộ định tuyến GPIO Matrix, cho phép ánh xạ (map) các chân TX, RX của UART vào bất kỳ chân GPIO trống nào của chip.

*Do ESP32 chạy trên hệ điều hành FreeRTOS và kiến trúc phần cứng đóng gói rất sâu, việc cấu hình "thuần thanh ghi" (Bare-metal) không qua Espressif IDF Framework (ESP-IDF) là **cực kỳ hiếm và phức tạp** do cơ chế quản lý năng lượng (Power Management) và Clock Gating. Tuy nhiên, cấu trúc thanh ghi cốt lõi của nó hoạt động như sau:*

Các thanh ghi cốt lõi:
- **UART_CLK_DIV_REG**: Thanh ghi chia cấu hình tốc độ Baud (gồm cả phần nguyên CLK_DIV và phần phân số CLK_DIV_FRAG).
- **UART_CONF0_REG**: Cấu hình chính (Độ dài bit dữ liệu BIT_NUM, cấu hình Stop bit STOP_BIT_NUM, bật/tắt Parity PARITY_EN).
- **UART_FIFO_REG**: ESP32 có bộ đệm phần cứng FIFO (128 bytes cho cả TX và RX). Đọc ghi dữ liệu đều thông qua thanh ghi này.
- **UART_INT_ENA_REG** / **UART_INT_CLR_REG**: Bật và xóa các ngắt của bộ FIFO (Ví dụ ngắt khi FIFO đầy dữ liệu nhận).

```c
#include "soc/uart_struct.h"
#include "soc/uart_reg.h"

// Biến con trỏ trỏ trực tiếp tới vùng nhớ thanh ghi của UART2
volatile uart_dev_t *uart_reg = &UART2;

void esp32_uart2_init_registers() {
    // Lưu ý: Thông thường phải cấp nguồn và cấu hình GPIO Matrix trước bằng API của IDF
    
    // 1. Cấu hình Baudrate (Giả sử nguồn clock APB = 80MHz, Baud = 115200)
    // Hệ số chia = 80000000 / 115200 = 694.444
    // Phần nguyên = 694, Phần thập phân = 0.444 * 16 = 7
    uart_reg->clk_div.div_int = 694;
    uart_reg->clk_div.div_frag = 7;

    // 2. Cấu hình khung dữ liệu: 8N1
    uart_reg->conf0.bit_num = 3;       // 3 tương ứng với 8 bits dữ liệu
    uart_reg->conf0.parity_en = 0;     // Vô hiệu hóa Parity
    uart_reg->conf0.stop_bit_num = 1;  // 1 tương ứng với 1 stop bit

    // 3. Reset bộ đệm phần cứng FIFO
    uart_reg->conf0.txfifo_rst = 1;
    uart_reg->conf0.txfifo_rst = 0;
    uart_reg->conf0.rxfifo_rst = 1;
    uart_reg->conf0.rxfifo_rst = 0;
}

void esp32_uart2_write(uint8_t data) {
    // Kiểm tra số lượng ô trống trong bộ đệm TX FIFO
    // Nếu txfifo_cnt đạt tới 128 tức là đầy, phải chờ
    while (((uart_reg->status.txfifo_cnt)) >= 128);
    
    // Ghi dữ liệu vào thanh ghi FIFO
    uart_reg->fifo.rw_byte = data;
}

uint8_t esp32_uart2_read() {
    // Chờ cho đến khi bộ đệm RX FIFO có dữ liệu (rxfifo_cnt > 0)
    while ((uart_reg->status.rxfifo_cnt) == 0);
    
    // Đọc ra 1 byte từ thanh ghi FIFO
    return uart_reg->fifo.rw_byte;
}
```

# ESP32 (Sử dụng Thư viện ESP-IDF)

*Framework chính thức ESP-IDF của Espressif quản lý UART theo kiến trúc driver của hệ điều hành thời gian thực (FreeRTOS). Driver của ESP32 sử dụng một hàng đợi ngắt (Interrupt Queue) và bộ đệm Ring Buffer bằng phần mềm để quản lý bộ đệm FIFO 128-byte của phần cứng.*

```c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"

// Định nghĩa cổng UART và kích thước bộ đệm
#define UART_NUM        UART_NUM_2
#define TXD_PIN         GPIO_NUM_17
#define RXD_PIN         GPIO_NUM_16
#define RX_BUF_SIZE     1024

void init_uart2(void) {
    // 1. Cấu hình các tham số cho UART
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT, // Sử dụng nguồn clock mặc định của hệ thống
    };
    
    // 2. Áp dụng cấu hình cấu trúc vào cổng UART_NUM_2
    ESP_ERROR_CHECK(uart_param_config(UART_NUM, &uart_config));

    // 3. Định tuyến chân TX, RX thông qua GPIO Matrix
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    // 4. Cài đặt Driver UART, cấp phát bộ đệm RX bằng phần mềm (Ring buffer)
    // Cổng này không dùng bộ đệm TX bằng phần mềm (set = 0), không dùng Event Queue (set = NULL)
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM, RX_BUF_SIZE * 2, 0, 0, NULL, 0));
}

void app_main(void) {
    // Khởi tạo UART2
    init_uart2();

    char *init_msg = "UART ESP-IDF Ready!\r\n";
    // Gửi dữ liệu đi
    uart_write_bytes(UART_NUM, init_msg, strlen(init_msg));

    uint8_t *data = (uint8_t *) malloc(RX_BUF_SIZE);

    while (1) {
        // Đọc dữ liệu từ bộ đệm UART RX (Hàm này sẽ block/chờ tối đa 1000ms / portMAX_DELAY nếu chưa có dữ liệu)
        int len = uart_read_bytes(UART_NUM, data, RX_BUF_SIZE, 1000 / portTICK_PERIOD_MS);
        
        if (len > 0) {
            // Gửi trả ngược lại toàn bộ chuỗi dữ liệu vừa nhận được
            uart_write_bytes(UART_NUM, (const char *) data, len);
        }
    }
}
```