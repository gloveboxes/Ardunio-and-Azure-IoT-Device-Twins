// Device: Arduino M0 Pro with embedded debugger
// Author: Dave Glover dglover@microsoft.com
// Date: April 2020

// Decoupled Device twin support from Azure IoT C SDK. This sample can be used with MQTT client and Azure IoT C SDK
// Simulated bash workload with local MQTT Broker and mosquitto_pub
/*
while :
do
  mosquitto_pub -h 192.168.10.1 -t "/device/control/led" -m "{\"Led1\":{\"value\":false},\"$version\":2}";
  mosquitto_pub -h 192.168.10.1 -t "/device/control/led" -m "{\"Led1\":{\"value\":true},\"$version\":2}";
done
*/

// https://cuneyt.aliustaoglu.biz/en/enabling-arduino-intellisense-with-visual-studio-code/
// https://learn.sparkfun.com/tutorials/efficient-arduino-programming-with-arduino-cli-and-visual-studio-code/all
// https://www.avrfreaks.net/forum/optimization-flags-arduino-165

#include "WiFi101.h"
#include "MQTT.h"
#include "device_twins.h"

#define NELEMS(x) (sizeof(x) / sizeof((x)[0]))

char ssid[] = "kryptonet";  // your network SSID (name)
char pass[] = "0403809914"; // your network password (use for WPA, or use as key for WEP)

int status = WL_IDLE_STATUS;
WiFiClient net;
MQTTClient client;

unsigned long lastMillis = 0;
bool state = false;
int c = 0;

#define JSON_MESSAGE_BYTES 128 // Number of bytes to allocate for the JSON telemetry message for IoT Central
static char msgBuffer[JSON_MESSAGE_BYTES] = {0};

DeviceTwinBinding LedDeviceTwinBinding;
DeviceTwinBinding deviceResetUtc;
DeviceTwinBinding *deviceTwinBindingSet[] = {&LedDeviceTwinBinding, &deviceResetUtc};

// remove compiler optimizations otherwise difficult to follow along in the debugger
// No optimization: -O0
// speed optimization: -O3
// size optimization: -Os
// balance optimization: -O2
#pragma GCC push_options
#pragma GCC optimize("O0")

void initDeviceTwins()
{
  LedDeviceTwinBinding.twinProperty = "Led1";
  LedDeviceTwinBinding.twinType = TYPE_BOOL;
  LedDeviceTwinBinding.handler = ledHandler;

  deviceResetUtc.twinProperty = "DeviceResetUTC";
  deviceResetUtc.twinType = TYPE_STRING;

  OpenDeviceTwinSet(deviceTwinBindingSet, NELEMS(deviceTwinBindingSet));
}

void ledHandler(DeviceTwinBinding *deviceTwinBinding)
{
  digitalWrite(LED_BUILTIN, *(bool *)deviceTwinBinding->twinState);
  c++;
  if (c % 100 == 0)
  {
    Serial.println(c);
  }
}

// Update Azure IoT Device Twin reported state
bool updateReportedStateCallback(DeviceTwinBinding *deviceTwinBinding, char *reportedState)
{
  // Test connection state
  if (client.connected())
  {
    // Update device twin reported state with MQTT publish
    // This will result in a MQTT message on topic $iothub/twin/res/# for deviceTwinBinding->requestId
    // Correlate the requestId with GetDeviceTwinBindingByRequestId(requestId)
    // Serial.println(reportedState);
    return true;
  }
  else
  {
    return false;
  }
}

void messageReceived(String &topic, String &payload)
{
 
  if (topic.equals("$iothub/twin/res/#")){

  }

  payload.getBytes((unsigned char *)msgBuffer, payload.length() + 1);
  DeviceTwinHandler((unsigned char *)msgBuffer, payload.length(), &updateReportedStateCallback, 99);
}

void connectMqttBroker()
{
  Serial.print("checking wifi...");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(1000);
  }

  Serial.print("\nconnecting...");
  while (!client.connect("arduino", "try", "try"))
  {
    Serial.print(".");
    delay(1000);
  }

  Serial.println("\nconnected to Mosquitto MQTT Broker!");

  client.subscribe("/device/control/led");
  client.subscribe("$iothub/twin/res/#");
}

void setup()
{
  Serial.begin(9600);
  // while (!Serial) ;

  pinMode(LED_BUILTIN, OUTPUT);

  if (WiFi.status() == WL_NO_SHIELD)
  {
    Serial.println("WiFi shield not present");
    // don't continue:
    while (true)
      ;
  }

  // attempt to connect to WiFi network:
  while (status != WL_CONNECTED)
  {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);

    status = WiFi.begin(ssid, pass);

    // wait 10 seconds for connection:
    delay(1000);
  }

  client.begin("192.168.10.1", net);
  client.onMessage(messageReceived);

  connectMqttBroker();
  initDeviceTwins();
}

void loop()
{
  client.loop();

  if (!client.connected())
  {
    connectMqttBroker();
  }

  // publish a message roughly every second.
  if (millis() - lastMillis > 10000)
  {
    client.publish("/hello", "hello, world");
    lastMillis = millis();
  }
}

#pragma GCC pop_options
