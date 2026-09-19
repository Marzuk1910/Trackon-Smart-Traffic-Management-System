Smart Traffic Light — Vehicle Counter with Bounding Box Visualization
========================================================================
from ultralytics import YOLO
import requests
import time
import os
from datetime import datetime
from google.colab import files
from google.colab.patches import cv2_imshow
import cv2

# ===== THINGSPEAK CONFIG =====
THINGSPEAK_WRITE_KEY = "GZJRPQQE7AEA7Z9Z"   # <-- rotate this, then load via userdata.get()
THINGSPEAK_URL = "https://api.thingspeak.com/update"

# ===== YOLO MODEL =====
model = YOLO("yolov8n.pt")

# COCO vehicle classes: car, motorcycle, bus, truck
VEHICLE_CLASSES = {2: "car", 3: "motorcycle", 5: "bus", 7: "truck"}
CONFIDENCE_THRESHOLD = 0.4  # ignore detections below this confidence

OUTPUT_DIR = "annotated_frames"
os.makedirs(OUTPUT_DIR, exist_ok=True)


def detect_and_annotate(img_path: str):
    """Run YOLO, draw boxes, return (annotated_image, total_count, per_class_counts)."""
    results = model(img_path, verbose=False)
    detections = results[0].boxes.data.cpu().numpy()

    per_class_counts = {name: 0 for name in VEHICLE_CLASSES.values()}
    for det in detections:
        cls_id = int(det[5])
        confidence = float(det[4])
        if cls_id in VEHICLE_CLASSES and confidence >= CONFIDENCE_THRESHOLD:
            per_class_counts[VEHICLE_CLASSES[cls_id]] += 1

    total_count = sum(per_class_counts.values())

    # results[0].plot() returns a BGR numpy array with boxes/labels/confidence drawn
    annotated_img = results[0].plot()

    return annotated_img, total_count, per_class_counts


def push_to_thingspeak(total_count: int, per_class_counts: dict):
    """Push total to field1, and breakdown to fields 2-5 (enable these fields
    in your ThingSpeak channel settings if you want them stored/charted)."""
    try:
        response = requests.get(
            THINGSPEAK_URL,
            params={
                "api_key": THINGSPEAK_WRITE_KEY,
                "field1": total_count,
                "field2": per_class_counts.get("car", 0),
                "field3": per_class_counts.get("motorcycle", 0),
                "field4": per_class_counts.get("bus", 0),
                "field5": per_class_counts.get("truck", 0),
            },
            timeout=10,
        )
        return response.status_code
    except requests.RequestException as e:
        print(f"ThingSpeak upload failed: {e}")
        return None


while True:
    print("Upload traffic image")
    uploaded = files.upload()
    if not uploaded:
        print("No file uploaded, skipping cycle.")
        time.sleep(15)
        continue

    img_path = list(uploaded.keys())[0]

    annotated_img, vehicle_count, per_class_counts = detect_and_annotate(img_path)

    print("Vehicle Count:", vehicle_count)
    print("Breakdown:", per_class_counts)

    # Show the annotated image inline in the Colab notebook
    cv2_imshow(annotated_img)

    # Save it too, with a timestamp, so you're building a visual log
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    save_path = os.path.join(OUTPUT_DIR, f"frame_{timestamp}.jpg")
    cv2.imwrite(save_path, annotated_img)
    print(f"Saved annotated frame to {save_path}")

    status = push_to_thingspeak(vehicle_count, per_class_counts)
    print(f"ThingSpeak response: {status}")

    time.sleep(15)
