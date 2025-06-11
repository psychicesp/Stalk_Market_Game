#include <WiFi.h>
#include <PubSubClient.h> // For MQTT client functionality
#include <ArduinoJson.h>

// --- Wi-Fi Configuration ---
// Replace with your actual Wi-Fi network SSID and password
const char *WIFI_SSID = "YOUR_WIFI_SSID";
const char *WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// --- MQTT Broker Configuration ---
// Replace with the IP address or hostname of your MQTT broker
const char *MQTT_BROKER = "YOUR_MQTT_BROKER_IP_OR_HOSTNAME"; // e.g., "192.168.1.10", "broker.hivemq.com"
const int MQTT_PORT = 1883;                                  // Default MQTT port, often 1883
const char *MQTT_TOPIC = "esp32/buttonPress";                // The MQTT topic to publish messages to

// --- Button Configuration ---
// Define the GPIO pins connected to your push buttons.
// INPUT_PULLUP means the button should connect the pin to GND when pressed.
const int NUM_BUTTONS = 2;                   // Configure for more buttons
const int BUTTON_PINS[NUM_BUTTONS] = {2, 4}; // Example GPIOs for ESP32 DevKitC (Button 1, Button 2)

// --- Button State Variables for Debouncing ---
int lastButtonState[NUM_BUTTONS];                  // Stores the previous state of each button
unsigned long lastDebounceTime[NUM_BUTTONS] = {0}; // Timestamp of the last button state change
const unsigned long DEBOUNCE_DELAY_MS = 50;        // Debounce time in milliseconds to prevent false triggers


String deviceMAC;

WiFiClient espClient;

PubSubClient mqttClient(espClient);

// --- Function to Connect to Wi-Fi ---
void connectToWiFi()
{
    {
        Serial.print("Connecting to WiFi: ");
        Serial.println(WIFI_SSID);

        WiFi.disconnect(true);

        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

        int retries = 0;
        while (WiFi.status() != WL_CONNECTED)
        {
            delay(500); // milliseconds
            Serial.print(".");
            retries++;
            if (retries > 40)
                Serial.println("\nFailed to connect to WiFi after multiple retries. Re-initiating connection...");
            retries = 0;
        }
    }

    Serial.println("\nWiFi connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
}

// --- Function to Reconnect to MQTT Broker ---
void reconnectMQTT()
{
    while (!mqttClient.connected())
    {
        Serial.print("Attempting MQTT connection...");
        String clientId = "ESP32Client-" + deviceMAC.substring(deviceMAC.length() - 6);

        // Attempt to connect
        if (mqttClient.connect(clientId.c_str()))
        {
            Serial.println("connected");

        }
        else
        {
            Serial.print("failed, rc=");
            Serial.print(mqttClient.state());
            Serial.println(" try again in 5 seconds");
            delay(5000); // milliseconds
        }
    }
}

// --- Function to Publish MQTT Message ---
void publishButtonPress(int buttonId)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        connectToWiFi()
        Serial.println("WiFi not connected. Attempting to reconnect.  Message may not have been sent");
        return;
    }

    // Check MQTT connection status BEFORE attempting to publish.
    if (!mqttClient.connected())
    {
        Serial.println("MQTT client not connected. Attempting to reconnect.  Message may not have been sent");
        reconnectMQTT();
    }

    // Create a mutable JSON document.
    // 96 bytes should be enough for a payload like {"mac": "XX:XX:XX:XX:XX:XX", "buttonId": N}
    // Rounding up to the next base 2 number because why not
    StaticJsonDocument<128> doc;

    // Add the unique device ID (MAC address) to the JSON payload
    doc["mac"] = deviceMAC;
    doc["buttonId"] = buttonId;

    String payloadString;
    serializeJson(doc, payloadString);

    Serial.print("Publishing MQTT message to topic '");
    Serial.print(MQTT_TOPIC);
    Serial.print("': ");
    Serial.println(payloadString);


    if (mqttClient.publish(MQTT_TOPIC, payloadString.c_str()))
    {
        Serial.println("Message published successfully!");
    }
    else
    {
        Serial.print("Failed to publish message, MQTT state: ");
        Serial.println(mqttClient.state());
    }
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
    WiFi.mode(WIFI_STA);
    deviceMAC = WiFi.macAddress();
    Serial.print("This ESP32's MAC Address (Unique ID): ");
    Serial.println(deviceMAC);

    connectToWiFi();

    // Set MQTT server and port
    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);

    reconnectMQTT();
}

// --- Loop Function (runs repeatedly) ---
void loop()
{
    // This is the primary place to check and maintain the Wi-Fi connection.

    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("Lost WiFi connection. Attempting to reconnect...");
        connectToWiFi();
        // After re-establishing Wi-Fi, try to reconnect MQTT as well
        reconnectMQTT();
    }

    // Keep the MQTT client connected and process incoming/outgoing messages
    // This must be called frequently, preferably in your main loop.
    // Even for publish-only, this is crucial for the PubSubClient to function.
    if (!mqttClient.connected())
    {
        reconnectMQTT();
    }
    mqttClient.loop(); // Crucial for connection management and outgoing queue processing

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
                    Serial.print(i + 1);
                    Serial.println(" Pressed!");
                    publishButtonPress(i + 1);
                }
                lastButtonState[i] = reading;
            }
        }
    }

    delay(10); // Short delay to yield CPU time and prevent a tight loop
}
