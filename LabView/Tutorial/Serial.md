# Sử dụng LAbView đọc dữ liệu qua serial COM Port

*Dưới đây là các bước chuẩn nhất để LabVIEW trên PC "nói chuyện" được với STM32 qua cổng COM:*

## Bước 1: Đảm bảo đã cài đặt NI-VISA (Rất quan trọng)
LabVIEW Community khi cài đặt mặc định có thể chưa bao gồm thư viện giao tiếp phần cứng. Bạn hãy mở ứng dụng NI Package Manager trên máy tính, tìm kiếm NI-VISA và tiến hành cài đặt. Đây là bộ driver lõi bắt buộc phải có để LabVIEW có thể đọc được cổng COM từ mạch USB-to-UART hoặc mạch nạp ST-Link.

## Bước 2: Xây dựng giao diện (Front Panel)
Trên màn hình xám (Front Panel), bạn nhấp chuột phải và tạo các công cụ điều khiển sau:

**VISA Resource Name**: (Chuột phải -> I/O -> VISA Resource). Đây là menu xổ xuống để bạn chọn đúng cổng COM của STM32 khi cắm vào máy.
**Numeric Control (Baud rate)**: Nhập giá trị baudrate khớp với STM32 (vd: 115200).
**String Indicator (Receive)**: Để hiển thị dữ liệu STM32 gửi lên.
**String Control (Transmit)**: Để bạn gõ lệnh từ PC gửi xuống STM32.
**Stop Button**: Dùng để ngắt vòng lặp an toàn.

![frontpanel](/LabView/Tutorial/img/1.png)

## Bước 3: Lập trình đồ họa (Block Diagram)
Nhấn **Ctrl + E** để chuyển sang màn hình trắng (Block Diagram) và lắp ghép các khối theo trình tự sau (chuột phải -> Instrument I/O -> Serial):

**VISA Configure Serial Port**: Đặt ở ngoài cùng, bên trái. Nối VISA Resource Name và Baud rate từ Front Panel vào khối này. Chức năng của nó là mở và thiết lập cổng COM ban đầu.

**While Loop**: (Chuột phải -> Structures -> While Loop). Vẽ một khung vòng lặp lớn. Luồng dây tín hiệu (màu tím) và dây error (màu vàng xanh) từ khối Configure sẽ đi xuyên qua viền của vòng lặp này vào bên trong.

Bên trong While Loop (Đọc/Ghi dữ liệu):

Dùng hàm **VISA Write** nếu bạn muốn gửi lệnh xuống STM32.

Dùng hàm **VISA Read** để nhận data.
*Mẹo kỹ thuật: Khối Read yêu cầu bạn phải khai báo số byte cần đọc ở chân byte count. Thay vì điền một số cứng ngắc, hãy dùng khối **VISA Bytes at Serial Port (Property Node)** đặt ngay trước khối Read. Nó sẽ kiểm tra xem bộ đệm máy tính đang có bao nhiêu byte và truyền con số đó cho khối Read, giúp bạn lấy được toàn bộ dữ liệu một cách linh hoạt.*

Thêm một khối **Wait** (ms) (khoảng 50ms - 100ms) để vòng lặp While không ngốn 100% CPU của máy tính.

**VISA Close**: Đặt khối này ở ngoài cùng, bên phải (bên ngoài vòng lặp While). Sau khi bạn nhấn nút Stop để thoát vòng lặp, luồng tín hiệu sẽ đi đến khối này để "đóng" cổng COM. Nếu thiếu khối này, lần chạy chương trình tiếp theo LabVIEW sẽ báo lỗi cổng COM đang bị chiếm dụng.

![blockdiagram](/LabView/Tutorial/img/2.png)

