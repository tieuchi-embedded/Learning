# GPIO MUX (Multiplexer - Bộ dồn kênh)
**GPIO MUX** (hay `IO_MUX`) là một bộ chuyển mạch phần cứng nằm ngay sát chân vật lý của vi điều khiển.

**Chức năng chính**: Quyết định xem **chân vật lý** đó sẽ được sử dụng cho **mục đích gì** trong số một vài chức năng được **thiết kế sẵn** (thường là 3-6 chức năng).

**Cách hoạt động**: Nó giống như một công tắc đường ray tàu hỏa. Bạn cấu hình thanh ghi IO_MUX để gạt công tắc, chọn xem chân này sẽ nối với khối điều khiển GPIO cơ bản (để bật/tắt LED), hay nối trực tiếp với một ngoại vi tốc độ cao (như SPI flash, JTAG, Ethernet MAC).

**Đặc điểm**: `IO_MUX` kết nối trực tiếp, **độ trễ gần như bằng không**, rất phù hợp cho các tín hiệu yêu cầu tốc độ cực cao. Tuy nhiên, tính linh hoạt của nó **bị giới hạn** (một chân chỉ chọn được một vài chức năng cố định).
___

# GPIO Matrix (Ma trận chuyển mạch GPIO)
GPIO Matrix là một **mạng lưới định tuyến tín hiệu** khổng lồ và cực kỳ linh hoạt nằm sâu hơn bên trong vi điều khiển, **đứng giữa các ngoại vi** (UART, I2C, PWM, SPI...) **và bộ IO_MUX**.

**Chức năng chính**: Cho phép **định tuyến** (route) bất kỳ **tín hiệu** đầu vào/đầu ra của bất kỳ **ngoại vi nội bộ** nào tới bất kỳ **chân vật lý** nào.

**Cách hoạt động**: Nó giống như một bảng cắm dây của các nhân viên tổng đài điện thoại ngày xưa. Ví dụ: Chip ESP32 có 3 bộ UART, thay vì bộ UART2 bắt buộc phải nằm ở chân 16 và 17, GPIO Matrix cho phép bạn dùng code để "kéo dây" chân TX của UART2 ra chân số 4, số 5 hay số 22 tùy ý.

**Đặc điểm**: Cực kỳ **linh hoạt**, giúp thiết kế mạch in (PCB) dễ dàng hơn rất nhiều vì bạn có thể đổi chân bằng phần mềm nếu lỡ vẽ sai dây. Nhược điểm nhỏ là khi tín hiệu đi qua ma trận này, nó **bị trễ** đi một chút (khoảng 1-2 chu kỳ xung nhịp), do đó không phù hợp cho các giao tiếp yêu cầu tần số siêu cao (như SDIO chạy ở 50MHz+).
___

# Quy trình cấu hình bằng Thanh ghi (Ví dụ GPIO trên ESP32)

# Các thanh ghi cốt lõi:
**`IO_MUX_x_REG`**: Thanh ghi quản lý bộ dồn kênh (Multiplexer) cho từng chân. Dùng để chọn chức năng của chân (Làm GPIO hay ngoại vi khác) và bật/tắt điện trở kéo (Pull-up/Pull-down).
**`GPIO_ENABLE_REG`**: Thanh ghi kích hoạt chế độ ngõ ra (Output Enable) cho các chân từ 0-31. (Với các chân 32-39, sử dụng GPIO_ENABLE1_REG).
**`GPIO_IN_REG`**: Thanh ghi chứa dữ liệu đầu vào. Đọc thanh ghi này để biết trạng thái mức logic của chân (0 hoặc 1).
GPIO_OUT_REG: Thanh ghi chứa dữ liệu đầu ra. Ghi vào thanh ghi này để xuất mức logic.
**`GPIO_OUT_W1TS_REG`** (Write 1 to Set): Thanh ghi cho phép Set (lên 1) các chân một cách độc lập (Atomic operation, tương tự nửa dưới BSRR của STM32).
**`GPIO_OUT_W1TC_REG`** (Write 1 to Clear): Thanh ghi cho phép Clear (xuống 0) các chân một cách độc lập (Atomic operation, tương tự nửa trên BSRR hoặc BRR của STM32).

*Khi bạn dùng một chân làm GPIO cơ bản (chỉ xuất mức 0 hoặc 1), giá trị On/Off thực chất được lưu trong Thanh ghi Đầu ra (GPIO_OUT_REG) của bộ điều khiển GPIO.*
## ID 256

*Để tín hiệu từ thanh ghi này đi được ra tới chân cắm vật lý, nó phải đi qua GPIO Matrix. Trong GPIO Matrix, tín hiệu "điều khiển bằng phần mềm" này được gán một mã ID cố định là 256 (Tên macro là `SIG_GPIO_OUT_IDX`).*

**Bản chất phần cứng** của ID 256:
**Mã 256** không mang một giá trị logic (0 hay 1) cụ thể. Nó là một mã định tuyến đặc biệt.Khi bạn gõ lệnh:
`gpio_matrix_out(pin_number, 256, false, false);`
Bạn đang ra lệnh cho Matrix rằng: 
*"Đối với cái chân vật lý số pin_number này, hãy tự động kết nối nó vào đúng cái bit thứ pin_number trong khối thanh ghi Output của hệ thống."*
Phần cứng GPIO Matrix sẽ tự động giải quyết phần còn lại:
**Nếu pin_number = 2**: Matrix tự động móc tín hiệu vào bit số 2 của `GPIO_OUT_REG`.
**Nếu pin_number = 33**: Matrix tự động móc tín hiệu vào bit số 1 của `GPIO_OUT1_REG` (vì $33 - 32 = 1$).
___

## Code ví dụ
```c
#include "soc/io_mux_reg.h"     // Chứa định nghĩa các pad IO MUX
#include "soc/gpio_reg.h"       // Chứa định nghĩa thanh ghi lõi GPIO (W1TS, W1TC)
#include "soc/gpio_sig_map.h"   // Chứa ID của GPIO Matrix (SIG_GPIO_OUT_IDX)
#include "rom/gpio.h"           // Chứa hàm gpio_matrix_out của ROM

#define LED_PIN 2

void GPIO_Manual_Init(void) {
    // 1. CẤU HÌNH IO MUX
    // Gạt công tắc phần cứng để chân GPIO2 hoạt động ở chế độ GPIO cơ bản (Function 2)
    // PERIPHS_IO_MUX_GPIO2_U là tên Pad nội bộ của GPIO2
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);

    // 2. CẤU HÌNH GPIO MATRIX
    // Ép GPIO Matrix lấy tín hiệu từ lõi điều khiển phần mềm (ID: 256) và đưa ra chân số 2.
    // Cú pháp: gpio_matrix_out(pin, signal_idx, invert_out, invert_enable)
    // SIG_GPIO_OUT_IDX chính là giá trị 256
    gpio_matrix_out(LED_PIN, SIG_GPIO_OUT_IDX, false, false);

    // 3. KÍCH HOẠT NGÕ RA (OUTPUT ENABLE)
    // Mở bộ đệm đầu ra cho chân số 2 bằng cách set bit thứ 2 lên 1
    REG_WRITE(GPIO_ENABLE_W1TS_REG, (1 << LED_PIN));
}

void GPIO_Manual_On(void) {
    // Kéo ngõ ra lên mức Cao (1)
    // Ghi bit 1 vào vị trí thứ 2 của thanh ghi W1TS (Write 1 To Set)
    // Ngay lập tức, tín hiệu này đi qua Matrix (ID 256) -> MUX -> Chân GPIO2 làm LED sáng
    REG_WRITE(GPIO_OUT_W1TS_REG, (1 << LED_PIN));
}

void GPIO_Manual_Off(void) {
    // Kéo ngõ ra xuống mức Thấp (0)
    // Ghi bit 1 vào vị trí thứ 2 của thanh ghi W1TC (Write 1 To Clear)
    // Thao tác này an toàn (atomic), không làm ảnh hưởng trạng thái các chân GPIO khác
    REG_WRITE(GPIO_OUT_W1TC_REG, (1 << LED_PIN));
}
```
ESP32 có **nhiều hơn 32 chân GPIO** (lên tới 39 chân). Một thanh ghi 32-bit (như `GPIO_OUT_REG` hay `GPIO_ENABLE_REG`) chỉ quản lý được các chân từ 0 đến 31.
`GPIO_OUT1_REG`, `GPIO_ENABLE1_REG` dùng cho các chân từ 32 đến 39. Các chân từ 34 đến 39 chỉ là Input (Không có điện trở kéo, không làm Output được).
```c
void GPIO_Manual_On_Universal(uint32_t pin) {
    if (pin < 32) {
        // Xử lý cho các chân 0 - 31
        REG_WRITE(GPIO_OUT_W1TS_REG, (1 << pin));
    } else if (pin < 34) {
        // Xử lý cho các chân 32 và 33 (34-39 là Input only)
        // Phải trừ đi 32 để lấy index cho thanh ghi mới
        REG_WRITE(GPIO_OUT1_W1TS_REG, (1 << (pin - 32))); 
    }
}
```
*Đây chính xác là cách thư viện ESP-IDF xử lý ngầm khi bạn gọi hàm **gpio_set_level()***

## Phân tích luồng tín hiệu (Hardware Flow)
Để bạn dễ hình dung quá trình code ở trên đã làm gì với phần cứng bên trong con chip, hãy nhìn vào hành trình của tín hiệu:

Khi bạn gọi `GPIO_Manual_On()`: Lõi CPU ghi một bit vào thanh ghi `GPIO_OUT_W1TS_REG`. Bộ điều khiển GPIO xuất ra một tín hiệu logic mức 1.

Tại Trạm **GPIO Matrix** `(gpio_matrix_out)`: Vì ở bước Init, bạn đã nối tín hiệu điều khiển phần mềm (`SIG_GPIO_OUT_IDX`) vào Kênh số 2, Matrix sẽ cho phép tín hiệu logic 1 này đi qua hướng về phía Pad số 2.

Tại Trạm **IO MUX** (`PIN_FUNC_SELECT`): Vì công tắc MUX của Pad GPIO2 đang được gạt sang cổng `FUNC_GPIO2` (thay vì các cổng ngoại vi khác), nó **chấp nhận tín hiệu** từ Matrix và đưa thẳng ra miếng kim loại bên ngoài con chip.

***Lưu ý thêm:** Trong thực tế lập trình bằng thư viện ESP-IDF, khi bạn gọi hàm `gpio_set_direction(2, GPIO_MODE_OUTPUT)`, thư viện sẽ tự động chạy ngầm chính xác 3 bước (MUX -> Matrix -> Enable) như trong hàm `GPIO_Manual_Init` của chúng ta ở trên. Việc tự tay cấu hình thế này rất hữu ích khi bạn muốn viết bootloader hoặc thư viện siêu nhẹ ép xung thời gian thực thi.*

# ESP32 (Sử dụng thư viện ESP-IDF)
ESP-IDF là framework chính thức của Espressif. Thư viện này quản lý cấu hình GPIO thông qua Struct gpio_config_t và các hàm API tiêu chuẩn, tương đương với thư viện HAL trên STM32, giúp lập trình viên không cần thao tác trực tiếp với IO_MUX hay địa chỉ thanh ghi.

***Lưu ý**: ESP-IDF sử dụng khái niệm bit_mask để có thể cấu hình nhiều chân cùng lúc trong một Struct.*
```c
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Định nghĩa chân để code dễ đọc hơn
#define LED_PIN   GPIO_NUM_2
#define BTN_PIN   GPIO_NUM_0

// Hàm khởi tạo cấu hình GPIO
void MX_GPIO_Init(void) {
    gpio_config_t io_conf = {};

    // 1. Cấu hình chân LED (GPIO2)
    // Đặt mức thấp lúc khởi động để đảm bảo LED tắt
    gpio_set_level(LED_PIN, 0);

    io_conf.pin_bit_mask = (1ULL << LED_PIN);     // Chọn chân cấu hình (dùng bitmask)
    io_conf.mode = GPIO_MODE_OUTPUT;              // Chế độ ngõ ra
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;     // Không dùng điện trở kéo lên
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE; // Không dùng điện trở kéo xuống
    io_conf.intr_type = GPIO_INTR_DISABLE;        // Không sử dụng ngắt
    gpio_config(&io_conf);                        // Áp dụng cấu hình

    // 2. Cấu hình chân Nút nhấn (GPIO0)
    io_conf.pin_bit_mask = (1ULL << BTN_PIN);
    io_conf.mode = GPIO_MODE_INPUT;               // Chế độ ngõ vào
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;      // Nút BOOT cần Pull-up nội để giữ mức 1
    gpio_config(&io_conf);                        // Áp dụng cấu hình
}

void app_main(void) {
    // Khởi tạo ngoại vi GPIO
    MX_GPIO_Init();

    while (1) {
        // Đọc trạng thái nút nhấn trên GPIO0
        // Trả về 0 hoặc 1
        if (gpio_get_level(BTN_PIN) == 0) {
            // Nếu nút được nhấn (GND), bật LED
            gpio_set_level(LED_PIN, 1);
            
            // Chống dội phím (Debounce) đơn giản sử dụng FreeRTOS delay
            vTaskDelay(100 / portTICK_PERIOD_MS); 
        } else {
            // Nếu không nhấn, tắt LED
            gpio_set_level(LED_PIN, 0);
        }
    }
}
```