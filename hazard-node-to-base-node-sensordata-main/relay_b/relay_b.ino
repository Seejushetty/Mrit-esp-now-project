#include <ESP8266WiFi.h>

extern "C"
{
  #include <user_interface.h>
}

#include <espnow.h>

#define RELAY_A_ID 3
#define WIFI_CHANNEL 1

uint8_t baseMAC[] =
{
  0xD4, 0xE9, 0xF4, 0xB2, 0xA4, 0x58
};

uint8_t hazardMAC[] =
{
  0xC0, 0xCD, 0xD6, 0x84, 0xA1, 0x04
};


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
};


// =====================================================
// RECEIVE
// =====================================================

void onDataRecv(
  uint8_t *mac,
  uint8_t *data,
  uint8_t len
)
{
  if (len != sizeof(MeshPacket))
  {
    Serial.println("INVALID PACKET SIZE");
    return;
  }

  MeshPacket packet;

  memcpy(
    &packet,
    data,
    sizeof(MeshPacket)
  );


  Serial.println();
  Serial.println("========================================");
  Serial.println("          RELAY A RECEIVED");
  Serial.println("========================================");

  Serial.print("MESSAGE ID : ");
  Serial.println(packet.messageID);

  Serial.print("SOURCE     : ");
  Serial.println(packet.source);

  Serial.print("DEST       : ");
  Serial.println(packet.destination);

  Serial.print("TYPE       : ");
  Serial.println(packet.type);

  Serial.print("EVENT      : ");
  Serial.println(packet.event);

  Serial.print("HOPS       : ");
  Serial.println(packet.hopCount);

  Serial.print("TTL        : ");
  Serial.println(packet.ttl);

  Serial.println("----------------------------------------");


  if (packet.ttl == 0)
  {
    Serial.println("TTL EXPIRED");

    Serial.println("PACKET DROPPED");

    return;
  }


  packet.hopCount++;

  packet.ttl--;


  // ===================================================
  // ACK
  // ===================================================

  if (packet.type == 2)
  {
    Serial.println("ACK FROM BASE");

    Serial.println("FORWARDING ACK TO HAZARD...");


    uint8_t result = esp_now_send(
      hazardMAC,
      (uint8_t *)&packet,
      sizeof(packet)
    );


    if (result == 0)
    {
      Serial.println("ACK FORWARDED TO HAZARD");
    }
    else
    {
      Serial.println("ACK FORWARD FAILED");
    }


    Serial.println("========================================");

    return;
  }


  // ===================================================
  // EMERGENCY
  // ===================================================

  if (
    packet.type == 1 ||
    packet.type == 3
  )
  {
    Serial.println("EMERGENCY PACKET");

    Serial.println("FORWARDING TO BASE...");


    uint8_t result = esp_now_send(
      baseMAC,
      (uint8_t *)&packet,
      sizeof(packet)
    );


    if (result == 0)
    {
      Serial.println("EMERGENCY FORWARDED TO BASE");
    }
    else
    {
      Serial.println("EMERGENCY FORWARD FAILED");
    }


    Serial.println("========================================");

    return;
  }
}


// =====================================================
// SETUP
// =====================================================

void setup()
{
  Serial.begin(115200);

  delay(1000);

  WiFi.mode(WIFI_STA);

  wifi_set_channel(
    WIFI_CHANNEL
  );

  delay(100);

  Serial.println();
  Serial.println("========================================");
  Serial.println("          RELAY A");
  Serial.println("========================================");

  Serial.print("RELAY A MAC: ");

  Serial.println(
    WiFi.macAddress()
  );

  Serial.print("WIFI CHANNEL: ");

  Serial.println(
    WIFI_CHANNEL
  );


  if (
    esp_now_init()
    != 0
  )
  {
    Serial.println("ESP-NOW INIT FAILED");

    return;
  }


  esp_now_set_self_role(
    ESP_NOW_ROLE_COMBO
  );


  // BASE
  if (
    esp_now_add_peer(
      baseMAC,
      ESP_NOW_ROLE_COMBO,
      WIFI_CHANNEL,
      NULL,
      0
    ) != 0
  )
  {
    Serial.println("BASE PEER FAILED");
  }
  else
  {
    Serial.println("BASE PEER ADDED");
  }


  // HAZARD
  if (
    esp_now_add_peer(
      hazardMAC,
      ESP_NOW_ROLE_COMBO,
      WIFI_CHANNEL,
      NULL,
      0
    ) != 0
  )
  {
    Serial.println("HAZARD PEER FAILED");
  }
  else
  {
    Serial.println("HAZARD PEER ADDED");
  }


  esp_now_register_recv_cb(
    onDataRecv
  );


  Serial.println();
  Serial.println("RELAY A READY");
  Serial.println("========================================");
}


void loop()
{
}
