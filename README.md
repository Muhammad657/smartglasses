
### Detailed Process:

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
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

#### API Keys
const char* OPENAI_API_KEY = "sk-your-openai-api-key";
const char* DEEPGRAM_API_KEY = "your-deepgram-api-key";
