# IoT Multi-Mode Communication Platform

Design and implementation of an IoT platform supporting five communication modes: Serial (RS485), ZigBee, BLE, Wi-Fi (ESP-NOW), and 4G (LTE Cat.1).

**Author:** prophecypr
**Date:** May 2025
**Firmware:** ESP-IDF v5.3.2
**MCU:** ESP32-C3 (RISC-V, 160 MHz)

---

## 1. Overview

This project implements a multi-mode communication IoT gateway system using a "Terminal + Multi-Gateway" architecture. Three ESP32-C3 boards form a complete data acquisition and transmission system:

- **Terminal** -- collects sensor data (temperature & humidity) and transmits via five selectable communication modes
- **Gateway 1 / Gateway 2** -- receive terminal data from any of the five communication modes and forward to Alibaba Cloud IoT via MQTT. Two gateways provide redundant coverage; they are functionally identical, differentiated only by MQTT mode offset

The system supports seamless manual switching between five communication modes, with an average switching latency of 131 ms. All sensor data is uploaded to Alibaba Cloud IoT Platform via MQTT and visualized through a Node-RED web dashboard.

## 2. System Architecture

```
                          +-----------------------+
                          |    Alibaba Cloud IoT   |
                          |    (MQTT Broker)       |
                          |    Node-RED Dashboard  |
                          +-----------+-----------+
                                      ^
                                      | MQTT
                          +-----------+-----------+
                          |                      |
                 +--------+--------+    +--------+--------+
                 |    Gateway 1     |    |    Gateway 2     |
                 | (5 Modes -> MQTT)|    | (5 Modes -> MQTT)|
                 +--------+--------+    +--------+--------+
                          ^                      ^
                          |                      |
              +-----------+----------+-----------+
              |        Terminal        |
              |      (Data Source)    |
              +------+------+---------+
                     |  5 Modes  |
                     +-----------+
```

### Communication Modes

| Mode | Protocol | Interface | Use Case |
|------|----------|-----------|----------|
| Serial | RS485 (UART) | SP3485 transceiver | Wired, stable point-to-point |
| ZigBee | IEEE 802.15.4 | DL-LN33 module (UART) | Mesh networking, multi-hop |
| BLE | Bluetooth 5.0 | GATT Client/Server | Low-power, short-range |
| Wi-Fi | ESP-NOW | Built-in Wi-Fi | Low-latency P2P |
| 4G | LTE Cat.1 | SIMCom M100P (UART + AT) | Long-range, direct to cloud |

## 3. Hardware Design

### MCU: ESP32-C3-DevKitM-1-N4

| Specification | Value |
|---|---|
| Architecture | RISC-V 32-bit, single-core |
| Clock | 160 MHz |
| SRAM | 400 KB |
| ROM | 384 KB |
| Flash | up to 4 MB SPI Flash |
| Wireless | Wi-Fi 4 (802.11 b/g/n) + BLE 5.0 |
| GPIO | 22 programmable pins |
| Security | Secure Boot, Flash Encryption, HW AES/SHA/HMAC |

### Peripheral Modules

| Module | Model | Interface |
|--------|-------|-----------|
| Temperature & Humidity Sensor | DHT11 | GPIO single-wire (RMT) |
| OLED Display | I2C 0.78" (128x64) | I2C (400 kHz) |
| RGB LED | WS2812 | RMT |
| 4G Communication | SIMCom M100P (LTE Cat.1) | UART (115200 baud) |
| ZigBee | DL-LN33 | UART (protocol framing) |
| RS485 Transceiver | SP3485 | UART + auto-direction circuit |
| Voltage Regulator | SGM2212 (5V -> 3.3V) | LDO, 800 mA max |

## 4. Software Design

### FreeRTOS Task Architecture (Terminal)

| Task | Priority | Stack (words) | Function |
|------|----------|---------------|----------|
| button_task | 24 | 2048 | Button scan, debounce, mode switching |
| IOT_transmission_task | 20 | 2048 | Data transmission based on active mode |
| OLED_task | 15 | 2048 | Display refresh (mode, temp, humidity) |
| ws2812_task | 10 | 2048 | LED color indication |
| dht11_task | 5 | 2048 | 1-second interval sensor reading |

### FreeRTOS Task Architecture (Gateway)

| Task | Priority | Stack (words) | Function |
|------|----------|---------------|----------|
| ws2812_task | 20 | 2048 | LED status display |
| uart_rx_task | 15 | 4096 | UART data reception & parsing |
| OLED_task | 15 | 2048 | Display refresh |
| espnow_task | 10 | 2048 | ESP-NOW standby |

### Cloud Integration (Alibaba Cloud IoT)

- **MQTT Broker:** `iot-06z00ek538m2kbw.mqtt.iothub.aliyuncs.com:1883`
- **Authentication:** HMAC-SHA256 signed (securemode=3)
- **Device: Gateway** -- `esp32c3_4g_gateway` (mode offset +10)
- **Device: Terminal** -- `esp32c3_4g_terminal` (original mode value)
- **Topics:**
  - `/{ProductKey}/{DeviceName}/user/temperature/update`
  - `/{ProductKey}/{DeviceName}/user/humidity/update`
  - `/{ProductKey}/{DeviceName}/user/mode/update`

### Web Dashboard (Node-RED)

- Real-time sensor data display with communication mode and gateway identification
- Temperature & humidity historical trend charts
- Excel data export for offline analysis
- Mode identification: units digit = communication mode, tens digit = gateway number

## 5. Security

| Mechanism | Application | Implementation |
|-----------|-------------|----------------|
| AES-128-CCM | ESP-NOW link-layer encryption | Hardware-accelerated, 16-byte LMK |
| AES-128-CBC | Application-layer data encryption | mbedTLS software implementation |
| HMAC-SHA256 | Alibaba Cloud IoT device authentication | DeviceSecret-based MQTT sign |

## 6. License

This project is developed for academic purposes (undergraduate graduation thesis). All hardware designs and firmware code are the original work of the author.
