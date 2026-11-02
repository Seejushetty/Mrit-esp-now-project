# ESP-NOW Sensor Data & Hazard Detection Mesh Network

This repository contains the firmware for a highly resilient, self-healing ESP-NOW mesh network. It is designed to read sensor data (temperature, humidity, flame, water) and provide multi-path emergency alerts via relay nodes to a central base station.

## Network Topology

The system comprises 4 distinct node roles to provide robustness through redundancy:
1. **Hazard Node (ESP32):** Reads sensor data (DHT11, Flame sensor, Water sensor) and transmits it to the Base Node. It features an OLED screen (SSD1306) to show the current network status and sensor readings. It actively monitors ACKs from the base and falls back to an alternate path if the primary route fails.
2. **Base Node (ESP32):** Receives, validates, and processes incoming mesh packets. It generates ACKs to confirm alert deliveries. It handles parsing of emergency packets (e.g. fire/SOS) and standard sensor telemetry.
3. **Relay A (ESP8266):** Intermediary node that forwards packets between the Hazard Node and the Base Node to extend range.
4. **Relay B (ESP8266):** Secondary intermediary node for redundant routing.

## Features
- **Self-Healing Routing:** The Hazard Node tests paths and dynamically switches relays if an ACK timeout occurs.
- **Sensor Telemetry (Type 4):** Reads temperature, humidity, flame, and water analog data and transmits it to the base.
- **Emergency Mode (Type 1 & 3):** Prioritized routing for fire or SOS alerts.
- **Duplicate Protection:** Base node tracks `lastMessageID` to prevent spamming multiple alerts for the same event.
- **OLED Dashboard:** The Hazard Node provides real-time local monitoring of the environment and connection status.

## Directory Structure
- `base_node/`: Firmware for the central ESP32 base station.
- `hazard_node/`: Firmware for the ESP32 hazard and sensor detection node.
- `relay_a/`: Firmware for the ESP8266 primary relay node.
- `relay_b/`: Firmware for the ESP8266 secondary relay node.

## Hardware Requirements
- **ESP32** x 2 (Base Node, Hazard Node)
- **ESP8266** x 2 (Relay A, Relay B)
- **DHT11 Sensor** (Connected to Pin 27 on Hazard Node)
- **Analog Flame Sensor** (Connected to Pin 33 on Hazard Node)
- **Analog Water Sensor** (Connected to Pin 34 on Hazard Node)
- **SSD1306 OLED Display (I2C)** (SDA: 25, SCL: 26 on Hazard Node)

## Library Dependencies
To compile these sketches, ensure you have installed the following Arduino libraries via the Library Manager:
- **Adafruit GFX Library**
- **Adafruit SSD1306**
- **DHT sensor library** (by Adafruit)

## Configuration
Before uploading, you must update the MAC addresses in the code to match your specific hardware.
1. Flash the nodes once just to read their MAC addresses from the Serial Monitor during the `setup()` phase.
2. Update the `relayAMAC` and `relayBMAC` in the `base_node` and `hazard_node` code.
3. Update the `baseMAC` and `hazardMAC` in the `relay_a` and `relay_b` code.
4. Re-upload the firmware to all devices.

> **Note**: As provided in this repository, the code for Relay B mirrors Relay A precisely to match the exact source configurations. Adjust Node ID and MAC configurations as needed for your specific deployment.

## Usage
1. Power on the Base Node.
2. Power on Relay A and Relay B.
3. Power on the Hazard Node.
4. The Hazard Node will begin sending network test packets to establish a route to the Base Node.
5. Sensor data will be broadcasted every 3 seconds (adjustable via `SENSOR_INTERVAL`).
6. View the serial output of the Base Node (115200 baud) to see the incoming telemetry and network status.

# hazard-node-to-base-node-sensordata
