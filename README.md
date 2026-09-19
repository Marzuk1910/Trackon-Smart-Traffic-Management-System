# Smart Traffic Light — Vehicle-Density-Based Signal Control

Vehicle detection based smart traffic control using **ESP32 + YOLOv8 + ThingSpeak**.

The system counts vehicles from traffic images using a YOLO object-detection model
running in Google Colab, uploads the count to a private ThingSpeak channel, and an
ESP32 reads that count to dynamically set each road's green-light duration —
busier roads get more green time.

---

## How it works

```
 Traffic image ──▶ YOLOv8 (Colab) ──▶ vehicle count ──▶ ThingSpeak (private channel)
                                                                │
                                                                ▼
                                              ESP32 reads latest count over HTTPS
                                                                │
                                                                ▼
                                        Calculates green time per road, drives
                                        the 4-road red/yellow/green signal cycle
```

---

## Channel access

This channel is **private** — going to `https://thingspeak.com/channels/3236956`
**"This channel is not public"**, **Sharing → Share channel view only with the following users**
https://thingspeak.com/channels/3236956/private_show
```

| View | URL | Who can open it |
|---|---|---|
| Public attempt (blocked) | `thingspeak.com/channels/3236956` | Anyone — but shows "not public" |
| Private authenticated view | `thingspeak.com/channels/3236956/private_show` | Only the channel owner + users added under Sharing |
| Data write | `api.thingspeak.com/update` | Anyone holding the **Write API Key** |
| Data read | `api.thingspeak.com/channels/3236956/fields/1/last.txt` | Anyone holding the **Read API Key** |


---

## Project structure

```
project/
├── README.md                          ← this file
├── colab/
│   └── vehicle_counter_with_thinkspeak.py  ← YOLO detection + bounding boxes + ThingSpeak upload
├── firmware/
│   ├── traffic_controller.ino         ← ESP32 signal controller
│   └── secrets.h                      ← WiFi + ThingSpeak credentials (NOT committed to git)
└── docs/
    └── PROJECT_VALIDATION_FRAMEWORK.md ← security / reproducibility / AI & controller validation plan
```

---

## Setup

### 1. ThingSpeak channel
1. Create a channel, keep **Access: Private**.
2. Enable Field1 (total vehicle count); optionally Field2–5 for a per-class
   breakdown (car/motorcycle/bus/truck).
3. Under **Sharing**, choose *"Share channel view only with the following
   users"* and add authorized emails.
4. Under **API Keys**, note your Write API Key (for the Colab script) and Read
   API Key (for the ESP32) — keep both private, see Security below.

### 2. Colab (vehicle detection)
1. Open `colab/vehicle_counter_with_boxes.py` content in a Colab notebook.
2. `pip install ultralytics opencv-python-headless requests`
3. In Colab, click the 🔑 **Secrets** icon (left sidebar) → add
   `THINGSPEAK_WRITE_KEY` with your Write API Key → enable notebook access.
4. Run the cell, upload a traffic image when prompted — it detects vehicles,
   draws bounding boxes, and uploads the count.

### 3. ESP32 (signal controller)
1. Put `traffic_controller.ino` and `secrets.h` in the same folder (Arduino
   IDE requires this).
2. Fill in your WiFi credentials and your **rotated** Read API Key in
   `secrets.h`.
3. Wire each road's Red/Yellow/Green LEDs (or relay board, for real signal
   heads) to the GPIO pins defined at the top of the `.ino` file.
4. Flash to the ESP32. It reads the latest count every cycle over HTTPS and
   sets green time accordingly, with a safe fixed-time fallback if WiFi or
   the ThingSpeak read fails.

---

## Security

- **Never hardcode API keys or WiFi passwords** directly in `.ino` or `.py`
  files that might be shared, screenshotted, or committed. Use `secrets.h`
  (Arduino) or Colab Secrets (Python) as set up above, and add `secrets.h` to
  `.gitignore`.
- **Rotate any key that's ever been exposed** — screenshots, pasted code, or
  a public repo all count as exposure. ThingSpeak → API Keys →
  *Generate New Write API Key* / *Add New Read API Key* (then delete the old
  one).
- **Use HTTPS**, not HTTP, for both the Colab uploader and the ESP32 reader —
  this repo's scripts already do this.
- Full checklist for credential handling, reproducibility, AI-detection
  validation, controller validation, and fault/safety testing is in
  `docs/PROJECT_VALIDATION_FRAMEWORK.md`.

---

## Status

- [x] Private ThingSpeak channel with restricted Sharing
- [x] YOLOv8 vehicle detection with bounding-box visualization
- [x] ESP32 dynamic green-time controller with HTTPS + safe fallback
- [ ] Full AI detection validation (precision/recall on a labeled dataset)
- [ ] Controller comparison vs. fixed-time/actuated baselines (SUMO)
- [ ] Fault-injection test pass on bench rig

See `docs/PROJECT_VALIDATION_FRAMEWORK.md` for the plan to close out the
remaining items.
