# Giới thiệu môi trường LabView

## Front Panel (Giao diện người dùng)
Đây là cửa sổ có nền màu xám. Môi trường này đóng vai trò như "bề mặt" của một thiết bị đo lường thực tế, hoặc chính là giao diện (UI/HMI) mà người dùng cuối sẽ thao tác.

Tại đây, bạn sẽ thiết kế giao diện bằng cách kéo thả hai loại đối tượng chính:

**Controls** (Biến đầu vào): Là các công cụ để người dùng nhập dữ liệu hoặc gửi lệnh. Ví dụ: nút nhấn (Button), công tắc (Switch), thanh trượt (Slider), ô nhập số/chữ.

*Tương đương trong C: Nó giống như các tham số truyền vào hàm (Arguments) hoặc việc bạn dùng lệnh scanf để lấy dữ liệu từ bàn phím.*
**Indicators** (Biến đầu ra): Là các công cụ để hiển thị dữ liệu ra màn hình. Ví dụ: biểu đồ (Graph), đồng hồ kim (Gauge), đèn LED báo trạng thái, ô hiển thị văn bản.

*Tương đương trong C: Giống như giá trị trả về (return) hoặc việc bạn gọi hàm printf để in kết quả.*

![frontpanel](/LabView/Tutorial/img/1.png)

## Block Diagram (Môi trường mã nguồn)
Bạn nhấn **Ctrl + E** để chuyển sang cửa sổ có nền màu trắng này. Đây chính là "trái tim" của chương trình, nơi bạn xây dựng logic hoạt động bằng cách nối dây (Wiring) thay vì gõ text.
Mọi thứ bạn tạo bên Front Panel đều sẽ tự động sinh ra một "chân cắm" (Terminal) tương ứng ở Block Diagram. Môi trường này bao gồm:
**Nodes** (Các khối hàm): Là các hàm thực thi lệnh (ví dụ: cộng trừ nhân chia, đọc file, hay khối VISA Write/Read để gửi UART mà bạn sắp làm).
**Wires** (Dây nối tín hiệu): Trong LabVIEW, dữ liệu chảy qua các dây nối với các màu sắc khác nhau quy định kiểu dữ liệu (Cam = Số thực, Xanh dương = Số nguyên, Xanh lá = Boolean, Hồng = Chuỗi ký tự).
**Structures** (Cấu trúc điều khiển): Thay vì viết chữ, bạn sẽ vẽ các khung hình chữ nhật để bao bọc các đoạn code lại. Phổ biến nhất là:
**While Loop / For Loop**: Vẽ một khung vòng lặp, mọi hàm nằm trong khung đó sẽ lặp lại.
**Case Structure**: Tương đương với lệnh if-else hoặc switch-case trong C.

*Điểm khác biệt cốt lõi (Dataflow): > Với C/C++, chương trình chạy tuần tự từ trên xuống dưới. Nhưng với LabVIEW, chương trình chạy theo luồng dữ liệu (Dataflow). Một khối hàm sẽ chỉ thực thi khi và chỉ khi nó đã nhận đầy đủ dữ liệu ở tất cả các chân đầu vào. Nếu bạn có 2 khối code không nối dây với nhau, LabVIEW sẽ tự động cho chúng chạy song song (Multithreading) một cách hoàn toàn độc lập, rất mạnh mẽ để xử lý đa luồng.*

![blockdiagram](/LabView/Tutorial/img/2.png)

## Icon & Connector Pane (Đóng gói hàm)
Ngoài 2 cửa sổ chính trên, góc trên cùng bên phải của Front Panel có một biểu tượng nhỏ. Đây là nơi bạn cấu hình chương trình hiện tại thành một SubVI.

*Tương đương trong C: Nó giống như việc bạn viết một hàm con (ví dụ: void Parse_Sensor_Data(...)) trong một file .c riêng biệt, sau đó include nó vào hàm main() để gọi ra dùng. Trong LabVIEW, bạn đóng gói một VI lại và kéo nó thả vào Block Diagram của một VI khác để tái sử dụng code cho gọn gàng.*

![blockdiagram](/LabView/Tutorial/img/3.png)

