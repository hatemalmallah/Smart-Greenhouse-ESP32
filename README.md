## Project Overview
This repository contains the hardware design, firmware, and IoT integration for an industrial-grade Smart Greenhouse control system. Unlike standard automated greenhouses, this project is engineered with a strong focus on **system reliability, fault tolerance, and precise actuator control**.

Powered by an **ESP32** microcontroller, the system monitors environmental parameters (Temperature, Humidity, Pressure, Gas, and Soil Moisture) via an I2C-managed **BME680** sensor. It intelligently controls 6 distinct actuators—represented by LEDs in this hardware prototype—simulating the following sub-systems: **Heating System, Cooling System, Water Pump, Grow Light, Ventilation Fan, and Humidifier**. The system manages all of these seamlessly using dual manual/automatic modes.

## Key Technical Highlights
* **Advanced Control Logic:** Implemented **Hysteresis algorithms** for stable, threshold-based actuator switching to prevent relay chattering and system instability.
* **Fault Tolerance & Safety:** Engineered a robust fail-safe mechanism. The system validates sensor data ranges, falls back to the last valid reading upon error, and executes an **Emergency Shutdown** after 5 consecutive invalid retries.
* **Signal Processing:** Utilized a Moving Average Filter (10-sample window) to clean noisy analog signals from sensors like soil moisture, preventing false triggering.
* **IoT & Remote Management:** Features bidirectional remote control via a custom **Telegram Bot API** and **Blynk** dashboard, alongside live data telemetry logged to **ThingSpeak** every 20 seconds for future Machine Learning (Predictive Maintenance) applications.
* **Custom Hardware:** Includes full PCB schematics and routing files designed specifically to handle the microcontroller, sensor bus, and actuator power management reliably.

## Hardware Components
* ESP32 Development Board (30-pin)
* BME680 Environmental Sensor
* LCD 16x2 with I2C module
* LDR Module (4-pin Photoresistor)
* 6x LED (5mm) - *Simulating Actuators*
* 6x Resistor (220 ohm)
* Buzzer (5V)
* MQ-2 Gas Sensor
* Push Button (4-pin)
