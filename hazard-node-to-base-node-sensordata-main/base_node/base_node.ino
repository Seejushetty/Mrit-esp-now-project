#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#define BASE_ID 1
#define WIFI_CHANNEL 1

// =====================================================
// RELAY MAC ADDRESSES
// =====================================================

uint8_t relayAMAC[] =
{
  0x2C, 0xF4, 0x32, 0x30, 0xCD, 0xD2
};

uint8_t relayBMAC[] =
{
  0x8C, 0xAA, 0xB5, 0x6B, 0x08, 0xAA
};

// =====================================================
// PACKET STRUCTURE
// MUST MATCH HAZARD + RELAY A + RELAY B
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
// DUPLICATE PROTECTION
// =====================================================

uint16_t lastMessageID = 0;


// =====================================================
// RECEIVE PACKET
// =====================================================

void onDataRecv(
  const uint8_t *mac,
  const uint8_t *data,
  int len
)
{
  // ===================================================
  // PACKET SIZE CHECK
  // ===================================================

  if (len != sizeof(MeshPacket))
  {
    Serial.println();
    Serial.println("INVALID PACKET SIZE");

    Serial.print("RECEIVED: ");
    Serial.println(len);

    Serial.print("EXPECTED: ");
    Serial.println(sizeof(MeshPacket));

    return;
  }

  MeshPacket packet;

  memcpy(
    &packet,
    data,
    sizeof(MeshPacket)
  );

  // ===================================================
  // GENERAL PACKET INFORMATION
  // ===================================================

  Serial.println();
  Serial.println("========================================");
  Serial.println("          PACKET AT BASE");
  Serial.println("========================================");

  Serial.print("MESSAGE ID   : ");
  Serial.println(packet.messageID);

  Serial.print("SOURCE       : ");
  Serial.println(packet.source);

  Serial.print("DESTINATION  : ");
  Serial.println(packet.destination);

  Serial.print("TYPE         : ");
  Serial.println(packet.type);

  Serial.print("PRIORITY     : P");
  Serial.println(packet.priority);

  Serial.print("EVENT        : ");
  Serial.println(packet.event);

  Serial.print("HOPS         : ");
  Serial.println(packet.hopCount);

  Serial.print("TTL          : ");
  Serial.println(packet.ttl);

  Serial.print("PATH         : ");
  Serial.println(packet.path);

  Serial.println("----------------------------------------");


  // ===================================================
  // CHECK DESTINATION
  // ===================================================

  if (
    packet.destination != BASE_ID
  )
  {
    Serial.println("PACKET NOT FOR BASE");
    Serial.println("========================================");

    return;
  }


  // ===================================================
  // SENSOR DATA
  // TYPE 4
  // ===================================================

  if (packet.type == 4)
  {
    int temp10;
    int hum10;
    int flame;
    int water;

    int valuesRead = sscanf(
      packet.event,
      "S%d,%d,%d,%d",
      &temp10,
      &hum10,
      &flame,
      &water
    );

    if (valuesRead == 4)
    {
      float temperature = temp10 / 10.0;
      float humidity = hum10 / 10.0;

      Serial.println();
      Serial.println("========================================");
      Serial.println("         SENSOR DATA RECEIVED");
      Serial.println("========================================");

      Serial.print("TEMPERATURE : ");
      Serial.print(temperature, 1);
      Serial.println(" C");

      Serial.print("HUMIDITY    : ");
      Serial.print(humidity, 1);
      Serial.println(" %");

      Serial.print("FLAME       : ");
      Serial.println(flame);

      Serial.print("WATER       : ");
      Serial.println(water);

      Serial.print("HOPS        : ");
      Serial.println(packet.hopCount);

      Serial.print("PATH        : ");
      Serial.println(packet.path);

      Serial.println("========================================");

      return;
    }

    else
    {
      Serial.println("SENSOR DATA FORMAT ERROR");
      Serial.println("========================================");

      return;
    }
  }


  // ===================================================
  // ONLY PROCESS EMERGENCY PACKETS
  // ===================================================

  if (
    packet.type != 1 &&
    packet.type != 3
  )
  {
    Serial.println("NOT AN EMERGENCY PACKET");
    Serial.println("========================================");

    return;
  }


  // ===================================================
  // DUPLICATE PROTECTION
  // ===================================================

  if (
    packet.messageID == lastMessageID
  )
  {
    Serial.println("DUPLICATE PACKET DETECTED");
    Serial.println("ALREADY PROCESSED");
    Serial.println("NO SECOND ALERT GENERATED");
    Serial.println("========================================");

    return;
  }

  lastMessageID = packet.messageID;


  // ===================================================
  // EMERGENCY
  // ===================================================

  if (packet.type == 1)
  {
    Serial.println("SOS EMERGENCY DETECTED");
  }

  if (packet.type == 3)
  {
    Serial.println("FIRE EMERGENCY DETECTED");
  }

  Serial.println("ALERT RECEIVED AT BASE");


  // ===================================================
  // CREATE ACK
  // ===================================================

  MeshPacket ack;

  memset(
    &ack,
    0,
    sizeof(ack)
  );

  ack.messageID = packet.messageID;

  ack.source = BASE_ID;

  ack.destination = packet.source;

  ack.type = 2;

  ack.priority = packet.priority;

  ack.hopCount = 0;

  ack.ttl = 5;

  strcpy(
    ack.event,
    "ACK"
  );


  // ===================================================
  // SEND ACK THROUGH RELAY A
  // ===================================================

  Serial.println();
  Serial.println("SENDING ACK THROUGH RELAY A...");

  esp_err_t resultA = esp_now_send(
    relayAMAC,
    (uint8_t *)&ack,
    sizeof(ack)
  );

  if (resultA == ESP_OK)
  {
    Serial.println("ACK SENT TO RELAY A");
  }
  else
  {
    Serial.print("RELAY A ACK FAILED: ");
    Serial.println(resultA);
  }


  // ===================================================
  // SEND ACK THROUGH RELAY B
  // ===================================================

  Serial.println("SENDING ACK THROUGH RELAY B...");

  esp_err_t resultB = esp_now_send(
    relayBMAC,
    (uint8_t *)&ack,
    sizeof(ack)
  );

  if (resultB == ESP_OK)
  {
    Serial.println("ACK SENT TO RELAY B");
  }
  else
  {
    Serial.print("RELAY B ACK FAILED: ");
    Serial.println(resultB);
  }


  Serial.println("----------------------------------------");

  Serial.println("BASE CONFIRMED ALERT");

  Serial.println("ACK PATHS ACTIVATED");

  Serial.println("========================================");
}


// =====================================================
// SETUP
// =====================================================

void setup()
{
  Serial.begin(115200);

  delay(1500);

  WiFi.mode(WIFI_STA);

  esp_wifi_set_channel(
    WIFI_CHANNEL,
    WIFI_SECOND_CHAN_NONE
  );

  delay(200);

  Serial.println();
  Serial.println("========================================");
  Serial.println("        SELF-HEALING BASE");
  Serial.println("========================================");

  Serial.print("BASE MAC: ");
  Serial.println(
    WiFi.macAddress()
  );

  Serial.print("WIFI CHANNEL: ");
  Serial.println(
    WIFI_CHANNEL
  );


  // ===================================================
  // ESP-NOW
  // ===================================================

  if (
    esp_now_init()
    != ESP_OK
  )
  {
    Serial.println("ESP-NOW INIT FAILED");

    while (true)
    {
      delay(1000);
    }
  }

  Serial.println("ESP-NOW INITIALIZED");


  // ===================================================
  // RELAY A PEER
  // ===================================================

  esp_now_peer_info_t peerA = {};

  memcpy(
    peerA.peer_addr,
    relayAMAC,
    6
  );

  peerA.channel = WIFI_CHANNEL;

  peerA.encrypt = false;


  if (
    esp_now_add_peer(&peerA)
    != ESP_OK
  )
  {
    Serial.println("RELAY A PEER FAILED");
  }
  else
  {
    Serial.println("RELAY A PEER ADDED");
  }


  // ===================================================
  // RELAY B PEER
  // ===================================================

  esp_now_peer_info_t peerB = {};

  memcpy(
    peerB.peer_addr,
    relayBMAC,
    6
  );

  peerB.channel = WIFI_CHANNEL;

  peerB.encrypt = false;


  if (
    esp_now_add_peer(&peerB)
    != ESP_OK
  )
  {
    Serial.println("RELAY B PEER FAILED");
  }
  else
  {
    Serial.println("RELAY B PEER ADDED");
  }


  // ===================================================
  // CALLBACK
  // ===================================================

  esp_now_register_recv_cb(
    onDataRecv
  );


  // ===================================================
  // READY
  // ===================================================

  Serial.println();
  Serial.println("BASE READY");
  Serial.println("SELF-HEALING NETWORK READY");
  Serial.println("SENSOR DATA SUPPORT: ON");
  Serial.println("========================================");
}


// =====================================================
// LOOP
// =====================================================

void loop()
{
}
