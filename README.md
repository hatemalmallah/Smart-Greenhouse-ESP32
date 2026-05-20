* **Modular Hardware Architecture:** Structured with dedicated sub-systems for processing, environmental sensing, human-machine interface (HMI), and localized alert mechanisms.

## Hardware Components

### 1. Core Processing & Communication
* **ESP32 Development Board (30-Pin):** The central MCU handling multi-threaded sensor reading, control loops, and Wi-Fi/IoT dual-stack connectivity.

### 2. Sensor Suite (Environmental Sensing)
* **BME680 Sensor:** High-precision I2C environmental sensor measuring ambient temperature, relative humidity, barometric pressure, and VOC gas (Indoor Air Quality).
* **MQ-2 Gas Sensor:** Analog sensor specialized in detecting LPG, smoke, and flammable gases for safety monitoring.
* **LDR Module (4-Pin Photoresistor):** Features both Analog output (for precise lux tracking) and Digital output (with on-board potentiometer thresholding) for ambient light level detection.

### 3. Actuators, Indicators & HMI
* **6x LEDs (5mm):** Serving as localized visual status indicators representing the 6 independent actuator channels.
* **5V Passive Buzzer:** Utilized for audible alarms during critical system failures or emergency shutdown sequences.
* **LCD 16x2 Display (with I2C Backpack):** Local Human-Machine Interface providing real-time telemetry display while consuming only 2 pins on the I2C bus.
* **4-Pin Push Button:** Hardware interrupt trigger used to toggle between Manual and Automatic system control modes.

### 4. Passive & Discrete Components
* **6x 220Ω Resistors:** Current-limiting resistors inline with each status indicator LED to protect the GPIO pins.
