#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h> // Biblioteka do nazwy domenowej .local

// --- 1. TWOJE DANE ---
const char* ssid = "nazwa wifi"; 
const char* password = "haslo";

#define LED_PIN    2    
#define LED_COUNT  144  
#define BRIGHTNESS 40   

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
WebServer server(80);

bool lastMeetingState = false;
String lastAvailability = "Available";
unsigned long lastHeartbeat = 0;

void colorAll(uint32_t c) {
  strip.setBrightness(BRIGHTNESS);
  for(int i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, c);
  }
  strip.show();
}

void updateLights() {
  lastAvailability.trim();
  
  if (lastMeetingState == true) {
    colorAll(strip.Color(255, 0, 0)); // Czerwony - Spotkanie
  } 
  else if (lastAvailability.indexOf("Available") >= 0) {
    colorAll(strip.Color(0, 255, 0)); // Zielony
  } 
  else if (lastAvailability.indexOf("Busy") >= 0 || lastAvailability.indexOf("DoNotDisturb") >= 0) {
    colorAll(strip.Color(255, 0, 0)); // Czerwony - Status
  } 
  else if (lastAvailability.indexOf("Away") >= 0 || lastAvailability.indexOf("BeRightBack") >= 0) {
    colorAll(strip.Color(200, 100, 0)); // Pomarańczowy
  } 
  else {
    colorAll(strip.Color(0, 0, 0)); // OFF
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.setSleep(false);

  strip.begin();
  strip.show(); 

  // Łączymy się bez statycznego adresu (DHCP)
  Serial.print("Łączenie z WiFi...");
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nPołączono!");
  Serial.print("Adres IP przydzielony przez telefon: ");
  Serial.println(WiFi.localIP());

  // Uruchamiamy mDNS, żeby można było użyć adresu http://teamslamp.local
  if (!MDNS.begin("teamslamp")) {
    Serial.println("Błąd ustawiania mDNS!");
  } else {
    Serial.println("Adres mDNS aktywny: http://teamslamp.local");
  }

  server.on("/webhook", HTTP_ANY, []() {
    if (server.hasArg("plain")) {
      String body = server.arg("plain");
      DynamicJsonDocument doc(2048);
      deserializeJson(doc, body);

      if (doc.containsKey("meetingState")) {
        lastMeetingState = doc["meetingState"]["isInMeeting"] | false;
      }
      if (doc.containsKey("presence")) {
        lastAvailability = doc["presence"]["availability"].as<String>();
      }
      updateLights(); 
    }
    server.send(200, "text/plain", "OK");
  });

  server.begin();
}

void loop() {
  server.handleClient();
  
  // mDNS potrzebuje chwili czasu procesora w pętli loop
  // MDNS.update(); // W nowszych rdzeniach ESP32 nie zawsze wymagane, ale nie zaszkodzi

  if (millis() - lastHeartbeat > 5000) {
    updateLights();
    lastHeartbeat = millis();
  }
}