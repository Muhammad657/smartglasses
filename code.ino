/*
  Jarvis Voice Assistant - ESP32-C3
  A complete voice assistant with speech-to-text and AI processing
  Features:
  - Records audio when button is held
  - Sends audio to Deepgram for transcription
  - Sends text to OpenAI for AI responses
  - Shows status and responses on 0.96" OLED display (128x64)
  - Uses SD card for temporary file storage
  
  Hardware Requirements:
  - ESP32-C3 board
  - 0.96" OLED SSD1306 (128x64) over I2C
  - Analog microphone on GPIO0
  - SD card module
  - Push button on GPIO3
*/

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <driver/adc.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <SD.h>
#include <FS.h>

#include <Wire.h>
#include <Adafruit_SSD1306.h>

// ============ HARDWARE CONFIGURATION ============

// Audio recording configuration
#define MIC_ADC_CHANNEL ADC1_CHANNEL_0  // GPIO0 - where analog microphone is connected
#define SAMPLE_RATE 16000               // 16kHz sample rate for speech recognition
#define MAX_DURATION_SEC 10             // Maximum recording duration in seconds
#define MAX_SAMPLES (SAMPLE_RATE * MAX_DURATION_SEC)  // Total samples we can record
#define SD_CS 10                        // Chip select pin for SD card

// User interface
#define BUTTON_PIN 3                    // Push button pin (active LOW when pressed)

// OLED Display configuration (0.96" 128x64 SSD1306)
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1                   // Reset pin (-1 if not used)
#define OLED_ADDR 0x3C                  // I2C address of OLED display

// ============ FILE SYSTEM ============
const char* RECORDING_FILE = "/recording.wav";  // Temporary file for recorded audio

// ============ NETWORK & API CONFIGURATION ============
const char* ssid = "smthing";                    // Your WiFi network name
const char* password = "parssowrd";              // Your WiFi password
const char* OPENAI_API_KEY = "OPENAI_APIKEY";    // Replace with your OpenAI API key
const char* DEEPGRAM_API_KEY = "DEEPGRAM_APIKEY";// Replace with your Deepgram API key

// System prompt that defines Jarvis's personality and behavior
const char* SYSTEM_PROMPT = "You are Jarvis, a helpful voice assistant. "
  "Be concise and polite. After receiving a user's message, provide a clear answer, "
  "and then produce a short summary no longer than 200 characters suitable for display on a 128x64 OLED. "
  "If asked for code or long text, provide the essential steps only.";

// ============ GLOBAL OBJECTS ============
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ============ FUNCTION DECLARATIONS ============
bool recordAudioWhileButtonHeld(const char* filePath);
String deepgramTranscribe(const char* filePath);
String askOpenAIChat(const String &prompt);
void writeWavHeader(File &file, uint32_t sampleRate, uint32_t numSamples);
String minimizeForDisplay(const String &text);
void showStatus(const char* line1, const char* line2 = nullptr);
bool initSD();

// ============ SETUP FUNCTION ============
// This runs once when the ESP32 starts up
void setup() {
  // Initialize serial communication for debugging
  Serial.begin(115200);
  delay(800);
  Serial.println("Jarvis Voice Assistant Starting...");

  // Configure button pin with internal pull-up resistor
  // Button will read LOW when pressed, HIGH when released
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Initialize OLED display
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("SSD1306 allocation failed");
    // Continue anyway - system will work without display
  } else {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    Serial.println("OLED display initialized");
  }

  // Show boot message
  showStatus("Jarvis Booting", "Please wait...");
  delay(2000);

  // ============ INITIALIZATION SEQUENCE ============
  
  // Step 1: Initialize SD card for file storage
  showStatus("Initializing", "SD Card...");
  if (!initSD()) {
    showStatus("SD Card Failed", "Check wiring");
    Serial.println("SD Card initialization failed!");
    delay(3000);
    // Continue anyway, but file operations will fail
  } else {
    showStatus("SD Card", "Ready");
    delay(1000);
  }

  // Step 2: Connect to WiFi network
  showStatus("Connecting to", "WiFi...");
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  int wifi_wait = 0;
  // Wait for WiFi connection with timeout (15 seconds max)
  while (WiFi.status() != WL_CONNECTED && wifi_wait < 30) {
    delay(500);
    wifi_wait++;
    Serial.print(".");
    
    // Show progress on display
    display.clearDisplay();
    display.setCursor(0,0);
    display.setTextSize(1);
    display.println("Connecting to WiFi");
    display.print("SSID: ");
    display.println(ssid);
    display.print("Attempt: ");
    display.print(wifi_wait);
    display.println("/30");
    display.display();
  }
  
  // Check if WiFi connected successfully
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    showStatus("WiFi Connected", WiFi.localIP().toString().c_str());
    delay(2000);
  } else {
    Serial.println("\nWiFi connection failed!");
    showStatus("WiFi Failed", "Check credentials");
    delay(3000);
    // Continue anyway, but API calls will fail
  }

  // Step 3: Final ready message
  showStatus("Jarvis", "Ready");
  Serial.println("Jarvis initialization complete!");
  Serial.println("System ready - hold button to speak");
  delay(2000);
  
  // Show main interface
  showStatus("Home", "Hold button to speak");
}

// ============ MAIN LOOP ============
// This runs continuously after setup
void loop() {
  // Check if button is pressed (active LOW)
  if (digitalRead(BUTTON_PIN) == LOW) {
    // ============ RECORDING PHASE ============
    showStatus("Recording...", "Release to stop");
    bool ok = recordAudioWhileButtonHeld(RECORDING_FILE);
    if (!ok) {
      Serial.println("Recording failed");
      showStatus("Recording failed", "Try again");
      delay(1500);
      return; // Go back to waiting for button press
    }

    // ============ PROCESSING PHASE ============
    showStatus("Processing...", "Please wait");

    // Step 1: Convert speech to text using Deepgram
    String transcript = deepgramTranscribe(RECORDING_FILE);
    Serial.println("Transcript: " + transcript);

    if (transcript.length() == 0) {
      showStatus("No transcript", "Try again");
      delay(1500);
    } else {
      // Step 2: Send transcript to OpenAI for AI processing
      String fullPrompt = String(SYSTEM_PROMPT) + "\nUser: " + transcript;
      String reply = askOpenAIChat(fullPrompt);
      Serial.println("AI reply: " + reply);

      // Step 3: Display minimized response on OLED
      String mini = minimizeForDisplay(reply);
      showStatus("Jarvis:", mini.c_str());
      
      // Keep the response displayed for reading
      delay(5000); // Show response for 5 seconds

      // Clean up temporary files
      if (SD.exists(RECORDING_FILE)) SD.remove(RECORDING_FILE);
    }

    // Short cooldown to prevent immediate re-trigger
    delay(400);
    
    // Wait for button to be fully released
    while (digitalRead(BUTTON_PIN) == LOW) delay(50);
    
    // Return to ready state
    showStatus("Ready", "Hold button to speak");
  }

  // Small delay to prevent overwhelming the processor
  delay(50);
}

// ============ HELPER FUNCTIONS ============

/**
 * Displays status messages on the OLED screen
 * @param line1 Main text (displayed in larger font)
 * @param line2 Optional secondary text (smaller font)
 */
void showStatus(const char* line1, const char* line2) {
  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextSize(2);  // Large font for main message
  display.println(line1);
  if (line2) {
    display.setTextSize(1);  // Smaller font for details
    display.println();
    display.println(line2);
  }
  display.display();
}

/**
 * Initializes the SD card for file storage
 * @return true if successful, false if failed
 */
bool initSD() {
  // Initialize SPI communication with specific pins
  SPI.begin(5, 9, 4); // SCK=5, MISO=9, MOSI=4 for ESP32-C3
  if (!SD.begin(SD_CS, SPI)) {
    Serial.println("Card mount failed");
    return false;
  }
  
  // Check what type of SD card is detected
  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return false;
  }
  
  Serial.println("SD mounted successfully");
  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC) {
    Serial.println("MMC");
  } else if (cardType == CARD_SD) {
    Serial.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }
  return true;
}

/**
 * Records audio while the button is held down
 * @param filePath Path to save the recorded WAV file
 * @return true if recording successful, false if failed
 */
bool recordAudioWhileButtonHeld(const char* filePath) {
  // Ensure SD card is ready
  if (!SD.begin(SD_CS, SPI)) {
    Serial.println("SD not ready");
    return false;
  }

  // Create WAV file for recording
  File file = SD.open(filePath, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to create recording file");
    return false;
  }

  // Write initial WAV header (will be updated later with actual size)
  writeWavHeader(file, SAMPLE_RATE, 0);

  size_t samplesWritten = 0;
  uint32_t startTime = millis();
  uint32_t nextSampleTime = micros(); // For precise timing

  // Configure ADC for microphone input
  adc1_config_width(ADC_WIDTH_BIT_12);                    // 12-bit resolution
  adc1_config_channel_atten(MIC_ADC_CHANNEL, ADC_ATTEN_DB_12); // 12dB attenuation

  // Recording loop: continues while button is held and under time limit
  while (digitalRead(BUTTON_PIN) == LOW && samplesWritten < MAX_SAMPLES) {
    // Wait for next sample time (maintains precise sample rate)
    while (micros() < nextSampleTime);
    nextSampleTime += 1000000 / SAMPLE_RATE; // Calculate next sample time

    // Read analog value from microphone (0-4095 for 12-bit ADC)
    int raw = adc1_get_raw(MIC_ADC_CHANNEL);
    // Convert to 16-bit by shifting left (0-65535)
    uint16_t sample = (uint16_t)(raw << 4);

    // Write sample to file (2 bytes per sample)
    if (file.write((uint8_t*)&sample, 2) != 2) {
      Serial.println("Write failed");
      break;
    }
    samplesWritten++;
  }

  // Update WAV header with actual file size information
  file.seek(0);
  writeWavHeader(file, SAMPLE_RATE, samplesWritten);
  file.close();

  Serial.printf("Recorded %d samples\n", samplesWritten);
  // Return true if we recorded at least 0.25 seconds of audio
  return samplesWritten > (SAMPLE_RATE/4);
}

/**
 * Sends audio file to Deepgram API for speech-to-text conversion
 * @param filePath Path to the WAV file to transcribe
 * @return Transcribed text as String, empty string if failed
 */
String deepgramTranscribe(const char* filePath) {
  // Check if audio file exists
  if (!SD.exists(filePath)) {
    Serial.println("Audio missing");
    return "";
  }

  File file = SD.open(filePath, FILE_READ);
  if (!file) {
    Serial.println("Failed to open audio file");
    return "";
  }

  // Create secure HTTPS client
  WiFiClientSecure client;
  client.setInsecure(); // Bypass certificate validation (not recommended for production)
  HTTPClient http;

  // Deepgram API endpoint
  String url = "https://api.deepgram.com/v1/listen?model=general";
  http.begin(client, url);
  http.addHeader("Authorization", String("Token ") + DEEPGRAM_API_KEY);

  // Set up multipart form data for file upload
  String boundary = "----DeepGramBoundary";
  http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);

  // Create multipart form data manually
  String head = "--" + boundary + "\r\n"
                "Content-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\n"
                "Content-Type: audio/wav\r\n\r\n";

  String tail = "\r\n--" + boundary + "--\r\n";

  // Calculate total content length
  int contentLength = head.length() + file.size() + tail.length();
  http.addHeader("Content-Length", String(contentLength));

  // Start POST request
  if (!http.POST((uint8_t*)NULL, 0)) {
    Serial.println("Failed to start POST");
    file.close();
    http.end();
    return "";
  }

  // Stream the file data to the HTTP connection
  WiFiClient* stream = http.getStreamPtr();
  stream->print(head);  // Write multipart header

  // Read file in chunks and send it
  uint8_t buf[512];
  while (file.available()) {
    size_t r = file.read(buf, sizeof(buf));
    stream->write(buf, r);
  }
  stream->print(tail);  // Write multipart footer
  file.close();

  // Get response from Deepgram
  String response = http.getString();
  http.end();

  // Parse JSON response to extract transcription
  DynamicJsonDocument doc(8192);
  DeserializationError err = deserializeJson(doc, response);
  if (err) {
    Serial.println("Deepgram JSON parse error");
    Serial.println(response);
    return "";
  }

  // Extract transcript from JSON response
  const char* transcript = nullptr;
  if (doc.containsKey("results")) {
    // Navigate through JSON structure to find transcript
    JsonVariant alt = doc["results"]["channels"][0]["alternatives"][0]["transcript"];
    if (!alt.isNull()) transcript = alt.as<const char*>();
  }

  if (transcript) return String(transcript);
  
  // Fallback: check for transcript at top level
  if (doc.containsKey("transcript")) return String((const char*)doc["transcript"]);

  return ""; // No transcript found
}

/**
 * Sends text to OpenAI Chat API and gets AI response
 * @param prompt The user's message + system prompt
 * @return AI response as String, empty string if failed
 */
String askOpenAIChat(const String &prompt) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient https;
  
  // OpenAI Chat Completions endpoint
  https.begin(client, "https://api.openai.com/v1/chat/completions");
  https.addHeader("Authorization", String("Bearer ") + OPENAI_API_KEY);
  https.addHeader("Content-Type", "application/json");

  // Build JSON request body
  DynamicJsonDocument bodyDoc(8192);
  bodyDoc["model"] = "gpt-3.5-turbo";  // Use GPT-3.5 Turbo model
  
  // Create messages array with system and user roles
  JsonArray messages = bodyDoc.createNestedArray("messages");
  
  // System message defines AI behavior
  JsonObject sys = messages.createNestedObject();
  sys["role"] = "system";
  sys["content"] = SYSTEM_PROMPT;
  
  // User message contains the actual query
  JsonObject usr = messages.createNestedObject();
  usr["role"] = "user";
  usr["content"] = prompt;

  // Convert JSON to string
  String body;
  serializeJson(bodyDoc, body);

  // Send POST request to OpenAI
  int code = https.POST(body);
  String resp = https.getString();
  https.end();

  // Process response
  if (code == 200) {
    DynamicJsonDocument doc(8192);
    if (deserializeJson(doc, resp) == DeserializationError::Ok) {
      // Extract AI response from JSON
      const char* content = doc["choices"][0]["message"]["content"];
      if (content) return String(content);
    }
  } else {
    Serial.printf("Chat HTTP error %d\n", code);
    Serial.println(resp);
  }
  return ""; // Return empty string on failure
}

/**
 * Writes WAV file header
 * @param file The file to write to
 * @param sampleRate Audio sample rate (e.g., 16000)
 * @param numSamples Number of audio samples in the file
 * 
 * WAV file format explanation:
 * - RIFF header: Identifies the file as a WAV file
 * - Format chunk: Specifies audio format (PCM, sample rate, etc.)
 * - Data chunk: Contains the actual audio samples
 */
void writeWavHeader(File &file, uint32_t sampleRate, uint32_t numSamples) {
  // Calculate file sizes
  uint32_t chunkSize = 36 + (numSamples * 2);  // 36 bytes header + audio data
  uint32_t byteRate = sampleRate * 2;          // bytes per second (16-bit = 2 bytes per sample)
  
  // WAV header structure
  uint8_t header[44] = {
    // RIFF header - identifies the file as WAV format
    'R','I','F','F',
    (uint8_t)(chunkSize & 0xff), (uint8_t)((chunkSize>>8)&0xff),
    (uint8_t)((chunkSize>>16)&0xff), (uint8_t)((chunkSize>>24)&0xff),
    'W','A','V','E',
    
    // Format chunk - describes the audio format
    'f','m','t',' ',
    16,0,0,0,        // PCM chunk size (16 bytes)
    1,0,             // PCM format (1 = uncompressed)
    1,0,             // Mono (1 channel)
    // Sample rate (little-endian)
    (uint8_t)(sampleRate & 0xff),(uint8_t)((sampleRate>>8)&0xff),
    (uint8_t)((sampleRate>>16)&0xff),(uint8_t)((sampleRate>>24)&0xff),
    // Byte rate (sample rate * bytes per sample)
    (uint8_t)(byteRate & 0xff),(uint8_t)((byteRate>>8)&0xff),
    (uint8_t)((byteRate>>16)&0xff),(uint8_t)((byteRate>>24)&0xff),
    2,0,             // Block align (bytes per sample * channels)
    16,0,            // Bits per sample (16-bit)
    
    // Data chunk - marks the start of audio data
    'd','a','t','a',
    // Data size (number of samples * bytes per sample)
    (uint8_t)((numSamples * 2) & 0xff),(uint8_t)(((numSamples * 2)>>8)&0xff),
    (uint8_t)(((numSamples * 2)>>16)&0xff),(uint8_t)(((numSamples * 2)>>24)&0xff)
  };
  file.write(header, 44);
}

/**
 * Shortens text for display on the small OLED screen
 * @param text The original text to minimize
 * @return Shortened version of the text (max ~200 characters)
 * 
 * This function:
 * - Replaces carriage returns and newlines with spaces
 * - Collapses multiple spaces into single spaces
 * - Truncates to about 200 characters if too long
 */
String minimizeForDisplay(const String &text) {
  String s = text;
  // Remove excess whitespace and newlines
  s.replace("\r", " ");
  s.replace("\n", " ");
  while (s.indexOf("  ") >= 0) s.replace("  ", " ");
  
  // Truncate if too long for OLED display
  if (s.length() > 200) s = s.substring(0, 197) + "...";
  return s;
}
