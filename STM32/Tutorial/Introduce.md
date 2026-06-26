# Hướng dẫn Lập trình STM32

## 1. Môi trường lập trình
Để phát triển ứng dụng trên dòng vi điều khiển STM32, chúng ta cần một chuỗi công cụ (Toolchain) hoàn chỉnh bao gồm các giai đoạn: Biên tập (Edit), Biên dịch (Compile), và Nạp/Gỡ lỗi (Flash/Debug).

### Giải pháp tích hợp: STM32CubeIDE
Giải pháp phổ biến và tối ưu nhất hiện nay là sử dụng **STM32CubeIDE** do chính hãng STMicroelectronics cung cấp. Đây là một môi trường phát triển tích hợp (IDE) dựa trên nền tảng Eclipse, được tích hợp sẵn:
* **Bộ biên dịch:** GNU Tools for STM32 (arm-none-eabi-gcc).
* **Trình nạp và gỡ lỗi:** Tích hợp ST-LINK GDB Server.
* **Bộ cấu hình đồ họa:** Tích hợp sẵn STM32CubeMX giúp cấu hình chân (Pinout), Clock, và ngoại vi bằng giao diện kéo thả trực quan, tự động sinh mã nguồn khởi tạo.

### Các giải pháp linh hoạt khác
Bên cạnh STM32CubeIDE, bạn hoàn toàn có thể tách rời các công cụ tùy theo thói quen và nhu cầu dự án:
* **Edit (Trình biên tập mã nguồn):** VS Code (khuyên dùng vì kho extension phong phú), Keil C V5, Notepad++, Sublime Text.
* **Compile (Bộ biên dịch):** * **Arm Compiler (AC5/AC6):** Đi kèm với Keil C, tối ưu hóa tốt cho nhân ARM.
  * **ARM GCC (arm-none-eabi-gcc):** Trình biên dịch mã nguồn mở mãnh mẽ, dễ tích hợp vào VS Code thông qua CMake hoặc Makefiles.
* **Flash (Trình nạp code):** Keil C, STM32CubeProgrammer (giao diện GUI hoặc CLI), ST-LINK Utility.

---

### Hai phương pháp tiếp cận lập trình trong tài liệu này

Trong suốt hành trình học tập, chúng ta sẽ tiếp cận vi điều khiển qua hai phương pháp tư duy khác nhau:

1. **Lập trình ghi giá trị trực tiếp vào thanh ghi (Register-level Programming):**
   * **Ưu điểm:** Tối ưu hóa dung lượng bộ nhớ (Code size), tốc độ thực thi đạt mức tối đa của phần cứng, giúp người học hiểu sâu sắc bản chất kiến trúc phần cứng bên trong MCU.
   * **Nhược điểm:** Thời gian phát triển lâu, code phức tạp, khó bảo trì, khó chuyển đổi (porting) sang các dòng chip khác và đòi hỏi phải đọc tài liệu kỹ thuật rất nhiều.

2. **Lập trình bằng thư viện HAL (Hardware Abstraction Layer):**
   * **Ưu điểm:** Thư viện chuẩn hóa cao do chính hãng ST cung cấp, cho phép phát triển ứng dụng cực nhanh, tự động sinh code từ cấu hình đồ họa, dễ dàng chuyển đổi mã nguồn giữa các dòng STM32 khác nhau.
   * **Nhược điểm:** Dung lượng code sinh ra lớn do chứa nhiều hàm kiểm tra an toàn, tốc độ thực thi một số tác vụ có thể chậm hơn so với can thiệp thanh ghi trực tiếp.

---

## 2. Lập trình thanh ghi STM32

Để làm chủ việc lập trình thanh ghi, bạn cần vững vàng 3 nền tảng cốt lõi: Tư duy con trỏ vùng nhớ, Kỹ thuật thao tác bit, và Kỹ năng tra cứu tài liệu kỹ thuật (Datasheet/Reference Manual).

### 2.1. Con trỏ trong vi điều khiển (Memory-Mapped I/O)
Trong kiến trúc ARM Cortex-M của STM32, tất cả các ngoại vi (GPIO, Timer, UART, ADC...) đều được ánh xạ vào một không gian bộ nhớ phẳng (Memory Map) dung lượng 4GB. Mỗi thanh ghi của ngoại vi thực chất là một ô nhớ có địa chỉ xác định. Để giao tiếp với phần cứng, chúng ta sử dụng **con trỏ** trong ngôn ngữ C để ép kiểu một địa chỉ số thành một vùng nhớ cấu trúc.

**Từ khóa quan trọng: `volatile`**
Khi làm việc với thanh ghi, việc sử dụng từ khóa `volatile` là **bắt buộc**. Từ khóa này báo cho trình biên dịch biết rằng giá trị của ô nhớ này có thể thay đổi bất kỳ lúc nào do phần cứng tác động (ví dụ: nút nhấn bên ngoài thay đổi trạng thái chân Input), do đó trình biên dịch không được tối ưu hóa bằng cách lưu giá trị vào thanh ghi CPU mà phải luôn đọc/ghi trực tiếp từ ô nhớ RAM/Ngoại vi.

**Ví dụ minh họa:**
Giả sử địa chỉ thanh ghi điều khiển xuất dữ liệu (Thanh ghi `ODR` của Port C) đặt tại địa chỉ `0x4001 100C`.

```c
// Định nghĩa địa chỉ thanh ghi bằng con trỏ
#define PORTC_ODR   (*(volatile uint32_t *)0x4001100C)

int main(void) {
    // Ghi trực tiếp giá trị 0x00000010 vào ô nhớ để bật chân PC4
    PORTC_ODR = 0x00000010; 
}
```
## 2.2. Kỹ thuật Set/Reset bit và thanh ghi
Chúng ta sử dụng các toán tử logic bit `(&, |, ~, ^, <<)` để thay đổi trạng thái bit mong muốn mà không làm ảnh hưởng đến các bit khác trong cùng thanh ghi. Toán tử dịch bit (1 << n) được dùng để tạo ra một mặt nạ (mask) trỏ đúng vào vị trí bit thứ n.

1. **Set bit** (Đưa bit lên 1): Sử dụng toán tử HOẶC (|). Bất kỳ bit nào HOẶC với 1 cũng bằng 1.

Công thức: `REG |= (1 << n);`

2. **Reset bit** (Xóa bit về 0): Sử dụng toán tử VÀ (&) đảo với toán tử PHỦ ĐỊNH (~). Bất kỳ bit nào VÀ với 0 cũng bằng 0.

Công thức: `REG &= ~(1 << n);`

3. **Toggle bit** (Đảo trạng thái bit): Sử dụng toán tử XOR (^).

Công thức: `REG ^= (1 << n);`

## 2.3. Quy trình đọc tài liệu kỹ thuật (Datasheet & Reference Manual)
Hãng ST cung cấp hai tài liệu quan trọng nhất:

**Datasheet** (DS): Chứa thông số phần cứng vật lý, sơ đồ chân (Pinout), dải điện áp và sơ đồ khối tổng thể bộ nhớ.

**Reference Manual** (RM): Mô tả chi tiết chức năng hoạt động của từng ngoại vi và bản đồ chi tiết của từng thanh ghi (địa chỉ offset, chức năng từng bit).

### Quy trình 4 bước tìm địa chỉ thanh ghi từ Reference Manual:

- Tìm địa chỉ gốc (Base Address): Mở mục Memory Map để tìm vùng địa chỉ mà ngoại vi đó quản lý. (VD: GPIOC là 0x4001 1000).
- Tìm địa chỉ lệch (Offset Address): Đi đến chương của ngoại vi, cuộn xuống Register Description. (VD: GPIOx_ODR có offset 0x0C).
- Tính toán địa chỉ tuyệt đối: **Base + Offset**. (VD: 0x40011000 + 0x0C = 0x4001100C).
- Tra cứu chức năng các bit: Đọc bảng mô tả chi tiết của thanh ghi đó để biết cần cấu hình bit nào.

___

## Lập trình thư viện HAL (Hardware Abstraction Layer)
Thư viện HAL ẩn đi sự phức tạp của tầng thanh ghi bằng cách cung cấp các hàm (API) hướng đối tượng. Thay vì tính toán địa chỉ vùng nhớ, bạn làm việc với các cấu trúc dữ liệu (Struct) và hằng số định nghĩa sẵn.
Quy trình phát triển ứng dụng bằng HAL:
Bước 1 (Cấu hình đồ họa): Sử dụng STM32CubeMX để chọn chân, bật Clock, thiết lập tần số hoạt động.
Bước 2 (Sinh code tự động): Nhấn Generate Code. IDE sẽ tự tạo cấu trúc dự án, copy các file .c/.h và viết sẵn các hàm khởi tạo như SystemClock_Config(), MX_GPIO_Init().
Bước 3 (Viết code ứng dụng): Viết mã nguồn vào trong các cặp thẻ chú thích an toàn để tránh mất code khi sinh lại tự động:
```c
/* USER CODE BEGIN 3 */
HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
HAL_Delay(500); 
/* USER CODE END 3 */
```

Dưới đây là quy trình chi tiết từng bước để bạn có thể tìm, đọc và hiểu bất kỳ hàm HAL nào trong dự án:

### Bước 1: Xác định cấu trúc thư mục chứa thư viện HAL
Khi tạo một dự án STM32 (ví dụ trên STM32CubeIDE), thư viện HAL sẽ nằm hoàn toàn trong thư mục Drivers. Cấu trúc chuẩn như sau:

- **Tên_Dự_Án**/
    - **Drivers**/
        - **STM32F4xx_HAL_Driver**/
            - **Inc**/ (Includes): Chứa các file tiêu đề (.h). Đây là nơi định nghĩa các cấu trúc dữ liệu (struct), các hằng số, các Macro cấu hình và các nguyên mẫu hàm (Function Prototype).
                - **Src**/ (Sources): Chứa các file mã nguồn (.c). Đây là nơi viết code thực thi chi tiết của các hàm.


## Bước 2: Sử dụng các công cụ điều hướng nhanh của IDE
Thay vì tìm kiếm thủ công từng file, hãy tận dụng tối đa các phím tắt của IDE (STM32CubeIDE, VS Code, Keil C):

- Tìm hàm theo tên: Sử dụng tổ hợp phím Ctrl + Open Resource (hoặc Ctrl + P trên VS Code) rồi gõ tên file ngoại vi cần tìm, ví dụ: ``stm32f4xx_hal_gpio.c``.

- Nhảy nhanh đến hàm (Go to Definition): Khi bạn thấy một hàm HAL trong file main.c (ví dụ: `HAL_GPIO_Init`), hãy giữ phím `Ctrl và click` chuột trái vào tên hàm (hoặc nhấn phím F3). IDE sẽ ngay lập tức đưa bạn đến file .c chứa mã nguồn chi tiết của hàm đó.

- Xem khai báo hằng số (Go to Declaration): Nếu muốn xem một tham số truyền vào (ví dụ: GPIO_PIN_13) được định nghĩa là gì, hãy `Ctrl + Click` vào tham số đó để nhảy tới file .h nơi chứa các danh sách Macro `#define`.



### Bước 3: Đọc khối chú thích Doxygen (Chìa khóa hiểu chức năng và tham số)
Hãng ST viết mã nguồn theo chuẩn tài liệu Doxygen. Ngay phía trên mỗi hàm trong file .c, luôn có một khối chú thích (Block Comment) cực kỳ chi tiết bằng tiếng Anh. Bạn chỉ cần đọc khối này là biết tất cả:

**@brief**: Mô tả ngắn gọn chức năng của hàm này dùng để làm gì.

**@param**: Giải thích chi tiết từng tham số truyền vào. Đặc biệt, ST luôn ghi rõ các giá trị hoặc Macro hợp lệ cho tham số đó. (Ví dụ: @param GPIO_Pin specifies the port bit to be written. This parameter can be one of GPIO_PIN_x where x can be (0..15)).

**@retval**: Giá trị trả về của hàm (Ví dụ: HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT).


### Bước 4: Phân tích cấu trúc (Giải phẫu) một hàm HAL
Hàm HAL tuân theo quy tắc đặt tên rất nghiêm ngặt: `HAL_[Tên_Ngoại_Vi]_[Hành_Động]`.
Các tham số truyền vào thường được chia làm 2 loại chính:

Hàm tương tác trực tiếp ngoại vi đơn giản (như **GPIO**): Tham số đầu tiên thường là vùng nhớ của ngoại vi (**GPIOx** như **GPIOA, GPIOB**), tham số tiếp theo là các cấu hình cụ thể.

Hàm sử dụng cấu trúc quản lý (**Handle Struct - như UART, SPI, TIMER**): Tham số đầu tiên luôn là một con trỏ trỏ tới cấu trúc quản lý ngoại vi (ví dụ: **&huart1, &htim2**). Cấu trúc này chứa mọi thông tin cấu hình và trạng thái của ngoại vi đó.