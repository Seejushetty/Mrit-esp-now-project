#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "DHT.h"

// =====================================================
// NODE IDs
// =====================================================

#define BASE_ID     1
#define HAZARD_ID   2
#define RELAY_A_ID  3
#define RELAY_B_ID  4

// =====================================================
// SENSOR PINS
// =====================================================

#define DHTPIN      27
#define DHTTYPE     DHT11

#define FLAME_AO    33
#define WATER_PIN   34

DHT dht(DHTPIN, DHTTYPE);

// =====================================================
// OLED
// =====================================================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define OLED_SDA 25
#define OLED_SCL 26

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  -1
);

// =====================================================
// MAC ADDRESSES
// =====================================================

uint8_t relayAMAC[] = {
  0x2C, 0xF4, 0x32, 0x30, 0xCD, 0xD2
};

uint8_t relayBMAC[] = {
  0x8C, 0xAA, 0xB5, 0x6B, 0x08, 0xAA
};

// =====================================================
// PACKET STRUCTURE
// MUST MATCH ALL OTHER NODES
// =====================================================

struct MeshPacket
{
  uint16_t messageID;

  uint8_t source;
  uint8_t destination;
  uint8_t type;
  uint8_t priority;
  uint8_t hopCount;
  uint8_t ttl;

  char event[20];
  char path[20];
};

// =====================================================
// TIMING
// =====================================================

const unsigned long ACK_TIMEOUT = 2500;
const unsigned long TEST_INTERVAL = 5000;
const unsigned long SENSOR_INTERVAL = 3000;

// =====================================================
// VARIABLES
// =====================================================

uint16_t messageID = 0;

uint8_t currentRelay = RELAY_A_ID;

bool waitingForACK = false;
bool ackReceived = false;

unsigned long lastSendTime = 0;
unsigned long lastSuccessfulTest = 0;

unsigned long lastSensorRead = 0;

// Latest sensor values
float temperature = 0;
float humidity = 0;

int flameValue = 0;
int waterValue = 0;

// =====================================================
// OLED
// =====================================================

void showOLED(
  String l1,
  String l2,
  String l3,
  String l4,
  String l5,
  String l6
)
{
  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.println(l1);

  display.setCursor(0, 10);
  display.println(l2);

  display.setCursor(0, 20);
  display.println(l3);

  display.setCursor(0, 30);
  display.println(l4);

  display.setCursor(0, 40);
  display.println(l5);

  display.setCursor(0, 54);
  display.println(l6);

  display.display();
}

// =====================================================
// READ SENSORS
// =====================================================

void readSensors()
{
  float newHumidity = dht.readHumidity();
  float newTemperature = dht.readTemperature();

  if (!isnan(newHumidity) && !isnan(newTemperature))
  {
    humidity = newHumidity;
    temperature = newTemperature;
  }
  else
  {
    Serial.println("DHT11 READ FAILED");
  }

  flameValue = analogRead(FLAME_AO);
  waterValue = analogRead(WATER_PIN);

  // ---------------------------------------------------
  // SERIAL OUTPUT
  // ---------------------------------------------------

  Serial.println();
  Serial.println("========================================");
  Serial.println("             SENSOR DATA");
  Serial.println("========================================");

  Serial.print("TEMPERATURE : ");
  Serial.print(temperature, 1);
  Serial.println(" C");

  Serial.print("HUMIDITY    : ");
  Serial.print(humidity, 1);
  Serial.println(" %");

  Serial.print("FLAME       : ");
  Serial.println(flameValue);

  Serial.print("WATER       : ");
  Serial.println(waterValue);

  Serial.println("========================================");

  // ---------------------------------------------------
  // OLED
  // ---------------------------------------------------

  String tempLine = "TEMP: " + String(temperature, 1) + " C";
  String humLine  = "HUM : " + String(humidity, 1) + " %";
  String flameLine = "FLAME: " + String(flameValue);
  String waterLine = "WATER: " + String(waterValue);

  showOLED(
    "HAZARD NODE 02",
    "SENSOR MONITOR",
    tempLine,
    humLine,
    flameLine,
    waterLine
  );
}

// =====================================================
// SEND SENSOR DATA
// TYPE 4 = SENSOR DATA
// =====================================================

void sendSensorData()
{
  MeshPacket packet;

  memset(
    &packet,
    0,
    sizeof(packet)
  );

  // ---------------------------------------------------
  // PACKET INFORMATION
  // ---------------------------------------------------

  packet.messageID = 0;

  packet.source = HAZARD_ID;
  packet.destination = BASE_ID;

  packet.type = 4;
  packet.priority = 2;

  packet.hopCount = 0;
  packet.ttl = 5;

  // ---------------------------------------------------
  // ENCODE SENSOR VALUES
  //
  // S274,652,4095,1234
  //
  // 274  = 27.4 C
  // 652  = 65.2 %
  // 4095 = flame value
  // 1234 = water value
  // ---------------------------------------------------

  int temp10 = (int)(temperature * 10.0);
  int hum10 = (int)(humidity * 10.0);

  snprintf(
    packet.event,
    sizeof(packet.event),
    "S%d,%d,%d,%d",
    temp10,
    hum10,
    flameValue,
    waterValue
  );

  // ---------------------------------------------------
  // SEND THROUGH CURRENT RELAY
  // ---------------------------------------------------

  uint8_t result;

  if (currentRelay == RELAY_A_ID)
  {
    strcpy(
      packet.path,
      "HAZARD->RELAY A"
    );

    result = esp_now_send(
      relayAMAC,
      (uint8_t *)&packet,
      sizeof(packet)
    );
  }
  else
  {
    strcpy(
      packet.path,
      "HAZARD->RELAY B"
    );

    result = esp_now_send(
      relayBMAC,
      (uint8_t *)&packet,
      sizeof(packet)
    );
  }

  // ---------------------------------------------------
  // SERIAL OUTPUT
  // ---------------------------------------------------

  Serial.println();
  Serial.println("========================================");
  Serial.println("       SENSOR PACKET TRANSMISSION");
  Serial.println("========================================");

  Serial.print("TYPE        : ");
  Serial.println(packet.type);

  Serial.print("DATA        : ");
  Serial.println(packet.event);

  Serial.print("CURRENT RELAY: ");

  if (currentRelay == RELAY_A_ID)
    Serial.println("RELAY A");
  else
    Serial.println("RELAY B");

  Serial.print("SEND RESULT : ");

  if (result == ESP_OK)
    Serial.println("SUCCESS");
  else
    Serial.println("FAILED");

  Serial.println("========================================");
}

// =====================================================
// SEND TEST PACKET
// =====================================================

void sendTestPacket(uint8_t relayID)
{
  MeshPacket packet;

  memset(
    &packet,
    0,
    sizeof(packet)
  );

  packet.messageID = messageID;

  packet.source = HAZARD_ID;
  packet.destination = BASE_ID;

  packet.type = 1;
  packet.priority = 0;

  packet.hopCount = 0;
  packet.ttl = 5;

  strcpy(
    packet.event,
    "FIRE"
  );

  uint8_t result;

  // ---------------------------------------------------
  // RELAY A
  // ---------------------------------------------------

  if (relayID == RELAY_A_ID)
  {
    strcpy(
      packet.path,
      "HAZARD->RELAY A"
    );

    result = esp_now_send(
      relayAMAC,
      (uint8_t *)&packet,
      sizeof(packet)
    );

    Serial.println();
    Serial.println("========================================");
    Serial.println("       NETWORK TEST");
    Serial.println("========================================");

    Serial.print("MESSAGE ID: ");
    Serial.println(messageID);

    Serial.println(
      "PATH: HAZARD -> RELAY A -> BASE"
    );
  }

  // ---------------------------------------------------
  // RELAY B
  // ---------------------------------------------------

  else
  {
    strcpy(
      packet.path,
      "HAZARD->RELAY B"
    );

    result = esp_now_send(
      relayBMAC,
      (uint8_t *)&packet,
      sizeof(packet)
    );

    Serial.println();
    Serial.println("========================================");
    Serial.println("       NETWORK TEST");
    Serial.println("========================================");

    Serial.print("MESSAGE ID: ");
    Serial.println(messageID);

    Serial.println(
      "PATH: HAZARD -> RELAY B -> BASE"
    );
  }

  Serial.print("SEND RESULT: ");

  if (result == ESP_OK)
  {
    Serial.println("SUCCESS");
  }
  else
  {
    Serial.println("FAILED");
  }

  // ---------------------------------------------------
  // OLED
  // ---------------------------------------------------

  if (relayID == RELAY_A_ID)
  {
    showOLED(
      "HAZARD NODE 02",
      "NETWORK TEST",
      "MSG ID: " + String(messageID),
      "PATH: RELAY A",
      "WAITING FOR ACK",
      "SELF HEAL: ON"
    );
  }
  else
  {
    showOLED(
      "HAZARD NODE 02",
      "NETWORK TEST",
      "MSG ID: " + String(messageID),
      "PATH: RELAY B",
      "WAITING FOR ACK",
      "SELF HEAL: ON"
    );
  }

  waitingForACK = true;
  ackReceived = false;
  lastSendTime = millis();
}

// =====================================================
// RECEIVE CALLBACK
// =====================================================

void onDataRecv(
  const uint8_t *mac,
  const uint8_t *data,
  int len
)
{
  if (len != sizeof(MeshPacket))
  {
    Serial.println(
      "WARNING: WRONG PACKET SIZE"
    );

    return;
  }

  MeshPacket packet;

  memcpy(
    &packet,
    data,
    sizeof(MeshPacket)
  );

  // ---------------------------------------------------
  // ACK
  // ---------------------------------------------------

  if (
    packet.type == 2 &&
    packet.destination == HAZARD_ID &&
    packet.messageID == messageID
  )
  {
    ackReceived = true;
    waitingForACK = false;
    lastSuccessfulTest = millis();

    Serial.println();
    Serial.println("========================================");
    Serial.println("          ACK RECEIVED");
    Serial.println("========================================");

    Serial.print("MESSAGE ID: ");
    Serial.println(packet.messageID);

    Serial.print("PATH: ");
    Serial.println(packet.path);

    Serial.print("HOPS: ");
    Serial.println(packet.hopCount);

    // -------------------------------------------------
    // DETERMINE ACTIVE RELAY
    // -------------------------------------------------

    if (
      strstr(
        packet.path,
        "RELAY A"
      ) != NULL
    )
    {
      currentRelay = RELAY_A_ID;
    }

    else if (
      strstr(
        packet.path,
        "RELAY B"
      ) != NULL
    )
    {
      currentRelay = RELAY_B_ID;
    }
  }
}

// =====================================================
// SETUP
// =====================================================

void setup()
{
  Serial.begin(115200);

  delay(1000);

  // ===================================================
  // SENSOR PINS
  // ===================================================

  pinMode(FLAME_AO, INPUT);
  pinMode(WATER_PIN, INPUT);

  dht.begin();

  Serial.println();
  Serial.println("SENSORS INITIALIZED");

  // ===================================================
  // OLED
  // ===================================================

  Wire.begin(
    OLED_SDA,
    OLED_SCL
  );

  if (
    !display.begin(
      SSD1306_SWITCHCAPVCC,
      0x3C
    )
  )
  {
    Serial.println(
      "OLED FAILED"
    );
  }

  else
  {
    Serial.println(
      "OLED OK"
    );

    showOLED(
      "HAZARD NODE 02",
      "Starting...",
      "",
      "",
      "",
      ""
    );

    delay(1000);
  }

  // ===================================================
  // WIFI
  // ===================================================

  WiFi.mode(WIFI_STA);

  delay(300);

  Serial.println();
  Serial.println("========================================");
  Serial.println("          HAZARD NODE 02");
  Serial.println("========================================");

  Serial.print("HAZARD MAC: ");
  Serial.println(
    WiFi.macAddress()
  );

  // ===================================================
  // CHANNEL
  // ===================================================

  esp_wifi_set_channel(
    1,
    WIFI_SECOND_CHAN_NONE
  );

  Serial.println(
    "WIFI CHANNEL: 1"
  );

  // ===================================================
  // ESP-NOW
  // ===================================================

  if (
    esp_now_init() != ESP_OK
  )
  {
    Serial.println(
      "ESP-NOW INIT FAILED!"
    );

    showOLED(
      "HAZARD NODE 02",
      "ESP-NOW FAILED",
      "",
      "",
      "",
      ""
    );

    while (true)
      delay(1000);
  }

  Serial.println(
    "ESP-NOW INITIALIZED"
  );

  // ===================================================
  // CALLBACK
  // ===================================================

  esp_now_register_recv_cb(
    onDataRecv
  );

  // ===================================================
  // RELAY A
  // ===================================================

  esp_now_peer_info_t peerA = {};

  memcpy(
    peerA.peer_addr,
    relayAMAC,
    6
  );

  peerA.channel = 1;
  peerA.encrypt = false;

  if (
    esp_now_add_peer(
      &peerA
    ) == ESP_OK
  )
  {
    Serial.println(
      "RELAY A PEER ADDED"
    );
  }

  else
  {
    Serial.println(
      "RELAY A PEER FAILED"
    );
  }

  // ===================================================
  // RELAY B
  // ===================================================

  esp_now_peer_info_t peerB = {};

  memcpy(
    peerB.peer_addr,
    relayBMAC,
    6
  );

  peerB.channel = 1;
  peerB.encrypt = false;

  if (
    esp_now_add_peer(
      &peerB
    ) == ESP_OK
  )
  {
    Serial.println(
      "RELAY B PEER ADDED"
    );
  }

  else
  {
    Serial.println(
      "RELAY B PEER FAILED"
    );
  }

  // ===================================================
  // READY
  // ===================================================

  Serial.println();
  Serial.println(
    "HAZARD NODE READY"
  );

  Serial.println(
    "SELF-HEALING MODE: ON"
  );

  Serial.println(
    "SENSOR MONITORING: ON"
  );

  Serial.println(
    "SENSOR TRANSMISSION: ON"
  );

  Serial.println(
    "========================================"
  );

  showOLED(
    "HAZARD NODE 02",
    "NETWORK READY",
    "SENSORS: ON",
    "RELAY A: READY",
    "RELAY B: READY",
    "SELF HEAL: ON"
  );

  delay(1000);

  // First network test
  messageID++;

  currentRelay = RELAY_A_ID;

  sendTestPacket(
    currentRelay
  );
}

// =====================================================
// LOOP
// =====================================================

void loop()
{
  // ===================================================
  // SENSOR MONITORING + TRANSMISSION
  // ===================================================

  if (
    millis() - lastSensorRead >= SENSOR_INTERVAL
  )
  {
    lastSensorRead = millis();

    readSensors();

    // Send latest sensor values to Base
    sendSensorData();
  }

  // ===================================================
  // ACK RECEIVED
  // ===================================================

  if (ackReceived)
  {
    Serial.println();
    Serial.println(
      "NETWORK TEST SUCCESS"
    );

    if (
      currentRelay == RELAY_A_ID
    )
    {
      showOLED(
        "HAZARD NODE 02",
        "NETWORK ONLINE",
        "PATH: RELAY A",
        "ACK RECEIVED!",
        "SENSORS: ON",
        "NEXT TEST: 5 SEC"
      );
    }

    else
    {
      showOLED(
        "HAZARD NODE 02",
        "NETWORK ONLINE",
        "PATH: RELAY B",
        "ACK RECEIVED!",
        "SENSORS: ON",
        "NEXT TEST: 5 SEC"
      );
    }

    ackReceived = false;

    // -------------------------------------------------
    // WAIT 5 SECONDS
    // -------------------------------------------------

    if (
      millis() - lastSuccessfulTest >= TEST_INTERVAL
    )
    {
      messageID++;

      sendTestPacket(
        currentRelay
      );
    }

    return;
  }

  // ===================================================
  // ACK TIMEOUT
  // ===================================================

  if (waitingForACK)
  {
    if (
      millis() - lastSendTime >= ACK_TIMEOUT
    )
    {
      Serial.println();
      Serial.println(
        "========================================"
      );

      Serial.println(
        "ACK TIMEOUT"
      );

      // ===============================================
      // RELAY A FAILED
      // ===============================================

      if (
        currentRelay == RELAY_A_ID
      )
      {
        Serial.println(
          "RELAY A FAILED"
        );

        Serial.println(
          "SELF HEALING..."
        );

        showOLED(
          "RELAY A FAILED",
          "SELF HEALING...",
          "SWITCHING PATH",
          "TRYING RELAY B",
          "",
          ""
        );

        delay(500);

        currentRelay = RELAY_B_ID;

        // Same message ID
        sendTestPacket(
          currentRelay
        );
      }

      // ===============================================
      // RELAY B FAILED
      // ===============================================

      else
      {
        Serial.println(
          "RELAY B FAILED"
        );

        Serial.println(
          "SELF HEALING..."
        );

        showOLED(
          "RELAY B FAILED",
          "SELF HEALING...",
          "SWITCHING PATH",
          "TRYING RELAY A",
          "",
          ""
        );

        delay(500);

        currentRelay = RELAY_A_ID;

        sendTestPacket(
          currentRelay
        );
      }

      Serial.println(
        "========================================"
      );
    }
  }

  // ===================================================
  // NEXT NETWORK TEST
  // ===================================================

  if (
    !waitingForACK &&
    !ackReceived &&
    lastSuccessfulTest > 0
  )
  {
    if (
      millis() - lastSuccessfulTest >= TEST_INTERVAL
    )
    {
      messageID++;

      sendTestPacket(
        currentRelay
      );
    }
  }

  delay(20);
}
