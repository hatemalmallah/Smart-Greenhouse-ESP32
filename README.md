## Project Overview
This repository contains the hardware design, firmware, and IoT integration for an industrial-grade Smart Greenhouse control system. Unlike standard automated greenhouses, this project is engineered with a strong focus on **system reliability, fault tolerance, and precise actuator control**.

Powered by a 30-pin **ESP32 Development Board**, the system monitors a comprehensive array of environmental parameters. It utilizes an I2C-managed **BME680** (for temperature, humidity, pressure, and gas), an **MQ-2 sensor** (for safety monitoring), and a 4-pin **LDR Module** (for ambient light detection). Local user interaction is facilitated by an **I2C 16x2 LCD** and a 4-pin **push button** to toggle between manual and automatic modes. The system intelligently manages 6 different actuators—currently simulated using **six 5mm LEDs** paired with **220Ω resistors**—and employs a **5V buzzer** for critical audible alarms during fail-safe events.

## Key Technical Highlights
* **Advanced Control Logic:** Implemented **Hysteresis algorithms** for stable, threshold-based actuator switching to prevent relay chattering and system instability.
* **Fault Tolerance & Safety:** Engineered a robust fail-safe mechanism. The system validates sensor data ranges, falls back to the last valid reading upon error, and executes an **Emergency Shutdown** after 5 consecutive invalid retries.
* **Signal Processing:** Utilized a Moving Average Filter (10-sample window) to clean noisy analog signals from sensors like soil moisture, preventing false triggering.
* **IoT & Remote Management:** Features bidirectional remote control via a custom **Telegram Bot API** and **Blynk** dashboard, alongside live data telemetry logged to **ThingSpeak** every 20 seconds for future Machine Learning (Predictive Maintenance) applications.
* **Custom Hardware:** Includes full PCB schematics and routing files designed specifically to handle the microcontroller, sensor bus, and actuator power management reliably.
