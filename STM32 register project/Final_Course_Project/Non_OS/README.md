# STM32 Bootloader & Multi-App Firmware Project

Dự án firmware nhúng cho vi điều khiển STM32, bao gồm **Bootloader tùy chỉnh**, **2 ứng dụng chính** và **2 firmware update** có khả năng nạp đè lẫn nhau qua cơ chế OTA (Over-The-Air).

Project description make up by AI
---

## Cấu trúc dự án

```
project/
├── Bootloader/          # Bootloader tùy chỉnh
├── App1/                # Ứng dụng 1 – Cảm biến nhiệt độ & LED & UART
├── App2/                # Ứng dụng 2 – La bàn số & Firmware Update host
├── FirmwareUpdate1/     # FW Update 1 – Detect độ nghiêng (Accelerometer)
└── FirmwareUpdate2/     # FW Update 2 – Detect rung lắc (Gyroscope SPI)
```

---

## Sơ đồ Flash Memory Layout

| Vùng nhớ         | Địa chỉ bắt đầu | Kích thước | Nội dung          |
|------------------|-----------------|------------|-------------------|
| Bootloader       | `0x08000000`    | 16 KB      | Bootloader code   |
| App 1            | `0x08004000`    | 64 KB      | Application 1     |
| App 2            | `0x08014000`    | 64 KB      | Application 2     |
| Metadata/Flag    | `0x08024000`    | ~4 KB      | Boot flag, version|

> **Lưu ý:** Firmware Update thế chỗ App 1, kích thước tối đa **64 KB**.

---

## Bootloader

### Chức năng

Bootloader khởi động hệ thống và quyết định nhảy vào **App 1** hoặc **App 2** dựa trên trạng thái nút nhấn.

### Logic khởi động

```
Reset xảy ra
    │
    ├─ [User Button GIỮ khi Reset] ──► Nhảy vào App 2
    │
    └─ [Bình thường]               ──► Nhảy vào App 1
```

### Cách hoạt động

1. Sau Reset, Bootloader kiểm tra trạng thái **User Button**.
2. Nếu nút **được giữ** → nhảy sang **App 2**.
3. Nếu nút **không nhấn** → nhảy sang **App 1** (mặc định).
4. Trước khi nhảy, Bootloader cấu hình lại **Vector Table** và deinitialize ngoại vi.

---

## App 1 – Cảm biến & Điều khiển cơ bản

### Chức năng

| Tính năng           | Mô tả                                              |
|---------------------|----------------------------------------------------|
| Đo nhiệt độ chip | Sử dụng ADC nội (internal temperature sensor)      |
| Bật/tắt LED      | Điều khiển LED người dùng qua nút nhấn hoặc lệnh  |
| UART Interrupt   | Nhận lệnh và trả kết quả qua UART (interrupt-driven)|

### Giao thức UART

- **Baud rate:** 115200
- **Format:** 8N1
- **Chế độ:** Interrupt (không blocking)

### Ví dụ lệnh UART

```
GET_TEMP     → Trả về: "TEMP: 35.2 C\r\n"
LED_ON       → Bật LED, trả về: "LED: ON\r\n"
LED_OFF      → Tắt LED, trả về: "LED: OFF\r\n"
```

---

## App 2 – La bàn số & Firmware Update Host

### Chức năng

| Tính năng              | Mô tả                                                |
|------------------------|------------------------------------------------------|
| La bàn số           | Đọc từ trường qua Magnetometer LSM303AGR (I2C)       |
| Firmware Update     | Nhận firmware mới qua UART và nạp đè vào vùng App 1 |

### Cảm biến LSM303AGR – Magnetometer

- **Giao tiếp:** I2C
- **Địa chỉ I2C:** `0x1E` (magnetometer)
- **Chức năng:** Đọc 3 trục từ trường (X, Y, Z), tính góc heading

### Cơ chế Firmware Update

```
App 2 nhận firmware qua UART
    │
    ├─ Kiểm tra kích thước (phải < 64 KB)
    ├─ Xóa vùng Flash App 1
    ├─ Ghi firmware mới từng page
    ├─ Kiểm tra CRC/checksum
    └─ Reset → Bootloader → App 1 mới
```

> Hỗ trợ **update nhiều lần** (mỗi lần xóa và ghi đè).

---

## Firmware Update 1 – Detect Độ Nghiêng

> Firmware này **thế chỗ App 1** sau khi được nạp từ App 2.

### Chức năng

| Tính năng              | Mô tả                                                 |
|------------------------|-------------------------------------------------------|
| Đo độ nghiêng       | Accelerometer LSM303AGR (I2C) – đọc trục X, Y, Z     |
| Nháy LED theo hướng | Nghiêng hướng nào → LED nháy theo pattern hướng đó   |
| Trả kết quả UART    | Gửi góc nghiêng về máy tính qua UART                 |

### Cảm biến LSM303AGR – Accelerometer

- **Giao tiếp:** I2C
- **Địa chỉ I2C:** `0x19` (accelerometer)
- **Output:** Góc Roll, Pitch tính từ gia tốc trọng trường

### Logic LED

```
Nghiêng TRÁI   → LED nháy: ← (short-long-short)
Nghiêng PHẢI   → LED nháy: → (long-short-long)
Nghiêng LÊN    → LED nháy: ↑ (nhanh 3 lần)
Nghiêng XUỐNG  → LED nháy: ↓ (chậm 3 lần)
Nằm phẳng      → LED sáng liên tục
```

### Output UART

```
TILT: Roll=12.5 Pitch=-8.3 Dir=LEFT\r\n
```

---

## Firmware Update 2 – Detect Rung Lắc

> Firmware này cũng **thế chỗ App 1** (có thể nạp sau FW Update 1).

### Chức năng

| Tính năng              | Mô tả                                              |
|------------------------|----------------------------------------------------|
| Detect rung lắc     | Gyroscope I3G4250D qua giao tiếp SPI               |
| Nháy LED liên tục   | Phát hiện rung → LED nháy nhanh liên tục           |

### Cảm biến I3G4250D – Gyroscope

- **Giao tiếp:** SPI
- **Tốc độ SPI:** Tối đa 10 MHz
- **Ngưỡng rung:** Cấu hình threshold tốc độ góc (deg/s)

### Logic phát hiện rung

```
|ω_x| + |ω_y| + |ω_z| > THRESHOLD
    │
    ├─ TRUE  → LED nháy liên tục (10 Hz)
    └─ FALSE → LED tắt
```

---

## Phần cứng & Kết nối

### Vi điều khiển

- **Board:** STM32 Discovery / Nucleo (ví dụ: STM32F4xx)

### Sơ đồ kết nối ngoại vi

| Ngoại vi          | Giao tiếp | Chân / Địa chỉ       |
|-------------------|-----------|----------------------|
| LSM303AGR (Mag)   | I2C       | SDA/SCL, addr `0x1E` |
| LSM303AGR (Accel) | I2C       | SDA/SCL, addr `0x19` |
| I3G4250D (Gyro)   | SPI       | MOSI/MISO/SCK/CS     |
| UART Debug        | UART      | TX/RX, 115200 baud   |
| User LED          | GPIO      | Xem datasheet board  |
| User Button       | GPIO      | Xem datasheet board  |

---

## Build & Flash

### Yêu cầu

- **IDE:** STM32CubeIDE hoặc Keil MDK / IAR
- **Toolchain:** ARM GCC (`arm-none-eabi-gcc`)
- **Flasher:** ST-Link v2 + STM32CubeProgrammer

### Thứ tự nạp firmware

```bash
# Bước 1: Nạp Bootloader tại địa chỉ 0x08000000
st-flash write Bootloader.bin 0x08000000

# Bước 2: Nạp App 1 tại địa chỉ 0x08004000
st-flash write App1.bin 0x08004000

# Bước 3: Nạp App 2 tại địa chỉ 0x08014000
st-flash write App2.bin 0x08014000
```

### Cập nhật Firmware qua App 2

1. Khởi động vào **App 2** (giữ User Button khi Reset)
2. Sử dụng công cụ UART terminal hoặc script Python để gửi file `.bin`
3. App 2 nhận, kiểm tra, ghi vào vùng App 1
4. Reset board → Bootloader tự động nhảy vào firmware mới

---

## Luồng hoạt động tổng thể

```
┌─────────────────────────────────────────────────┐
│                   POWER ON / RESET              │
└───────────────────────┬─────────────────────────┘
                        │
               ┌────────▼────────┐
               │   BOOTLOADER    │
               └────────┬────────┘
                        │
           ┌────────────┴────────────┐
           │ User Button held?       │
      NO   │                         │   YES
    ┌──────▼──────┐           ┌──────▼──────┐
    │    APP 1    │           │    APP 2    │
    │ (hoặc FW   │           │ La bàn số  │
    │  Update 1/2)│           │ + FW Update │
    └─────────────┘           └─────────────┘
           │                         │
    ┌──────▼──────┐           ┌──────▼──────┐
    │ Nhiệt độ   │           │  Gửi FW mới │
    │ LED on/off │           │  qua UART   │
    │ UART result│           └──────┬──────┘
    └─────────────┘                 │
                             ┌──────▼──────┐
                             │  Nạp vào   │
                             │  vùng App1  │
                             │  → Reset   │
                             └─────────────┘
```

---

## Ghi chú phát triển

- Tất cả firmware update phải có kích thước **< 64 KB** để phù hợp với vùng App 1.
- Sau mỗi lần update, CRC được kiểm tra trước khi thực hiện reset.
- Bootloader không tự xóa hoặc ghi Flash – đây là nhiệm vụ của App 2.
- Deinitialize toàn bộ ngoại vi và ngắt trước khi nhảy giữa các vùng ứng dụng.

