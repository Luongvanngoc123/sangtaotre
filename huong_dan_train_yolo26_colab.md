# Hướng dẫn train YOLO26 trên Google Colab

File dataset cần train:

```text
Find car and xe uu tien.v1-continuous-improvement-2026-05-22.yolov8.zip
```

## 1. Bật GPU trong Colab

Vào:

```text
Runtime > Change runtime type > T4 GPU
```

Sau đó chạy cell này để kiểm tra GPU:

```python
!nvidia-smi
```

Nếu Colab đang dùng Tesla T4 thì VRAM thường khoảng `15GB`. Cấu hình train bên dưới đã set theo mức VRAM này.

## 2. Mount Google Drive

```python
from google.colab import drive
drive.mount('/content/drive')
```

## 3. Tự tìm file zip trong Drive

Cell này sẽ tự tìm file zip trong toàn bộ `MyDrive`, kể cả nằm trong thư mục con.

```python
from pathlib import Path

DATASET_ZIP_NAME = "Find car and xe uu tien.v1-continuous-improvement-2026-05-22.yolov8.zip"
drive_root = Path("/content/drive/MyDrive")

matches = list(drive_root.rglob(DATASET_ZIP_NAME))

if not matches:
    raise FileNotFoundError(f"Không tìm thấy file: {DATASET_ZIP_NAME}")

zip_path = str(matches[0])
print("Đã tìm thấy:", zip_path)
```

## 4. Giải nén dataset

Lưu ý quan trọng:

```text
zip_path = đường dẫn tới file .zip trong Google Drive
out_dir  = thư mục để giải nén dataset ra
```

Không được đặt `out_dir` trỏ vào file `.zip`. Nếu đặt như dưới đây là sai:

```python
out_dir = Path(zip_path)
```

Cell đúng là:

```python
import zipfile
from pathlib import Path

out_dir = Path("/content/find_car_priority_dataset")

if out_dir.suffix == ".zip":
    raise ValueError("out_dir phải là thư mục giải nén, không được là đường dẫn file .zip")

out_dir.mkdir(parents=True, exist_ok=True)

with zipfile.ZipFile(zip_path, "r") as zip_ref:
    zip_ref.extractall(out_dir)

print("Đã giải nén vào:", out_dir)
print(list(out_dir.iterdir()))
```

## 5. Sửa lại đường dẫn trong `data.yaml`

Roboflow thường để `data.yaml` theo đường dẫn cũ. Cell này sửa lại cho đúng Colab.

```python
import yaml

data_yaml = out_dir / "data.yaml"

with open(data_yaml, "r", encoding="utf-8") as f:
    data = yaml.safe_load(f)

data["path"] = str(out_dir)

with open(data_yaml, "w", encoding="utf-8") as f:
    yaml.safe_dump(data, f, sort_keys=False, allow_unicode=True)

print(open(data_yaml, encoding="utf-8").read())
```

## 6. Cài Ultralytics

```python
!pip install -U ultralytics
```

Nếu bản mới bị lỗi không nhận `yolo26s.pt`, dùng bản đã chạy ổn trước đó:

```python
!pip install -U ultralytics==8.4.51
```

## 7. Train YOLO26 cho GPU 15GB VRAM

Cấu hình khuyến nghị cho GPU khoảng `15GB VRAM`, ví dụ Tesla T4:

- Model: `yolo26s.pt`
- Ảnh train: `768`
- Batch: `24`
- GPU: `device=0`
- Cache: `ram`

```python
from ultralytics import YOLO

model = YOLO("yolo26s.pt")

results = model.train(
    data=str(data_yaml),
    epochs=150,
    imgsz=768,
    batch=24,
    device=0,
    project="/content/runs",
    name="find_car_priority_yolo26s_768_b24",
    cache="ram",
    optimizer="MuSGD",
    amp=True,
    workers=8,
    patience=100,
    plots=True
)
```

Nếu Colab báo hết VRAM, giảm batch xuống `16` trước:

```python
results = model.train(
    data=str(data_yaml),
    epochs=150,
    imgsz=768,
    batch=16,
    device=0,
    project="/content/runs",
    name="find_car_priority_yolo26s_768_b16",
    cache="ram",
    optimizer="MuSGD",
    amp=True,
    workers=8,
    patience=100,
    plots=True
)
```

Nếu vẫn hết VRAM, giảm tiếp xuống `12`:

```python
results = model.train(
    data=str(data_yaml),
    epochs=150,
    imgsz=768,
    batch=12,
    device=0,
    project="/content/runs",
    name="find_car_priority_yolo26s_768_b12",
    cache="ram",
    optimizer="MuSGD",
    amp=True,
    workers=8,
    patience=100,
    plots=True
)
```

Nếu vẫn nặng, đổi sang YOLO26n:

```python
model = YOLO("yolo26n.pt")
```

## 8. Copy model tốt nhất về Google Drive

Nếu train bằng cấu hình `b24`:

```python
import shutil
from pathlib import Path

best_pt = Path("/content/runs/find_car_priority_yolo26s_768_b24/weights/best.pt")
save_to = "/content/drive/MyDrive/find_car_priority_yolo26s_best.pt"

if not best_pt.exists():
    raise FileNotFoundError(f"Không thấy model best.pt tại: {best_pt}")

shutil.copy(best_pt, save_to)
print("Đã lưu model vào:", save_to)
```

Nếu train bằng cấu hình `b16`, đổi đường dẫn thành:

```python
best_pt = Path("/content/runs/find_car_priority_yolo26s_768_b16/weights/best.pt")
```

Nếu train bằng cấu hình `b12`, đổi đường dẫn thành:

```python
best_pt = Path("/content/runs/find_car_priority_yolo26s_768_b12/weights/best.pt")
```

## 9. Test nhanh model sau train

```python
from ultralytics import YOLO

model = YOLO("/content/drive/MyDrive/find_car_priority_yolo26s_best.pt")
metrics = model.val(data=str(data_yaml), imgsz=768, device=0)
print(metrics)
```

## 10. Tải model về máy

Sau khi file nằm trong Drive:

```text
MyDrive/find_car_priority_yolo26s_best.pt
```

Bạn tải file đó về Windows rồi đặt vào project:

```text
Stem_repo/ai/models/find_car_priority_yolo26s_best.pt
```
