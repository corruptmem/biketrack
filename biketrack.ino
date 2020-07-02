/**************************************************************
 *
 * For this example, you need to install PubSubClient library:
 *   https://github.com/knolleary/pubsubclient
 *   or from http://librarymanager/all#PubSubClient
 *
 * TinyGSM Getting Started guide:
 *   https://tiny.cc/tinygsm-readme
 *
 * For more MQTT examples, see PubSubClient library
 *
 **************************************************************
 * Use Mosquitto client tools to work with MQTT
 *   Ubuntu/Linux: sudo apt-get install mosquitto-clients
 *   Windows:      https://mosquitto.org/download/
 *
 * Subscribe for messages:
 *   mosquitto_sub -h test.mosquitto.org -t GsmClientTest/init -t GsmClientTest/ledStatus -q 1
 * Toggle led:
 *   mosquitto_pub -h test.mosquitto.org -t GsmClientTest/led -q 1 -m "toggle"
 *
 * You can use Node-RED for wiring together MQTT-enabled devices
 *   https://nodered.org/
 * Also, take a look at these additional Node-RED modules:
 *   node-red-contrib-blynk-ws
 *   node-red-dashboard
 *
 **************************************************************/



// GPRS settings
const char apn[] = "everywhere";
const char gprsUser[] = "";
const char gprsPass[] = "";

// MQTT details
const char* broker = "test.mosquitto.org";
const char* topicLed = "CameronTest/led";
const char* topicInit = "CameronTest/init";
const char* topicLedStatus = "CameronTest/ledStatus";

// Hardware
#define LED_PIN     12
#define PWR_PIN     4
#define UART_BAUD   115200
#define PIN_DTR     25
#define PIN_TX      27
#define PIN_RX      26
#define PWR_PIN     4

// Modem
#define DUMP_AT_COMMANDS
#define TINY_GSM_MODEM_SIM7000
#define SerialMon Serial
#define TINY_GSM_DEBUG SerialMon
#define SerialAT Serial1

#include <TinyGsmClient.h>
#include <PubSubClient.h>
#include <StreamDebugger.h>
#include <ArduinoJson.h>

StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm modem(debugger);
TinyGsmClient client(modem);
PubSubClient mqtt(client);

int ledStatus = LOW;
uint32_t lastReconnectAttempt = 0;

void mqttCallback(char* topic, byte* payload, unsigned int len) {
  SerialMon.print("Message arrived [");
  SerialMon.print(topic);
  SerialMon.print("]: ");
  SerialMon.write(payload, len);
  SerialMon.println();

  // Only proceed if incoming message's topic matches
  if (String(topic) == topicLed) {
    ledStatus = !ledStatus;
    digitalWrite(LED_PIN, ledStatus);
    mqtt.publish(topicLedStatus, ledStatus ? "1" : "0");
  }
}

boolean mqttConnect() {
  SerialMon.print("Connecting to ");
  SerialMon.println(broker);

  // Connect to MQTT Broker
  boolean status = mqtt.connect("CamClient1927");

  if (status == false) {
    SerialMon.println("Fail");
    return false;
  }
  
  SerialMon.println("Success");
  
  mqtt.subscribe(topicLed);
  
  return mqtt.connected();
}


void setup() {
  SerialMon.begin(115200);
  delay(10);

  pinMode(LED_PIN, OUTPUT);
  pinMode(PWR_PIN, OUTPUT);
  digitalWrite(PWR_PIN, HIGH);
  delay(300);
  digitalWrite(PWR_PIN, LOW);

  DBG("Wait 1...");
  delay(3000);

  DBG("Setting SerialAT settings");
  SerialAT.begin(UART_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);
  delay(6000);

  // Restart takes quite some time
  // To skip it, call init() instead of restart()
  DBG("Initializing modem...");
  modem.restart();

  String modemInfo = modem.getModemInfo();
  SerialMon.print("Modem Info: ");
  SerialMon.println(modemInfo);


  SerialMon.print("Waiting for network...");
  if (!modem.waitForNetwork()) {
    SerialMon.println(" fail");
    delay(10000);
    return;
  }
  SerialMon.println(" success");

  if (modem.isNetworkConnected()) {
    SerialMon.println("Network connected");
  }

    SerialMon.print(F("Connecting to "));
    SerialMon.print(apn);
    if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
      SerialMon.println(" fail");
      delay(10000);
      return;
    }
    SerialMon.println(" success");

  if (modem.isGprsConnected()) {
    SerialMon.println("GPRS connected");
  }

  // Configure GPS 
  // Set SIM7000G GPIO4 HIGH ,Open GPS power
  modem.sendAT("+SGPIO=0,4,1,1");
  modem.enableGPS();

  // MQTT Broker setup
  mqtt.setBufferSize(2048);
  mqtt.setServer(broker, 1883);
  mqtt.setCallback(mqttCallback);
}


void loop() {

  if (!mqtt.connected()) {
    SerialMon.println("=== MQTT NOT CONNECTED ===");
    // Reconnect every 10 seconds
    uint32_t t = millis();
    if (t - lastReconnectAttempt > 10000L) {
      lastReconnectAttempt = t;
      if (mqttConnect()) {
        lastReconnectAttempt = 0;
      }
    }
    
    delay(100);
  }

  // send GPS info
  float latitude  = -9999;
  float longitude = -9999;
  float speed     = 0;
  float alt       = 0;
  int   vsat      = 0;
  int   usat      = 0;
  float acc       = 0;
  int   year      = 0;
  int   month     = 0;
  int   day       = 0;
  int   hour      = 0;
  int   minute    = 0;
  int   second    = 0;


  modem.getGPS(
    &latitude, 
    &longitude, 
    &speed, 
    &alt, 
    &vsat, 
    &usat, 
    &acc, 
    &year,
    &month, 
    &day, 
    &hour, 
    &minute, 
    &second);

  StaticJsonDocument<1024> doc;
  doc["lat"] = latitude;
  doc["lon"] = longitude;
  doc["spd"] = speed;
  doc["alt"] = alt;
  doc["sat"] = vsat;
  doc["sat"] = usat;
  doc["acc"] = acc;

  char c[1024];
  int bs = serializeJson(doc, c, 1024);
  
  mqtt.publish("CameronTest/loc", (const uint8_t*) c, bs, true);

  mqtt.loop();
  
  delay(2000);
}
