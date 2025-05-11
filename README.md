# 🥯 ToastBot UI

A smart toaster user interface designed with **M5Dial** and **SquareLine Studio** for intuitive toast shade selection and feedback. The system communicates with toast shade sensors to deliver precise and consistent toasting results every time.

---

## 📦 Project Overview

ToastBot is a capstone project aimed at combining **embedded systems**, **UI design**, and **sensor-based feedback** to automate the toasting process based on user preferences. The UI allows users to select their desired toast shade, and the system adjusts toasting time accordingly using real-time color sensing.

---

## 🎯 Features

- Rotary dial control using **M5Dial encoder**
- Visual toast shade selection from light to dark
- Real-time sensor feedback from RGB color sensors
- Animated UI built in **SquareLine Studio**
- Toasting status display and completion indicator
- GPIO communication with Arduino/ESP32 controller

---

## 🧰 Technologies Used

- **M5Stack M5Dial (ESP32-S3 based)**
- **SquareLine Studio** (UI development)
- **Arduino IDE** (Microcontroller programming)
- **APDS9960** Color Sensors (I2C)
- **PCA9546 I2C Multiplexer** (multi-sensor setup)
- **Serial Communication** (between UI and sensor unit)

---

## 🗂️ File Structure

```
ToastBot-UI/
├── images/               # Toast shade icons and UI assets
├── ui_project/           # SquareLine Studio generated files
├── firmware/             # Arduino code for M5Dial rotary + display
├── README.md
```

---

## 🚀 Getting Started

1. **Clone this repo**:
   ```bash
   git clone https://github.com/yourusername/ToastBot-UI.git
   ```

2. **Open SquareLine Studio**:
   - Load the `.slp` project file from `ui_project/`

3. **Flash UI to M5Dial**:
   - Export UI code from SquareLine and upload using Arduino IDE

4. **Upload firmware**:
   - Flash the `firmware/` sketch to the M5Dial to enable rotary interaction

5. **Connect sensors and main controller**:
   - Ensure proper I2C bus connection with sensors
   - Establish serial communication with Arduino or Raspberry Pi for control logic

---

## 🔌 Dependencies

Make sure the following libraries are installed in Arduino IDE:
- `M5Dial`
- `Adafruit APDS9960`
- `Wire.h`
- `TFT_eSPI` (used by M5Stack for display)

---

## 📸 Screenshots

![UI Screenshot - Toast Selection](images/toast_ui_sample.png)

---

## 👩‍💻 Authors

- Stephanie Vo  
- McMaster University – Engineering Physics Capstone Team
