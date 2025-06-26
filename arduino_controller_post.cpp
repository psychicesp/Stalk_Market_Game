// Include necessary libraries
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <UUID.h>

// --- Wi-Fi Configuration ---
// Replace with your actual Wi-Fi network SSID and password
const char *WIFI_SSID = "YOUR_WIFI_SSID";
const char *WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// --- Local Server Configuration ---
// Replace with the IP address and port of your local server
const char *SERVER_IP = "192.168.1.100";      // e.g., a Raspberry Pi or another ESP32
const int SERVER_PORT = 80;                   // e.g., 80 for HTTP, 3000 for a Node.js server
const char *SERVER_ENDPOINT = "/buttonPress"; // Single endpoint for all button presses

// --- Button Configuration ---
// Define the GPIO pins connected to your push buttons.
// INPUT_PULLUP means the button should connect the pin to GND when pressed.
// You can now define ANY number of buttons here!
const int NUM_BUTTONS = 2;                   // Configure for 2, 3, 4, or more buttons
const int BUTTON_PINS[NUM_BUTTONS] = {2, 4}; // Example GPIOs for ESP32 DevKitC (Button 1, Button 2)
// For 4 buttons: const int BUTTON_PINS[NUM_BUTTONS] = {2, 4, 16, 17};

// --- Button State Variables for Debouncing ---
int lastButtonState[NUM_BUTTONS];                  // Stores the previous state of each button
unsigned long lastDebounceTime[NUM_BUTTONS] = {0}; // Timestamp of the last button state change
const unsigned long DEBOUNCE_DELAY_MS = 25;        // Debounce time in milliseconds to prevent false triggers

// Global variable to store the unique MAC address of this ESP32 board
String deviceMAC;
UUID uuidGenerator;

// --- Function to Connect to Wi-Fi ---
void connectToWiFi()
{
    Serial.print("Connecting to WiFi: ");
    Serial.println(WIFI_SSID);

    // Stop any ongoing Wi-Fi activity before starting a new connection attempt
    WiFi.disconnect(true); // Disconnect and delete stored network configurations

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int retries = 0;
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500); // milliseconds
        Serial.print(".");
        retries++;
        if (retries > 40)
        { // Increased retries to 20 seconds (40 * 500ms) before re-attempting from scratch
            Serial.println("\nFailed to connect to WiFi after multiple retries. Re-initiating connection...");
            retries = 0; // Reset retries to continue the attempt
        }
    }

    Serial.println("\nWiFi connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
}

// --- Function to Send POST Request ---
void sendPostRequest(int buttonId)
{
    // buttonId is now the index + 1 for user-friendly IDs
    // Check Wi-Fi status BEFORE attempting to send.
    // The main loop() is responsible for re-establishing connection if lost.
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("WiFi not connected. Request will not be sent.");
        return; // Exit if Wi-Fi is not connected, the loop() will handle reconnection
    }

    HTTPClient http;
    String serverPath = "http://" + String(SERVER_IP) + ":" + String(SERVER_PORT) + String(SERVER_ENDPOINT);

    // Create a mutable JSON document.
    StaticJsonDocument<256> doc; // 38 bytes for UUID string, 18 bytes for MAC string, 4 bytes for buttonId, 24 bytes for keys, and 72 bytes for object overhead/cushion.

    doc["mac"] = deviceMAC;
    doc["buttonId"] = buttonId;
    uuidGenerator.generate();
    doc["messageId"] = uuidGenerator.toCharArray();

    String requestBody;
    serializeJson(doc, requestBody); // Convert the modified JSON document back to a string

    Serial.print("Sending POST request to: ");
    Serial.println(serverPath);
    Serial.print("Payload: ");
    Serial.println(requestBody);

    http.begin(serverPath.c_str());
    http.addHeader("Content-Type", "application/json"); // Indicate that the request body is JSON

    // Send the POST request with the modified JSON payload
    int httpResponseCode = http.POST(requestBody);

    if (httpResponseCode > 0)
    {
        Serial.printf("HTTP Response code: %d\n", httpResponseCode);
        String response = http.getString();
        Serial.println("Server Response:");
        Serial.println(response);
    }
    else
    {
        Serial.printf("HTTP Request failed, error: %s\n", http.errorToString(httpResponseCode).c_str());
    }

    http.end(); // Free resources used by the HTTP client
}

// --- Setup Function (runs once at startup) ---
void setup()
{
    Serial.begin(115200); // Initialize serial communication at 115200 baud rate for debugging output

    // Configure button pins and read their initial states
    for (int i = 0; i < NUM_BUTTONS; i++)
    {
        pinMode(BUTTON_PINS[i], INPUT_PULLUP);            // Enable internal pull-up resistor
        lastButtonState[i] = digitalRead(BUTTON_PINS[i]); // Record current button state
    }

    // Get and store the unique MAC address of the Wi-Fi station interface.
    // Setting WiFi.mode(WIFI_STA) ensures the MAC address is accessible.
    WiFi.mode(WIFI_STA);
    deviceMAC = WiFi.macAddress();
    Serial.print("This ESP32's MAC Address (Unique ID): ");
    Serial.println(deviceMAC);

    connectToWiFi(); // Establish Wi-Fi connection on startup
}

// --- Loop Function (runs repeatedly) ---
void loop()
{
    // This is the primary place to check and maintain the Wi-Fi connection.
    // If the connection is lost, attempt to reconnect.
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("Lost WiFi connection. Attempting to reconnect...");
        connectToWiFi();
    }

    // Iterate through each button to monitor its state
    for (int i = 0; i < NUM_BUTTONS; i++)
    {
        int reading = digitalRead(BUTTON_PINS[i]); // Read the current raw state of the button

        // If the button's state has changed, reset its debounce timer
        if (reading != lastButtonState[i])
        {
            lastDebounceTime[i] = millis();
        }

        // After the debounce delay, if the button's state is stable and different from the last *processed* state
        if ((millis() - lastDebounceTime[i]) > DEBOUNCE_DELAY_MS)
        {
            if (reading != lastButtonState[i])
            {
                if (reading == LOW)
                { // If button is pressed (LOW due to INPUT_PULLUP)
                    Serial.print("Button ");
                    Serial.print(i + 1); // Use 1-based indexing for button ID
                    Serial.println(" Pressed!");
                    sendPostRequest(i + 1); // Trigger the POST request for this button, passing 1-based ID
                }
                lastButtonState[i] = reading; // Update the last stable button state
            }
        }
    }

    delay(20); // Short delay to yield CPU time and prevent a tight loop.  Lower number increases polling rate and higher number saves battery. It should be less than the DEBOUNCE rate
}