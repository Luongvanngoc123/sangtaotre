# Các trường hợp điều khiển đèn giao thông AI

Tài liệu này mô tả các tình huống vận hành chính của hệ thống đèn giao thông AI + cảm biến.

Hệ thống hiện dùng 2 cụm đèn vật lý:

- Pha A: Đường 1 + Đường 3.
- Pha B: Đường 2 + Đường 4.

Mỗi chu kỳ đầy đủ gồm các bước:

| Bước | Đường 1 + Đường 3 | Đường 2 + Đường 4 | Ý nghĩa |
| --- | --- | --- | --- |
| 1 | Xanh | Đỏ | Cho pha A chạy |
| 2 | Vàng | Đỏ | Cảnh báo pha A sắp dừng |
| 3 | Đỏ | Xanh | Cho pha B chạy |
| 4 | Đỏ | Vàng | Cảnh báo pha B sắp dừng |

Lưu ý: Trong tài liệu cũ, các bước vàng cũng được gọi là “pha”. Để tránh nhầm, tài liệu này gọi đúng hơn là `bước chuyển pha`.

## Nguyên tắc chung

- AI nhận diện xe trong từng vùng Road 1, Road 2, Road 3, Road 4.
- Arduino nhận mức xe theo dạng `LEVELS,r1,r2,r3,r4`.
- Road 1 và Road 3 gom thành pha A.
- Road 2 và Road 4 gom thành pha B.
- Nếu một pha đông hơn, pha đó được kéo dài thời gian xanh.
- Nếu cả hai pha đều có xe, hệ thống vẫn luân phiên để tránh một pha chiếm đèn liên tục.
- Nếu có xe ưu tiên, pha chứa xe ưu tiên được ưu tiên trước.
- Nếu có xe đứng yên trong vùng Block Zone, hệ thống giữ đỏ tạm thời để tránh va chạm.

## Mức lưu lượng xe

| Mức | Ý nghĩa | Thời gian xanh tham khảo |
| --- | --- | --- |
| 0 | Không có xe | Bỏ qua hoặc giữ đỏ |
| 1 | Ít xe | 5 giây |
| 2 | Vừa | 10 giây |
| 3 | Đông / ùn tắc | 15 giây hoặc kéo dài theo cấu hình |

## Trường hợp 1: Lưu lượng xe cân bằng

Hai hướng có lượng xe gần như ngang nhau.

Hệ thống chạy theo chu kỳ tiêu chuẩn, thời gian xanh của hai pha gần bằng nhau.

| Bước | Đường 1 + Đường 3 | Đường 2 + Đường 4 | Hành động |
| --- | --- | --- | --- |
| 1 | Xanh | Đỏ | Pha A chạy thời gian chuẩn |
| 2 | Vàng | Đỏ | Pha A chuẩn bị dừng |
| 3 | Đỏ | Xanh | Pha B chạy thời gian chuẩn |
| 4 | Đỏ | Vàng | Pha B chuẩn bị dừng |

## Trường hợp 2: Một hướng đông xe hơn rõ rệt

Một pha có nhiều xe hơn pha còn lại, nhưng pha còn lại vẫn có xe.

Hệ thống kéo dài thời gian xanh cho hướng đông hơn, nhưng vẫn phải luân phiên.

| Bước | Đường 1 + Đường 3 | Đường 2 + Đường 4 | Hành động |
| --- | --- | --- | --- |
| 1 | Xanh dài hơn | Đỏ | Pha A đông hơn nên xanh lâu hơn |
| 2 | Vàng | Đỏ | Pha A kết thúc |
| 3 | Đỏ | Xanh ngắn hơn | Pha B vẫn được chạy để tránh chờ quá lâu |
| 4 | Đỏ | Vàng | Pha B kết thúc |

Nếu pha B đông hơn thì đảo ngược lại.

## Trường hợp 3: Một hướng gần như không có xe

Một pha có rất ít xe hoặc không có xe.

Hệ thống giảm thời gian xanh hoặc bỏ qua pha không có xe để tránh lãng phí thời gian.

| Tình trạng | Hành động |
| --- | --- |
| Pha A có xe, pha B không có xe | Pha A xanh lâu hơn hoặc tiếp tục giữ xanh |
| Pha A không có xe, pha B có xe | Bỏ qua hoặc rút ngắn pha A, chuyển sang pha B |
| Cả hai pha đều không có xe | Giữ đỏ hoặc chuyển chế độ lưu lượng thấp |

## Trường hợp 4: Tắc đường cục bộ một hướng

Một hướng bị ùn rõ ràng, ví dụ một pha có nhiều xe liên tục.

Hệ thống kéo dài xanh để giải phóng dòng xe, nhưng không được bỏ hẳn pha còn lại nếu pha còn lại có xe.

| Tình trạng | Hành động |
| --- | --- |
| Pha A ùn tắc, pha B ít xe | Pha A xanh dài, pha B xanh ngắn |
| Pha B ùn tắc, pha A ít xe | Pha B xanh dài, pha A xanh ngắn |
| Pha ùn tắc vẫn còn xe sau thời gian xanh | Có thể gia hạn thêm nếu pha đối diện không có xe |

## Trường hợp 5: Hai hướng đều đông xe

Giờ cao điểm, cả hai pha đều có mật độ cao.

Hệ thống chia thời gian xanh ở mức trung bình hoặc cao cho cả hai pha.

| Bước | Đường 1 + Đường 3 | Đường 2 + Đường 4 | Hành động |
| --- | --- | --- | --- |
| 1 | Xanh trung bình / dài | Đỏ | Pha A chạy |
| 2 | Vàng | Đỏ | Pha A kết thúc |
| 3 | Đỏ | Xanh trung bình / dài | Pha B chạy |
| 4 | Đỏ | Vàng | Pha B kết thúc |

Điểm quan trọng: dù hai hướng đều đông, hệ thống vẫn phải luân phiên thay vì cho một hướng xanh lặp lại liên tục.

## Trường hợp 6: Xe trong giao lộ hoặc kẹt giữa ngã tư

Vùng giữa giao lộ được gọi là `Block Zone`.

Hệ thống cần phân biệt xe đang đi qua và xe bị đứng yên.

| Điều kiện trong Block Zone | Hành động |
| --- | --- |
| Không có xe | Chạy đèn bình thường |
| Có xe nhưng đang di chuyển | Không chặn pha, cho hệ thống chạy bình thường |
| Có xe đứng yên nhiều frame liên tiếp | Giữ tất cả đèn đỏ tạm thời |
| Giao lộ trống lại | Chuyển pha tiếp theo |

Lý do: nếu cứ thấy xe trong giao lộ là đỏ hết thì xe đang đi qua cũng bị hiểu nhầm là kẹt. Vì vậy chỉ nên chặn khi xe đứng yên hoặc có xung đột rõ ràng.

## Trường hợp 7: Có xe ưu tiên

Xe ưu tiên có thể là xe cứu thương, xe chữa cháy, xe công vụ hoặc dòng xe được cấu hình ưu tiên.

AI cần nhận diện class xe ưu tiên, ví dụ:

```text
xe uu tien
priority
emergency
ambulance
```

Khi phát hiện xe ưu tiên:

| Tình huống | Hành động |
| --- | --- |
| Xe ưu tiên ở Road 1 hoặc Road 3 | Ưu tiên pha A |
| Xe ưu tiên ở Road 2 hoặc Road 4 | Ưu tiên pha B |
| Xe ưu tiên đang ở pha đang xanh | Kéo dài xanh theo mức độ xe |
| Xe ưu tiên ở pha đang đỏ | Chuyển sang pha đó sau bước an toàn |
| Hết xe ưu tiên | Trở lại thuật toán mật độ bình thường |

Quy tắc an toàn:

- Không chuyển xanh đột ngột nếu pha còn lại vừa xanh, cần qua bước vàng/đỏ an toàn.
- Nếu Block Zone đang bị kẹt, xe ưu tiên cũng không được làm hệ thống mở xanh gây va chạm.
- Nếu có nhiều xe ưu tiên ở cả hai pha, hệ thống chọn pha có số xe ưu tiên nhiều hơn hoặc pha chờ lâu hơn.

## Trường hợp 8: Giao thông thấp hoặc ban đêm

Lưu lượng xe rất thấp, ví dụ ban đêm.

| Tình trạng | Hành động đề xuất |
| --- | --- |
| Không có xe ở cả hai pha | Giữ đỏ hoặc chớp vàng tùy cấu hình |
| Chỉ một pha có xe | Cho pha đó xanh ngắn |
| Có xe xuất hiện sau thời gian chờ dài | Chuyển sang pha có xe |

Ghi chú: bản code hiện tại ưu tiên chế độ an toàn `đỏ khi không có xe`. Chế độ `chớp vàng` là tùy chọn có thể bổ sung sau.

## Trường hợp 9: Mất dữ liệu cảm biến hoặc camera

Hệ thống có hai nguồn dữ liệu:

- Camera AI.
- Cảm biến siêu âm HC-SR04.

| Trạng thái | Hành động |
| --- | --- |
| Camera hoạt động, cảm biến hoạt động | Dùng AI làm chính, cảm biến kiểm chứng |
| Camera thấy xe, cảm biến không thấy | Vẫn cho AI hoạt động, gửi cảnh báo lệch dữ liệu |
| Camera không thấy xe, cảm biến thấy xe | Dùng cảm biến bổ sung demand, gửi cảnh báo |
| Camera mất tín hiệu tạm thời | Chạy bằng cảm biến |
| Cảm biến lỗi nhưng camera còn hoạt động | Chạy bằng camera, cảnh báo cảm biến lỗi |
| Mất cả camera và cảm biến | Chuyển chu kỳ mặc định hoặc giữ đỏ an toàn |

## Trường hợp 10: Cảm biến và AI mâu thuẫn

Đây là trường hợp camera và cảm biến đưa ra kết quả khác nhau.

| Camera | Cảm biến | Hành động |
| --- | --- | --- |
| Có xe | Có xe | Bình thường |
| Có xe | Không có xe | Tin camera, gửi cảnh báo |
| Không có xe | Có xe | Bổ sung demand từ cảm biến, gửi cảnh báo |
| Không có xe | Không có xe | Không có demand |

Nguyên tắc đang dùng: lấy demand theo kiểu `max(AI, cảm biến)`, tức là chỉ cần một nguồn thấy xe thì làn đó vẫn được tính có xe.

## Trường hợp 11: Lỗi vùng khoanh làn

Nếu khoanh sai vùng Road 1, Road 2, Road 3, Road 4 thì AI có thể điều khiển sai pha đèn.

Ví dụ:

- Xe nằm ở Road 3 nhưng bị khoanh nhầm vào Road 2.
- Khi đó AI sẽ gửi demand cho Road 2, làm pha B xanh thay vì pha A.

Cách xử lý:

1. Mở file chọn vùng làn.
2. Khoanh đúng Road 1, Road 2, Road 3, Road 4 theo đèn thật.
3. Lưu lại config.
4. Chạy lại AI.

Quy tắc mapping:

| Road trong GUI | Cụm đèn thật |
| --- | --- |
| Road 1 | Pha A |
| Road 2 | Pha B |
| Road 3 | Pha A |
| Road 4 | Pha B |

## Trường hợp 12: Xe đứng yên ngoài giao lộ

Xe dừng chờ ở làn đường nhưng chưa vào Block Zone.

| Điều kiện | Hành động |
| --- | --- |
| Xe đứng yên trong vùng Road | Tính là xe đang chờ, không đỏ hết |
| Xe đứng yên trong Block Zone | Coi là kẹt giao lộ, đỏ hết tạm thời |

Điểm khác nhau:

- Xe dừng trong làn là bình thường.
- Xe dừng giữa giao lộ mới là nguy hiểm.

## Tóm tắt thuật toán đề xuất

Thứ tự ưu tiên xử lý:

1. Đọc camera AI và cảm biến.
2. Kiểm tra Block Zone.
3. Nếu có xe đứng yên trong Block Zone, giữ tất cả đèn đỏ.
4. Nếu không kẹt giao lộ, kiểm tra xe ưu tiên.
5. Nếu có xe ưu tiên, ưu tiên pha chứa xe ưu tiên.
6. Nếu không có xe ưu tiên, xử lý theo mật độ xe.
7. Nếu hai pha đều có xe, luân phiên pha để tránh bỏ đói một hướng.
8. Nếu một pha không có xe, bỏ qua hoặc rút ngắn pha đó.
9. Nếu mất AI, chuyển sang cảm biến.
10. Nếu mất toàn bộ dữ liệu, dùng chu kỳ mặc định hoặc giữ đỏ an toàn.

## Các điểm đã bổ sung so với mô tả ban đầu

- Phân biệt `pha xanh chính` và `bước vàng chuyển pha`.
- Bổ sung logic xe đang di chuyển trong Block Zone không bị coi là kẹt.
- Bổ sung trường hợp xe ưu tiên và quy tắc an toàn khi chuyển pha.
- Bổ sung mất dữ liệu camera/cảm biến.
- Bổ sung mâu thuẫn giữa AI và cảm biến.
- Bổ sung lỗi khoanh vùng làn.
- Bổ sung xe đứng yên ngoài giao lộ và trong giao lộ.
- Bổ sung chế độ lưu lượng thấp/ban đêm.

