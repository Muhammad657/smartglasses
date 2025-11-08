# 🎙️ Jarvis Voice Assistant - ESP32 Dev Module

A standalone voice assistant built on ESP32 Dev mod. that listens to your voice commands and displays AI responses on an OLED screen. No audio output - just pure visual interaction!

## 🌟 Features

- **Voice Activation**: Hold button to record voice commands
- **Speech-to-Text**: Converts speech to text using Deepgram API
- **AI Processing**: Gets intelligent responses from OpenAI GPT
- **Visual Interface**: Shows status and responses on 0.96" OLED display (128x64)
- **Standalone Operation**: Uses SD card for temporary storage
- **Wireless Connectivity**: Connects to WiFi for cloud processing

## 🛠️ Hardware Requirements

- ESP32 Dev Module (the big one)
- 0.96" OLED SSD1306 display (128x64, I2C)
- Electret microphone breakout board (analog output)
- SD card module
- Push button
- Breadboard and jumper wires

## 📋 Software Requirements

- Arduino IDE with ESP32 support
- APIs:
  - OpenAI API key (for GPT responses)
  - Deepgram API key (for speech-to-text)

## 🔌 Wiring Diagram - ESP32 Dev Module

| ESP32 Pin | Component | Connection |
|-----------|-----------|------------|
| GPIO36 | Microphone | Analog Out (AUDIO) |
| GPIO4 | Push Button | One side (other to GND) |
| GPIO23 | SD Card | MOSI |
| GPIO18 | SD Card | SCK |
| GPIO19 | SD Card | MISO |
| GPIO5 | SD Card | CS |
| GPIO21 | OLED Display | SDA |
| GPIO22 | OLED Display | SCL |
| 3.3V | All components | VCC |
| GND | All components | GND |

## 🚀 How It Works

### System Flow:


1. **Initialization**: 
   - Boots up and shows "Jarvis Booting"
   - Initializes SD card
   - Connects to WiFi
   - Displays "Jarvis Ready"

2. **Recording**:
   - Hold button to start recording
   - Records audio to SD card as WAV file
   - Release button to stop

3. **Processing**:
   - Sends audio to Deepgram for speech-to-text
   - Sends text to OpenAI for AI response
   - Processes and minimizes response for display

4. **Display**:
   - Shows AI response on OLED screen
   - Returns to ready state

## ⚙️ Setup Instructions

### 1. Hardware Setup
- Wire components according to the pin table above
- Ensure stable 3.3V power supply
- Use proper grounding
- Electret mic: VCC→3.3V, GND→GND, AUDIO→GPIO36

### 2. Software Setup
**Install these libraries in Arduino IDE:**
- ArduinoJson by Benoit Blanchon
- Adafruit SSD1306
- Adafruit GFX Library

**Select Board:**
- Go to Tools → Board → ESP32 Arduino → "ESP32 Dev Module"

### 3. API Configuration
**Get your API keys:**
1. [OpenAI API](https://platform.openai.com/) - for GPT responses
2. [Deepgram API](https://deepgram.com/) - for speech-to-text

**Update the code with your credentials:**

#### WiFi Configuration
```const char* ssid = "YOUR_WIFI_SSID";```
```const char* password = "YOUR_WIFI_PASSWORD";```

#### API Keys
``` const char* OPENAI_API_KEY = "your-openai-api-key";```
```** const char* DEEPGRAM_API_KEY = "your-deepgram-api-key"; ```
