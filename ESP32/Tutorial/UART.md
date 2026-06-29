# Cấu hình thanh ghi UART cho ESP32 

ESP32 có 3 bộ UART phần cứng (UART0, UART1, UART2). Giao tiếp ngoại vi của ESP32 cực kỳ linh hoạt nhờ bộ định tuyến GPIO Matrix, cho phép ánh xạ (map) các chân TX, RX của UART vào bất kỳ chân GPIO trống nào của chip.

*Do ESP32 chạy trên hệ điều hành FreeRTOS và kiến trúc phần cứng đóng gói rất sâu, việc cấu hình "thuần thanh ghi" (Bare-metal) không qua Espressif IDF Framework (ESP-IDF) là **cực kỳ hiếm và phức tạp** do cơ chế quản lý năng lượng (Power Management) và Clock Gating. Tuy nhiên, cấu trúc thanh ghi cốt lõi của nó hoạt động như sau:*

Các thanh ghi cốt lõi:
- **UART_CLK_DIV_REG**: Thanh ghi chia cấu hình tốc độ Baud (gồm cả phần nguyên CLK_DIV và phần phân số CLK_DIV_FRAG).
- **UART_CONF0_REG**: Cấu hình chính (Độ dài bit dữ liệu BIT_NUM, cấu hình Stop bit STOP_BIT_NUM, bật/tắt Parity PARITY_EN).
- **UART_FIFO_REG**: ESP32 có bộ đệm phần cứng FIFO (128 bytes cho cả TX và RX). Đọc ghi dữ liệu đều thông qua thanh ghi này.
- **UART_INT_ENA_REG** / **UART_INT_CLR_REG**: Bật và xóa các ngắt của bộ FIFO (Ví dụ ngắt khi FIFO đầy dữ liệu nhận).

```c
/*
 * ============================================================
 *  VÍ DỤ: UART0 cấu hình trực tiếp qua THANH GHI (bare-metal)
 *  - Không dùng driver/uart.h, tự ghi/đọc vào vùng nhớ thanh ghi
 *    phần cứng của UART0 để hiểu rõ cơ chế hoạt động bên dưới.
 *  - Vẫn chạy trong môi trường ESP-IDF/FreeRTOS (app_main, vTaskDelay)
 *    vì ESP32 luôn cần bootloader + FreeRTOS để khởi động.
 *  - Dùng chân mặc định GPIO1 (TX0) / GPIO3 (RX0) - cổng nạp code.
 * ============================================================
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"            // Cho vTaskDelay, pdMS_TO_TICKS
#include "soc/uart_struct.h"          // Định nghĩa struct uart_dev_t mô tả layout các thanh ghi UART
#include "soc/uart_reg.h"             // Định nghĩa địa chỉ/bit các thanh ghi UART (dùng nếu cần macro thô)
#include "esp_private/periph_ctrl.h"  // Cho periph_module_enable() - hàm bật/tắt clock cho peripheral
#include "soc/periph_defs.h"          // Định nghĩa PERIPH_UART0_MODULE và các module khác

/*
 * "UART0" ở đây là 1 biến toàn cục kiểu uart_dev_t do SDK định nghĩa sẵn,
 * trỏ thẳng tới vùng địa chỉ thanh ghi vật lý của khối UART0 trong chip.
 * Ta tạo con trỏ uart_reg để code gọn hơn khi truy cập các field bên trong.
 * "volatile" báo cho compiler biết: giá trị có thể tự thay đổi bởi phần cứng,
 * không được tối ưu hóa (cache) giá trị đọc/ghi.
 */
volatile uart_dev_t *uart_reg = &UART0;

/*
 * Hàm khởi tạo UART0 ở cấp thanh ghi.
 * Phải làm đủ các bước sau, thiếu bước nào cũng sẽ không hoạt động:
 *   1) Bật clock cho khối UART0 (nếu không, mọi đọc/ghi vào thanh ghi
 *      coi như "nói chuyện với khối mạch đang ngủ" - vô nghĩa hoặc treo).
 *   2) Cấu hình tốc độ baud (chia clock APB ra đúng tốc độ mong muốn).
 *   3) Cấu hình khung truyền (số bit dữ liệu, parity, stop bit).
 *   4) Reset FIFO phần cứng để đảm bảo không còn dữ liệu rác từ trước.
 */
void esp32_uart0_init_registers(void) {
    // Bước 1: Mở "vòi" clock cấp cho khối UART0.
    // (Trên thực tế UART0 thường đã được bootloader mở sẵn để làm console
    //  log mặc định, nhưng gọi lại ở đây vẫn an toàn và không gây lỗi.)
    periph_module_enable(PERIPH_UART0_MODULE);

    // Bước 2: Cấu hình thanh ghi chia clock để ra đúng baudrate 115200.
    // Công thức: baudrate = clock_APB / (div_int + div_frag/16)
    // Với clock_APB = 80,000,000 Hz và baudrate = 115200:
    //   80000000 / 115200 = 694.444...
    //   -> phần nguyên (div_int)  = 694
    //   -> phần thập phân 0.444 * 16 ≈ 7  -> div_frag = 7
    uart_reg->clk_div.div_int = 694;
    uart_reg->clk_div.div_frag = 7;

    // Bước 3: Cấu hình khung truyền kiểu "8N1" (8 bit dữ liệu, No parity, 1 stop bit)
    // - bit_num: 3 tương ứng 8 bit dữ liệu (0=5bit,1=6bit,2=7bit,3=8bit)
    uart_reg->conf0.bit_num = 3;
    // - parity_en = 0: tắt hẳn việc kiểm tra parity
    uart_reg->conf0.parity_en = 0;
    // - stop_bit_num = 1: dùng 1 bit stop (giá trị 2=1.5 bit, 3=2 bit)
    uart_reg->conf0.stop_bit_num = 1;

    // Bước 4: Reset FIFO phần cứng (128 byte cho TX, 128 byte cho RX)
    // Cách reset: set bit lên 1 rồi set lại về 0 ngay (giống "nhấn rồi thả nút reset")
    uart_reg->conf0.txfifo_rst = 1;
    uart_reg->conf0.txfifo_rst = 0;
    uart_reg->conf0.rxfifo_rst = 1;
    uart_reg->conf0.rxfifo_rst = 0;
}

/*
 * Gửi 1 byte ra UART0.
 * FIFO phần cứng TX chỉ chứa tối đa 128 byte, nên trước khi ghi thêm
 * phải kiểm tra FIFO còn chỗ trống hay không.
 */
void esp32_uart0_write(uint8_t data) {
    // status.txfifo_cnt: số byte hiện đang nằm trong FIFO chờ gửi đi.
    // Nếu FIFO đã đầy (>= 128) thì phải chờ phần cứng đẩy bớt dữ liệu ra ngoài.
    while ((uart_reg->status.txfifo_cnt) >= 128);

    // fifo.rw_byte: ghi vào thanh ghi này tức là "nhồi" 1 byte vào FIFO TX,
    // phần cứng sẽ tự động tuần tự hóa (serialize) byte đó ra chân TX theo
    // đúng baudrate/khung truyền đã cấu hình ở trên.
    uart_reg->fifo.rw_byte = data;
}

/*
 * Kiểm tra xem RX FIFO hiện có bao nhiêu byte đang chờ được đọc.
 * Dùng để đọc theo kiểu "non-blocking" - không bị treo chương trình
 * nếu chưa có dữ liệu tới.
 */
int esp32_uart0_available(void) {
    // status.rxfifo_cnt: số byte UART0 đã nhận được và đang nằm trong FIFO RX
    return uart_reg->status.rxfifo_cnt;
}

/*
 * Đọc 1 byte ra khỏi RX FIFO.
 * CHỈ nên gọi hàm này khi đã chắc chắn esp32_uart0_available() > 0,
 * nếu không sẽ đọc ra giá trị rác (FIFO trống).
 */
uint8_t esp32_uart0_read(void) {
    // Đọc thanh ghi fifo.rw_byte sẽ lấy ra byte cũ nhất đang nằm trong FIFO RX
    // (cơ chế FIFO: First In First Out)
    return uart_reg->fifo.rw_byte;
}

/*
 * app_main: hàm khởi đầu của firmware, được FreeRTOS gọi như 1 task.
 */
void app_main(void) {
    // Khởi tạo UART0 ở cấp thanh ghi trước khi dùng
    esp32_uart0_init_registers();

    int count = 0;       // Biến đếm số lần đã gửi, chỉ để minh họa nội dung
    char msg[64];         // Buffer tạm chứa chuỗi cần gửi mỗi vòng lặp

    while (1) {
        // ---- PHẦN GỬI ĐỊNH KỲ MỖI GIÂY ----
        // Ghép chuỗi "Hello #n\r\n" vào msg, trả về độ dài chuỗi (không tính ký tự null)
        int len = snprintf(msg, sizeof(msg), "Hello #%d\r\n", count++);

        // Gửi từng byte một ra UART0 bằng hàm tự viết ở trên
        for (int i = 0; i < len; i++) {
            esp32_uart0_write((uint8_t)msg[i]);
        }

        // ---- PHẦN ECHO DỮ LIỆU NHẬN ĐƯỢC (không chặn vòng lặp) ----
        // Lặp lấy hết các byte hiện có trong RX FIFO (nếu có) và gửi ngược lại.
        // Dùng available() để tránh việc gọi esp32_uart0_read() khi FIFO trống
        // (vì hàm read() ở bản này không tự chờ - phải tự kiểm tra trước).
        while (esp32_uart0_available() > 0) {
            esp32_uart0_write(esp32_uart0_read());
        }

        // ---- TẠO NHỊP 1 GIÂY/LẦN ----
        // Vì các hàm trên đều không tự có cơ chế delay/timeout như uart_read_bytes
        // của driver chính thức, ta phải tự gọi vTaskDelay để "nhường" CPU cho các
        // task khác trong 1000ms, đồng thời tạo đúng nhịp gửi mỗi giây.
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

# ESP32 (Sử dụng Thư viện ESP-IDF)

*Framework chính thức ESP-IDF của Espressif quản lý UART theo kiến trúc driver của hệ điều hành thời gian thực (FreeRTOS). Driver của ESP32 sử dụng một hàng đợi ngắt (Interrupt Queue) và bộ đệm Ring Buffer bằng phần mềm để quản lý bộ đệm FIFO 128-byte của phần cứng.*

```c
/*
 * ============================================================
 *  VÍ DỤ: UART0 dùng thư viện driver/uart.h của ESP-IDF
 *  - Dùng chân mặc định GPIO1 (TX0) / GPIO3 (RX0) - chính là
 *    cổng nạp code (USB-Serial) trên board NodeMCU-32S.
 *  - Mỗi giây gửi 1 chuỗi "Hello #n", đồng thời echo lại nếu
 *    có dữ liệu gửi tới.
 * ============================================================
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"   // Các định nghĩa cơ bản của FreeRTOS (kiểu dữ liệu, macro...)
#include "freertos/task.h"       // Cho vTaskDelay, portTICK_PERIOD_MS...
#include "driver/uart.h"         // API UART cấp cao do ESP-IDF cung cấp (uart_param_config, uart_write_bytes...)

// Chọn cổng UART muốn dùng. ESP32 có UART_NUM_0, UART_NUM_1, UART_NUM_2.
// UART_NUM_0 mặc định nối với chip USB-Serial trên board -> xem được qua Serial Monitor.
#define UART_NUM        UART_NUM_0

// Kích thước bộ đệm (buffer) dùng để chứa dữ liệu đọc về từ RX FIFO của UART.
#define RX_BUF_SIZE     1024

/*
 * Hàm khởi tạo UART0:
 * - Cấu hình baudrate, số bit dữ liệu, parity, stop bit...
 * - Cài driver UART (driver sẽ tự tạo ring buffer + xử lý ngắt nền,
 *   nhờ vậy ta chỉ cần gọi uart_read_bytes/uart_write_bytes mà không
 *   phải tự quản lý FIFO phần cứng).
 */
void init_uart0(void) {
    // Struct chứa toàn bộ thông số cấu hình UART
    uart_config_t uart_config = {
        .baud_rate = 115200,                  // Tốc độ truyền: 115200 bit/giây (chuẩn phổ biến)
        .data_bits = UART_DATA_8_BITS,        // Mỗi khung truyền 8 bit dữ liệu
        .parity    = UART_PARITY_DISABLE,     // Không dùng bit kiểm tra chẵn/lẻ (parity)
        .stop_bits = UART_STOP_BITS_1,        // 1 bit stop để báo kết thúc khung truyền
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,// Không dùng bắt tay phần cứng (RTS/CTS)
        .source_clk = UART_SCLK_DEFAULT,      // Dùng nguồn xung clock mặc định của hệ thống (APB)
    };

    // Áp cấu hình struct trên vào UART_NUM (UART0)
    // ESP_ERROR_CHECK sẽ in lỗi và dừng chương trình nếu hàm trả về lỗi (esp_err_t != ESP_OK)
    ESP_ERROR_CHECK(uart_param_config(UART_NUM, &uart_config));

    // KHÔNG gọi uart_set_pin() ở đây vì GPIO1/GPIO3 đã được nối cứng tới
    // UART0 thông qua IO_MUX (không qua GPIO Matrix) -> dùng mặc định luôn.

    // Cài driver UART:
    //   - Tham số 2: kích thước RX ring buffer (byte) -> driver tự copy dữ liệu
    //     từ FIFO phần cứng (128 byte) ra buffer phần mềm lớn hơn này.
    //   - Tham số 3: kích thước TX buffer, để 0 nghĩa là gửi theo kiểu chặn (blocking),
    //     không cần buffer riêng cho gửi.
    //   - Tham số 4,5: queue_size và con trỏ queue dùng cho UART event (không dùng ở đây -> 0, NULL)
    //   - Tham số 6: cờ cấp phát ngắt (interrupt alloc flags), để 0 dùng mặc định.
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM, RX_BUF_SIZE * 2, 0, 0, NULL, 0));
}

/*
 * Hàm chính của chương trình (entry point khi ESP-IDF chạy app).
 * Đây thực chất là 1 FreeRTOS task được framework tự tạo và gọi.
 */
void app_main(void) {
    // Bước 1: khởi tạo UART0 trước khi dùng
    init_uart0();

    // Buffer tạm để chứa dữ liệu đọc về từ UART (cấp phát tĩnh trên stack,
    // tránh phải malloc/free không cần thiết)
    uint8_t data[RX_BUF_SIZE];

    // Biến đếm để biết đã gửi bao nhiêu lần (chỉ để minh họa nội dung gửi đi)
    int count = 0;

    // Buffer tạm chứa chuỗi sẽ gửi đi mỗi vòng lặp
    char msg[64];

    // Vòng lặp vô tận - chạy mãi cho tới khi mất điện/reset (đúng tinh thần firmware nhúng)
    while (1) {
        // ---- PHẦN GỬI ĐỊNH KỲ ----
        // snprintf: ghép chuỗi an toàn (không tràn buffer) vào msg,
        // trả về số ký tự đã ghi (không tính ký tự null-terminator)
        int len_msg = snprintf(msg, sizeof(msg), "Hello #%d\r\n", count++);

        // Gửi len_msg byte trong msg ra UART0 (hàm này blocking tới khi đẩy hết vào FIFO/buffer)
        uart_write_bytes(UART_NUM, msg, len_msg);

        // ---- PHẦN NHẬN (đồng thời đóng vai trò delay 1 giây) ----
        // uart_read_bytes sẽ:
        //   - Trả về ngay nếu đọc được dữ liệu trước khi hết timeout
        //   - Hoặc đợi tối đa 1000ms (1 giây) rồi trả về 0 nếu không có gì
        // => Nhờ cơ chế timeout này, ta không cần gọi thêm vTaskDelay
        //    để tạo nhịp gửi 1 giây/lần.
        int len = uart_read_bytes(UART_NUM, data, RX_BUF_SIZE, 1000 / portTICK_PERIOD_MS);

        // Nếu có dữ liệu nhận được (len > 0) thì gửi ngược lại (echo)
        if (len > 0) {
            uart_write_bytes(UART_NUM, (const char *) data, len);
        }
        // Nếu len == 0 (không có gì gửi tới trong 1 giây) -> bỏ qua, lặp lại vòng while
    }
}
```