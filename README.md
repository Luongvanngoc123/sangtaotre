# Hệ thống đèn giao thông AI + cảm biến siêu âm

Repo này chứa sketch Arduino `project_dengt.ino` để điều khiển 2 module đèn giao thông loại `R/Y/G/GND`, đại diện cho 4 làn đường.

Hệ thống có 2 nguồn dữ liệu:

- AI gửi dữ liệu qua Serial: `LEVELS,r1,r2,r3,r4` và `BLOCKED,...`.
- Cảm biến siêu âm HC-SR04 dùng để kiểm chứng camera, đồng thời làm dự phòng khi AI mất tín hiệu quá 15 giây.

## Logic pha đèn

Chỉ dùng 2 cụm đèn vật lý:

- Cụm đèn pha 1 đại diện cho Road 1 và Road 3.
- Cụm đèn pha 2 đại diện cho Road 2 và Road 4.

Thuật toán vẫn nhận 4 mức xe:

- `r1`: mức xe Road 1
- `r2`: mức xe Road 2
- `r3`: mức xe Road 3
- `r4`: mức xe Road 4

Sau đó Arduino gom pha:

- Pha 1 lấy `max(r1, r3)`.
- Pha 2 lấy `max(r2, r4)`.

Mức xe:

- `0`: không có xe
- `1`: ít xe, xanh 5 giây
- `2`: vừa, xanh 10 giây
- `3`: đông, xanh 15 giây

Nếu block zone đang có xe của pha đối diện, Arduino giữ tất cả đèn đỏ cho đến khi an toàn.

## Logic camera + cảm biến siêu âm

Khi AI đang online, Arduino luôn đọc HC-SR04 và so sánh với dữ liệu camera:

| Camera | HC-SR04 | Hành động |
| --- | --- | --- |
| Có xe | Có xe | Bình thường, cho chạy theo AI |
| Có xe | Không có xe | Tất cả đèn đỏ và gửi cảnh báo về server |
| Không có xe | Có xe | Gửi cảnh báo về server |
| Không có xe | Không có xe | Bình thường |

Arduino gửi cảnh báo qua Serial theo dạng:

```text
ALERT,CAM_THAY_CAM_BIEN_KHONG,cameraOnlyMask,sensorOnlyMask,cameraMask,sensorMask
ALERT,CAM_KHONG_THAY_CAM_BIEN_THAY,cameraOnlyMask,sensorOnlyMask,cameraMask,sensorMask
ALERT,DU_LIEU_KHONG_KHOP,cameraOnlyMask,sensorOnlyMask,cameraMask,sensorMask
```

Ý nghĩa:

- `CAM_THAY_CAM_BIEN_KHONG`: camera thấy xe nhưng cảm biến không thấy.
- `CAM_KHONG_THAY_CAM_BIEN_THAY`: camera không thấy xe nhưng cảm biến thấy.
- `DU_LIEU_KHONG_KHOP`: dữ liệu camera và cảm biến bị lệch hỗn hợp.

Trong đó mỗi mask là 4 bit ứng với Road 1 đến Road 4.

Ví dụ `cameraOnlyMask = 1` nghĩa là Road 1 camera thấy xe nhưng cảm biến không thấy.  
Ví dụ `sensorOnlyMask = 2` nghĩa là Road 2 cảm biến thấy xe nhưng camera không thấy.

## Chân module đèn giao thông

Module đang dùng là loại `R/Y/G/GND`, thường đã có điện trở trên module nên có thể cắm thẳng vào Arduino.

| Cụm đèn | Đại diện | Chân R | Chân Y | Chân G | GND |
| --- | --- | --- | --- | --- | --- |
| Pha 1 | Road 1 + Road 3 | D2 | D3 | D4 | GND chung |
| Pha 2 | Road 2 + Road 4 | D5 | D6 | D7 | GND chung |

Không còn cắm LED riêng cho Road 3 và Road 4. Các chân `D8`, `D9`, `D10`, `D11`, `D12`, `D13` không dùng trong sketch này.

Tất cả chân `GND` của 2 module đèn nối về thanh GND trên breadboard, rồi nối thanh GND đó về chân `GND` trên Arduino Uno.

## Chân cảm biến HC-SR04

Code vẫn dùng 4 cảm biến siêu âm cho 4 Road. Mỗi cảm biến dùng 1 chân tín hiệu ở `A0-A3`.

| Hướng | VCC | GND | TRIG | ECHO |
| --- | --- | --- | --- | --- |
| Road 1 | 5V chung | GND chung | A0 | A0 qua điện trở 1k-4.7k |
| Road 2 | 5V chung | GND chung | A1 | A1 qua điện trở 1k-4.7k |
| Road 3 | 5V chung | GND chung | A2 | A2 qua điện trở 1k-4.7k |
| Road 4 | 5V chung | GND chung | A3 | A3 qua điện trở 1k-4.7k |

Mỗi cảm biến HC-SR04 dùng chung một chân tín hiệu:

- `TRIG` nối trực tiếp vào chân `A0/A1/A2/A3`.
- `ECHO` nối vào cùng chân đó nhưng nên đi qua điện trở `1k-4.7k`.

Ví dụ Road 1:

```text
HC-SR04 VCC  -> 5V
HC-SR04 GND  -> GND
HC-SR04 TRIG -> A0
HC-SR04 ECHO -> điện trở 1k-4.7k -> A0
```

Điện trở ở dây `ECHO` không phải để làm sáng LED. Nó dùng để bảo vệ chân Arduino vì `TRIG` và `ECHO` đang dùng chung một chân tín hiệu.

## Cách nối breadboard

1. Kéo `5V` từ Arduino vào thanh `+` của breadboard.
2. Kéo `GND` từ Arduino vào thanh `-` của breadboard.
3. Nếu dùng nhiều breadboard riêng, nối tất cả thanh `+` với nhau và tất cả thanh `-` với nhau.
4. Cụm đèn pha 1 nối `R/Y/G` về `D2/D3/D4`, chân `GND` về GND chung.
5. Cụm đèn pha 2 nối `R/Y/G` về `D5/D6/D7`, chân `GND` về GND chung.
6. Mỗi HC-SR04 nối `VCC` về 5V chung, `GND` về GND chung, `TRIG/ECHO` về chân `A0-A3` theo đúng Road.

## Lưu ý khi lắp thực tế

- Không dùng `D0/D1` vì 2 chân đó cần cho USB Serial với AI.
- `D8-D13` đang được bỏ trống, không cắm LED Road 3/Road 4 vào đó nữa.
- Nếu đặt cảm biến ngoài trời mưa, HC-SR04 thường không chống nước. Nên dùng `JSN-SR04T` nếu cần chống mưa tốt hơn.
- Khoảng cách phát hiện hiện tại trong code là `3-30cm`.
- Khi AI hoạt động bình thường, cảm biến siêu âm dùng để kiểm chứng camera.
- Khi AI không gửi `LEVELS` qua Serial trong 15 giây, Arduino dùng HC-SR04 làm dự phòng để chạy đèn.
- Muốn gửi cảnh báo về server, cấu hình biến môi trường `TRAFFIC_SERVER_URL` trước khi chạy file bat AI.

Ví dụ:

```powershell
$env:TRAFFIC_SERVER_URL = "http://localhost:3000/api/traffic-alert"
.\run_ai_obs_arduino.bat
```

## Kiểm tra từng cảm biến

Firmware có lệnh Serial để kiểm tra 4 cảm biến:

```text
SENSORS
```

Arduino sẽ trả về dạng:

```text
SENSORS,R1=NO_ECHO:KHONG_DOC_DUOC,R2=18cm:CO_XE,R3=55cm:OK_KHONG_XE,R4=NO_ECHO:KHONG_DOC_DUOC
```

Ý nghĩa:

- `CO_XE`: cảm biến đọc được vật trong vùng `3-30cm`.
- `OK_KHONG_XE`: cảm biến có echo nhưng vật nằm ngoài vùng nhận xe.
- `KHONG_DOC_DUOC`: không nhận được echo. Có thể do chưa có vật trước cảm biến, đấu sai `VCC/GND/TRIG/ECHO`, hoặc cảm biến chưa hoạt động.

Khi test, nên đưa tay hoặc xe mô hình cách từng HC-SR04 khoảng `5-20cm` rồi gửi lại lệnh `SENSORS`.

## Upload lên Arduino

Dùng Arduino CLI trong PowerShell:

```powershell
$tmp = "$env:TEMP\project_dengt"
New-Item -ItemType Directory -Force -Path $tmp | Out-Null
Copy-Item .\project_dengt.ino "$tmp\project_dengt.ino" -Force
arduino-cli compile --fqbn arduino:avr:uno "$tmp"
arduino-cli upload -p COM6 --fqbn arduino:avr:uno "$tmp"
```

Đổi `COM6` thành cổng COM thật của Arduino nếu máy tính nhận cổng khác.
