# Hướng dẫn Lập trình ESP32 (ESP32 classic, dual-core)

## 1. Môi trường lập trình
Để phát triển ứng dụng trên dòng vi điều khiển ESP32, chúng ta cần một chuỗi công cụ (Toolchain) hoàn chỉnh bao gồm các giai đoạn: Biên tập (Edit), Biên dịch (Compile), và Nạp/Gỡ lỗi (Flash/Debug).

### Giải pháp tích hợp: ESP-IDF (Espressif IoT Development Framework)
Giải pháp chính thức và đầy đủ nhất hiện nay là sử dụng **ESP-IDF** do chính hãng Espressif cung cấp. Khác với STM32CubeIDE (một IDE đồ họa độc lập), ESP-IDF về bản chất là một **bộ khung phát triển (framework) dựa trên dòng lệnh**, được tích hợp vào IDE thông qua extension. Bộ công cụ này bao gồm:
* **Bộ biên dịch:** Xtensa GCC toolchain (`xtensa-esp32-elf-gcc`) — biên dịch cho lõi Xtensa LX6 của ESP32.
* **Hệ thống build:** CMake + Ninja, được điều khiển qua công cụ dòng lệnh `idf.py`.
* **Trình nạp:** `esptool.py` — nạp firmware qua UART (không cần mạch nạp ngoài như ST-LINK).
* **Trình gỡ lỗi:** OpenOCD + GDB (cần mạch JTAG ngoài, ví dụ ESP-Prog, để gỡ lỗi step-by-step).
* **Công cụ cấu hình:** `idf.py menuconfig` — một menu dạng văn bản (không phải kéo-thả đồ họa như CubeMX) để bật/tắt tính năng, cấu hình tần số CPU, kích thước Flash, v.v. Việc cấu hình chân (pinout) trên ESP32 **không** làm bằng công cụ đồ họa mà làm trực tiếp trong code, vì hầu hết chân GPIO của ESP32 đều có thể gán linh hoạt cho hầu hết ngoại vi qua **GPIO Matrix**.

### Các giải pháp linh hoạt khác
* **Edit (Trình biên tập mã nguồn):** VS Code với extension **ESP-IDF** (khuyên dùng, chính thức từ Espressif), CLion, Eclipse (qua plugin ESP-IDF), Vim/Neovim với clangd.
* **Compile (Bộ biên dịch):**
  * **Xtensa GCC:** Trình biên dịch chính thức cho ESP32/ESP32-S2/ESP32-S3.
  * **Arduino-ESP32 (Arduino core):** Một lớp tương thích Arduino đặt trên ESP-IDF, giúp dễ tiếp cận nhưng không phải trọng tâm tài liệu này.
* **Flash (Trình nạp code):** `idf.py flash`, `esptool.py write_flash`, hoặc công cụ đồ họa **ESP Flash Download Tool**.

---

### Hai phương pháp tiếp cận lập trình trong tài liệu này

1. **Lập trình ghi giá trị trực tiếp vào thanh ghi (Register-level Programming):**
   * **Ưu điểm:** Tối ưu hóa dung lượng bộ nhớ và tốc độ thực thi tối đa, giúp hiểu sâu kiến trúc phần cứng bên trong ESP32 (GPIO Matrix, IO MUX, bus ngoại vi APB).
   * **Nhược điểm:** Thời gian phát triển lâu, phải tự quản lý việc bật clock/reset ngoại vi (vốn được driver làm sẵn), khó porting sang dòng chip RISC-V (ESP32-C3/C6) vì sơ đồ thanh ghi khác hoàn toàn.

2. **Lập trình bằng các Component/Driver của ESP-IDF (tương đương vai trò của HAL trên STM32):**
   * **Ưu điểm:** API chuẩn hóa do Espressif cung cấp, tự động xử lý GPIO Matrix, clock gating, ngắt; tích hợp sẵn với FreeRTOS (ESP-IDF build trên nền FreeRTOS); dễ chuyển đổi giữa các dòng chip ESP32 khác nhau.
   * **Nhược điểm:** Dung lượng code và độ trễ lớn hơn do qua nhiều lớp trừu tượng (driver → HAL nội bộ → LL/register).

---

## 2. Lập trình thanh ghi ESP32

### 2.1. Con trỏ và ánh xạ bộ nhớ ngoại vi (Memory-Mapped I/O)
Trên kiến trúc Xtensa LX6 của ESP32, mọi ngoại vi (GPIO, UART, Timer, SPI...) được ánh xạ vào không gian địa chỉ bộ nhớ. Để truy cập, ta dùng con trỏ trong C để ép kiểu một địa chỉ số thành vùng nhớ thanh ghi.

**Từ khóa quan trọng: `volatile`**
Giống như trên STM32, từ khóa `volatile` là **bắt buộc** khi khai báo con trỏ trỏ tới thanh ghi. Nó báo cho trình biên dịch biết giá trị ô nhớ này có thể bị phần cứng thay đổi bất kỳ lúc nào, nên không được tối ưu hóa bằng cách cache giá trị vào thanh ghi CPU.

### 2.2. Kỹ thuật Set/Reset bit và thanh ghi "Write-1-to-Set/Clear" (W1TS/W1TC)
Các toán tử bit cơ bản hoạt động giống hoàn toàn như trên mọi vi điều khiển khác:

1. **Set bit:** `REG |= (1 << n);`
2. **Reset bit:** `REG &= ~(1 << n);`
3. **Toggle bit:** `REG ^= (1 << n);`

**Điểm đặc thù của ESP32:** Việc sử dụng các toán tử bit cơ bản `(|=, &=)` trên thanh ghi GPIO_OUT_REG yêu cầu CPU phải "Đọc - Sửa - Ghi". Trong môi trường đa nhiệm (FreeRTOS) như ESP-IDF, thao tác này có thể bị ngắt (ISR) chen ngang gây sai lệch dữ liệu (Race condition).

Để giải quyết, ESP32 cung cấp cặp thanh ghi đặc biệt cho phép **set/clear** bit một cách nguyên tử (**atomic**) — khái niệm tương đương thanh ghi `BSRR` trên STM32:

| Thanh ghi | Offset | Chức năng |
|---|---|---|
| `GPIO_OUT_W1TS_REG` | `0x08` | Ghi 1 vào bit nào → bit đó trong `GPIO_OUT_REG` được **Set** lên 1 |
| `GPIO_OUT_W1TC_REG` | `0x0C` | Ghi 1 vào bit nào → bit đó trong `GPIO_OUT_REG` được **Clear** về 0 |

```c
#define GPIO_OUT_W1TS_REG  (*(volatile uint32_t *)0x3FF44008)
#define GPIO_OUT_W1TC_REG  (*(volatile uint32_t *)0x3FF4400C)

// Dùng toán tử GÁN (=), tuyệt đối KHÔNG DÙNG OR (|=) với thanh ghi W1TS/W1TC
GPIO_OUT_W1TS_REG = (1 << 2);  // Set GPIO2 lên HIGH một cách an toàn
GPIO_OUT_W1TC_REG = (1 << 2);  // Clear GPIO2 xuống LOW một cách an toàn 
```
>Tại sao lại dùng = mà không dùng |=? > Thanh ghi W1TS/W1TC được thiết kế phần cứng theo kiểu "chỉ kích hoạt những bit mang giá trị 1 được ghi vào". Nếu bạn ghi 0, nó sẽ không làm gì cả. Do đó, ta chỉ cần gán thẳng = để ghi đúng bit cần kích hoạt. Nếu dùng |=, CPU sẽ cố gắng đọc giá trị của thanh ghi này ra (vốn luôn bằng 0 hoặc rác), gây lãng phí chu kỳ máy.

>Vì ESP-IDF luôn chạy trên nền **FreeRTOS** (đa nhiệm), việc dùng W1TS/W1TC được khuyến nghị hơn `|=` / `&=` thông thường để tránh race-condition khi nhiều task hoặc ISR cùng thao tác trên một thanh ghi.

___
### 2.3. Quy trình đọc tài liệu kỹ thuật (Datasheet & Technical Reference Manual)
Espressif cung cấp hai tài liệu tương đương với DS/RM của ST:

* **Datasheet:** Thông số điện, pinout vật lý, sơ đồ khối tổng thể.
* **Technical Reference Manual (TRM):** Mô tả chi tiết từng ngoại vi và bản đồ thanh ghi (địa chỉ offset, ý nghĩa từng bit) — tương đương Reference Manual của STM32.

### Quy trình 4 bước tìm địa chỉ thanh ghi từ TRM:
1. **Tìm địa chỉ gốc (Base Address):** Mở chương *Register Summary* / *System and Memory* để tìm `DR_REG_xxx_BASE` của ngoại vi. (VD: GPIO là `0x3FF44000`).
2. **Tìm địa chỉ lệch (Offset):** Vào chương mô tả ngoại vi, tìm bảng *Register Summary*. (VD: `GPIO_OUT_REG` có offset `0x0004`).
3. **Tính địa chỉ tuyệt đối:** Base + Offset. (VD: `0x3FF44000 + 0x04 = 0x3FF44004`).
4. **Tra cứu chức năng từng bit:** Đọc bảng mô tả chi tiết của thanh ghi để biết bit nào điều khiển chức năng gì.

> Ngoài ra, ESP-IDF cũng cung cấp sẵn các file `.h` định nghĩa toàn bộ địa chỉ này (xem mục 3.2), nên trong thực tế bạn ít khi phải tự tính tay — nhưng việc hiểu quy trình vẫn rất cần thiết để đọc hiểu mã nguồn driver.
___
**Ví dụ minh họa:**
Vùng ngoại vi GPIO của ESP32 có địa chỉ gốc `DR_REG_GPIO_BASE = 0x3FF44000`. Thanh ghi xuất dữ liệu `GPIO_OUT_REG` nằm ở offset `0x04`, tức địa chỉ tuyệt đối `0x3FF44004`.

```c
#include "soc/io_mux_reg.h" // Chứa định nghĩa các Pad MUX

// Định nghĩa địa chỉ thanh ghi bằng con trỏ
#define GPIO_OUT_REG    (*(volatile uint32_t *)0x3FF44004)
#define GPIO_ENABLE_REG (*(volatile uint32_t *)0x3FF44020)

void app_main(void) {
    // Bước 1: Cấu hình IO MUX để gán chức năng GPIO cho chân vật lý số 4
    // Nếu bỏ qua bước này, chân có thể vẫn đang ở chế độ JTAG hoặc Reset mặc định
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO4_U, FUNC_GPIO4);

    // Bước 2: Kích hoạt chế độ Output (Output Enable) cho chân GPIO4 (Set bit 4)
    GPIO_ENABLE_REG |= (1 << 4);

    // Bước 3: Xuất mức logic HIGH (1) ra chân GPIO4
    GPIO_OUT_REG |= (1 << 4);
}
```

> **Lưu ý quan trọng:** Trước khi GPIO có thể xuất tín hiệu, chân đó phải được "bật" làm output qua thanh ghi `GPIO_ENABLE_REG` (offset `0x20`, địa chỉ `0x3FF44020`) — tương tự việc cấu hình `MODER` trên STM32. Việc quên bước này là lỗi rất thường gặp khi lập trình thanh ghi trên ESP32.

---

## 3. Lập trình bằng Driver/Component của ESP-IDF

ESP-IDF cung cấp các **component driver** (ví dụ `driver/gpio.h`, `driver/uart.h`, `driver/spi_master.h`) đóng vai trò tương đương thư viện HAL của STM32: ẩn việc tính toán địa chỉ thanh ghi, thay vào đó làm việc với struct cấu hình và hàm API.

### Quy trình phát triển ứng dụng bằng ESP-IDF driver:
* **Bước 1 (Cấu hình project):** Dùng `idf.py menuconfig` để bật tính năng cần thiết (Wi-Fi, Bluetooth, kích thước stack FreeRTOS, tần số CPU...). Khác với CubeMX, đây **không phải** công cụ cấu hình pinout đồ họa.
* **Bước 2 (Viết struct cấu hình ngoại vi bằng tay):** Vì không có code-generator như CubeMX, bạn tự khai báo struct cấu hình (ví dụ `gpio_config_t`) ngay trong `main.c` — không có khái niệm vùng `/* USER CODE BEGIN/END */` vì không có gì để "sinh lại" tự động ghi đè code của bạn.
* **Bước 3 (Build/Flash/Monitor):** `idf.py build`, `idf.py -p <PORT> flash`, `idf.py monitor` (xem log qua UART, tương đương Terminal trong Keil/CubeIDE).

**Ví dụ — Nhấp nháy LED trên GPIO2 (tương đương ví dụ `HAL_GPIO_TogglePin` trong tài liệu gốc):**
```c
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define LED_PIN GPIO_NUM_2

void app_main(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_PIN),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    while (1) {
        gpio_set_level(LED_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(500));   // tương đương HAL_Delay(500)
        gpio_set_level(LED_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
```
> **Khác biệt cốt lõi so với HAL/STM32:** `HAL_Delay()` trên STM32 (bare-metal, không hệ điều hành) là vòng lặp chờ bận (blocking) chiếm CPU. `vTaskDelay()` trên ESP-IDF là một lệnh gọi tới **FreeRTOS** — nó thực sự "nhường" CPU cho các task khác trong thời gian chờ, vì ESP-IDF luôn chạy trên nền hệ điều hành thời gian thực.

---

## 4. Tìm hiểu mã nguồn driver trong ESP-IDF
Dưới đây là quy trình tìm, đọc và hiểu bất kỳ hàm driver nào trong ESP-IDF — tương đương quy trình đọc HAL trên STM32.

### Bước 1: Xác định cấu trúc thư mục chứa ESP-IDF
ESP-IDF không nằm trong thư mục project như `Drivers/` của STM32CubeIDE, mà nằm ở một vị trí cài đặt riêng, được biến môi trường `IDF_PATH` trỏ tới (thường là `~/esp/esp-idf`). Cấu trúc quan trọng:

* **`$IDF_PATH/components/`**
  * **`driver/`** — Chứa API tầng cao mà bạn gọi trực tiếp trong code (`gpio.h`, `gpio.c`, `uart.h`, `uart.c`...). Đây là lớp tương đương vai trò "HAL" của STM32.
  * **`hal/`** — Một lớp **trừu tượng hóa nội bộ thấp hơn** mà chính ESP-IDF cũng gọi là "HAL" (`hal/gpio_hal.h`), nằm giữa lớp `driver/` và lớp thanh ghi thô.
  * **`soc/<chip>/include/soc/`** — Chứa định nghĩa địa chỉ thanh ghi thô dạng `#define` (ví dụ `gpio_reg.h` chứa `GPIO_OUT_REG`, `DR_REG_GPIO_BASE`...) — đây chính là nơi tương đương file `stm32f4xx.h` định nghĩa thanh ghi của ST.

### Bước 2: Sử dụng công cụ điều hướng nhanh của IDE
* **Tìm hàm theo tên:** `Ctrl + P` trên VS Code, gõ tên file cần tìm, ví dụ `gpio.c`.
* **Nhảy đến định nghĩa hàm (Go to Definition):** Đặt con trỏ vào tên hàm (ví dụ `gpio_config`) trong `main.c`, nhấn `F12` hoặc `Ctrl + Click`. VS Code (với extension C/C++ và ESP-IDF) sẽ đưa bạn tới file `.c` chứa mã nguồn chi tiết — tương tự F3 trên STM32CubeIDE.
* **Xem khai báo hằng số:** `Ctrl + Click` vào tham số (ví dụ `GPIO_MODE_OUTPUT`) để nhảy tới file `.h` chứa định nghĩa enum/macro.

### Bước 3: Đọc khối chú thích Doxygen
Mã nguồn ESP-IDF cũng tuân theo chuẩn Doxygen, với cấu trúc tương tự ST nhưng dùng `@return` thay vì `@retval`:

* **`@brief`** — Mô tả ngắn gọn chức năng hàm.
* **`@param`** — Giải thích từng tham số, ghi rõ giá trị/macro hợp lệ (VD: `@param gpio_num GPIO number. If you want to set the output level of e.g. GPIO16, gpio_num should be GPIO_NUM_16`).
* **`@return`** — Giá trị trả về, thường là `esp_err_t` (VD: `ESP_OK`, `ESP_ERR_INVALID_ARG`) — tương đương `HAL_OK`/`HAL_ERROR` của STM32.

### Bước 4: Phân tích cấu trúc (giải phẫu) một hàm driver ESP-IDF
Hàm driver tuân theo quy tắc đặt tên: `[ten_ngoai_vi]_[hanh_dong]`, ví dụ `gpio_set_level`, `uart_write_bytes`, `spi_device_transmit`. Tham số được chia 2 loại chính:

* **Ngoại vi đơn giản, không cần trạng thái phức tạp (như GPIO):** Tham số đầu tiên thường là **số hiệu/chỉ số** của đối tượng (`gpio_num_t gpio_num`, `uart_port_t uart_num`) — khác với STM32 dùng con trỏ vùng nhớ (`GPIOx`).
* **Ngoại vi cần quản lý trạng thái phức tạp (như SPI, I2C, UART nâng cao):** Sử dụng một **handle** (con trỏ tới cấu trúc quản lý), ví dụ `spi_device_handle_t`, `i2c_master_dev_handle_t` — đây là khái niệm tương đương trực tiếp với `&huart1`, `&htim2` của HAL trên STM32, chứa toàn bộ thông tin cấu hình và trạng thái của ngoại vi đó.