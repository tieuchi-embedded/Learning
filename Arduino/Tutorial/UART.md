# Quy trình cấu hình thanh ghi UART cho ATmega328P (Arduino Uno)
*Kiến trúc AVR 8-bit của Arduino Uno rất đơn giản và trực quan, cực kỳ thích hợp để học về cấu hình thanh ghi. Bộ UART trên chip này được gọi là USART0.*

Các thanh ghi cốt lõi:
- **UBRR0H** và **UBRR0L** (USART Baud Rate Registers): Cặp thanh ghi 16-bit lưu số chia tần số để thiết lập Baud rate.
- **UCSR0A** (USART Control and Status Register A): Chứa các cờ trạng thái (truyền xong, nhận xong) và cấu hình tốc độ nhân đôi (U2X0).
- **UCSR0B** (USART Control and Status Register B): Bật/tắt bộ truyền (TX), bộ nhận (RX) và các ngắt (Interrupts).
- **UCSR0C** (USART Control and Status Register C): Cấu hình định dạng khung truyền (Số data bit, Parity bit, Stop bit, chế độ Đồng bộ/Bất đồng bộ).
- **UDR0** (USART Data Register): Thanh ghi đệm chứa dữ liệu truyền hoặc nhận.

Đoạn code sau cấu hình UART0 chạy ở tốc độ 9600 bps, cấu hình chuẩn 8N1 (8 data bits, No parity, 1 stop bit) với thạch anh 16MHz:

```c
void UART_Init(unsigned int baud) {
    // 1. Tính toán giá trị UBRR cho cấu hình 9600 bps (F_CPU = 16000000)
    // Công thức: UBRR = (F_CPU / (16 * Baud)) - 1
    unsigned int ubrr_value = (F_CPU / (16UL * baud)) - 1;
    
    UBRR0H = (unsigned char)(ubrr_value >> 8); // Ghi 4 bit cao
    UBRR0L = (unsigned char)ubrr_value;        // Ghi 8 bit thấp

    // 2. Bật chức năng Truyền (TX) và Nhận (RX)
    UCSR0B = (1 << RXEN0) | (1 << TXEN0);

    // 3. Thiết lập định dạng khung truyền: 8 data bits, 1 stop bit, No Parity
    // UMSEL01 = 0, UMSEL00 = 0 (Chế độ Bất đồng bộ)
    // UPM01 = 0, UPM00 = 0 (Không dùng Parity)
    // USBS0 = 0 (1 stop bit)
    // UCSZ01 = 1, UCSZ00 = 1 (8-bit dữ liệu)
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

void UART_Transmit(unsigned char data) {
    // Chờ cho đến khi bộ đệm truyền trống (Cờ UDRE0 lên 1)
    while (!(UCSR0A & (1 << UDRE0)));
    // Ghi dữ liệu vào thanh ghi dịch để gửi đi
    UDR0 = data;
}

unsigned char UART_Receive(void) {
    // Chờ cho đến khi có dữ liệu nhận về hoàn tất (Cờ RXC0 lên 1)
    while (!(UCSR0A & (1 << RXC0)));
    // Trả về dữ liệu từ thanh ghi đệm
    return UDR0;
}
```

# Arduino Uno (Sử dụng Thư viện Arduino Core)

*Thư viện Arduino định nghĩa sẵn đối tượng Serial (đối với UART0). Bạn không cần quan tâm đến cấu hình phần cứng bên dưới, mọi thứ được tự động thiết lập trong nền.*

Mã nguồn C++ (Arduino IDE)
```cpp
void setup() {
  // Khởi tạo UART với tốc độ 115200 bps. 
  // Mặc định cấu hình của Arduino khi gọi Serial.begin(baud) là SERIAL_8N1
  Serial.begin(115200);
  
  // Chờ cổng Serial sẵn sàng (đặc biệt cần thiết với các board có USB native)
  while (!Serial) {
    ; 
  }
  
  Serial.println("UART Arduino Uno Ready!");
}

void loop() {
  // Kiểm tra xem có dữ liệu đến trong bộ đệm nhận hay không
  if (Serial.available() > 0) {
    // Đọc 1 byte dữ liệu từ bộ đệm
    char inChar = (char)Serial.read();
    
    // Gửi ngược lại byte đó (Echo)
    Serial.print("Received: ");
    Serial.println(inChar);
  }
}
```