#include <WiFi.h>
#include <WebServer.h>
#include <esp_timer.h>

#define CURRENT_SENSOR_PIN 34
#define VOLTAGE_SENSOR_PIN 35

const float sensitivity = 0.100; // V/A for ACS712 20A version
const float voltageScaleFactor = 250.0 / 3.3; // for ZMPT101B
const int sampleSize = 10;  // size of currentsample va voltagesample
const float threshold = 2000; // for power in watts

float currentSamples[sampleSize] = {0};
float voltageSamples[sampleSize] = {0};
float energyConsumed = 0;

WebServer server(80);
esp_timer_handle_t timer;

// filter
float movingAverage(float *samples, int size, float newSample) {
    float sum = 0;
    for (int i = 0; i < size - 1; i++) {
        samples[i] = samples[i + 1];
        sum += samples[i];
    }
    samples[size - 1] = newSample;
    sum += newSample;
    return sum / size;
}

// Function to calculate energy
void integratePower(float power, float interval) {
    energyConsumed += power * interval / 3600.0; // Convert to Wh
}

// Timer interrupt handler
void IRAM_ATTR onTimer(void* arg) {
    int adcValueCurrent = analogRead(CURRENT_SENSOR_PIN);
    int adcValueVoltage = analogRead(VOLTAGE_SENSOR_PIN);

    float current = (adcValueCurrent - 512) * (3.3 / 1024) / sensitivity;
    float voltage = (adcValueVoltage - 512) * (3.3 / 1024) * voltageScaleFactor;

    float filteredCurrent = movingAverage(currentSamples, sampleSize, current);
    float filteredVoltage = movingAverage(voltageSamples, sampleSize, voltage);

    float power = filteredVoltage * filteredCurrent;
    integratePower(power, 1); // Integrate every second

    checkForunknown(power); // Check for unknown
}

// Function to set up the esp_timer
void setupTimer() {
    const esp_timer_create_args_t timer_args = {
        .callback = &onTimer,
        .name = "energy_timer"
    };
    esp_timer_create(&timer_args, &timer);
    esp_timer_start_periodic(timer, 1000000); // 1 second (in microseconds)
}

// Function to check for power unknown
void checkForunknown(float power) {
    if (power > threshold) {
        Serial.println("Warning: Power consumption anomaly detected!");
    }
}

// Function to handle the root web server request
void handleRoot() {
    String html = "<h1>Energy Monitoring System</h1>";
    html += "<p>Current: " + String(currentSamples[sampleSize - 1]) + " A</p>";
    html += "<p>Voltage: " + String(voltageSamples[sampleSize - 1]) + " V</p>";
    html += "<p>Power: " + String(voltageSamples[sampleSize - 1] * currentSamples[sampleSize - 1]) + " W</p>";
    html += "<p>Energy: " + String(energyConsumed) + " Wh</p>";
    server.send(200, "text/html", html);
}

// Function to set up the web server
void setupWebServer() {
    server.on("/", handleRoot);
    server.begin();
}

// Function to set up WiFi connection
void setupWiFi() {
    WiFi.begin("your_SSID", "your_PASSWORD");
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }
    Serial.println("Connected to WiFi");
}

void setup() {
    Serial.begin(115200);
    pinMode(CURRENT_SENSOR_PIN, INPUT);
    pinMode(VOLTAGE_SENSOR_PIN, INPUT);
    
    setupWiFi();
    setupWebServer();
    setupTimer();
}

void loop() {
    server.handleClient();
}
