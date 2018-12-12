#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>

ESP8266WebServer server (80);

WiFiEventHandler DisConnectHandler;
WiFiEventHandler GotIpHandler;

#define LED 2

unsigned long    previousMillis;

const char* APssid     = "ESPap";
const char* APpassword = "0123456789";

bool isConnecte   = false;  // Dice si se pueden enviar datos al servidor o no
bool wasFound     = false;  // Dice si debe seguir intentando conectar al wifi luego de no poder hacerlo
bool isBroken     = false;  // Rompe la ejecución del hilo de conexión de modo cliente
char ssid[30];
char pass[30];

void onDisconnectHandler(const WiFiEventStationModeDisconnected& event){
  if (isConnecte == true) {
    Serial.println ( "WiFi disconnect." );
    isBroken = true;
    isConnecte = false;
  }
}
void onGotIPHandler(const WiFiEventStationModeGotIP& event){
  Serial.print ("Got Ip: ");
  Serial.println(WiFi.localIP());
  isConnecte = true;
}

void init_configs() {
  WiFi.persistent(false);
  pinMode (LED, OUTPUT);
  digitalWrite (LED, HIGH);
  Serial.begin (115200);
  EEPROM.begin(100);

  GotIpHandler       = WiFi.onStationModeGotIP(&onGotIPHandler);
  DisConnectHandler  = WiFi.onStationModeDisconnected(&onDisconnectHandler);

  server.on("/"      , HTTP_POST, handleRoot);
  server.on("/config", HTTP_POST, save_config);
  server.on("/apply" , HTTP_POST, apply_config);
  server.onNotFound(handleNotFound);
  server.begin();

  clientSetup();
}

bool findWifi () {
  bool F = false; // Flag
  Serial.println("");
  Serial.println("scan start");
  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  if (n == 0) {
    Serial.println("no networks found");
    wasFound = false;
    F = false;
  } else {
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i) {
      if (String(ssid) == WiFi.SSID(i)) {
        F = true;
      }
      /*
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.println(WiFi.SSID(i));
      */
      delay(10);
    }
  }
  return F;
}

void setup_client() {

  if (findWifi()) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);
    
    int  countConexion = 0;   // Account to determine if you can connect or not
    while (WiFi.status() != WL_CONNECTED and countConexion <= 150) {
      countConexion++;
      delay(100);
      Serial.print(".");
      digitalWrite(LED, HIGH);
      delay(100);
      digitalWrite(LED, LOW);
    }
    if (countConexion > 150) {
      Serial.println("Error de conexion");
      wasFound = true;
    }
    if (countConexion <= 150) {
      isBroken = false;
      while (true) {
        if (!isBroken) {
          if (isConnecte == true) {
            wdt_reset();
            unsigned long currentMillis = millis();
            if (currentMillis - previousMillis >= 1000) {
              previousMillis = currentMillis;
              digitalWrite(LED, !digitalRead(LED));
              
              clientLoop();
              
            }
            server.handleClient();
          }
        } else {
          break;
        }
      }
    }
  }
}

void setup_AP () {
  isConnecte = false;
  digitalWrite(LED, LOW);
  Serial.println("Configuring access point...");
  WiFi.softAP(APssid, APpassword);

  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
  
  Serial.println("HTTP server started");
  while (true) {
    wdt_reset();
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= 10000) {
      if (!wasFound) {
        if (findWifi()) {
          break;
        }
      }
      previousMillis = currentMillis;
    }
    server.handleClient();
  }
  setup_client();
  setup_AP();
}

void handleRoot() {
  server.send(200, "text/html", "OK");
}

void save_config() {
  save_value(0,server.arg("ssid"));
  save_value(30,server.arg("pass"));

  Serial.println(server.arg("ssid"));
  Serial.println(server.arg("pass"));
  
  server.send(200, "text/html", "OK");
}

void apply_config() {
  read_value(0).toCharArray(ssid, 30);
  read_value(30).toCharArray(pass, 30);
  
  WiFi.disconnect(); 
  setup_client();
  setup_AP();
}

void handleNotFound(){
  server.send(401, "text/plain", "401: Unauthorized");
}

void save_value(int addr, String a) {
  int tamano = a.length(); 
  char inchar[30]; 
  a.toCharArray(inchar, tamano+1);
  for (int i = 0; i < tamano; i++) {
    EEPROM.write(addr+i, inchar[i]);
  }
  for (int i = tamano; i < 30; i++) {
    EEPROM.write(addr+i, 255);
  }
  EEPROM.commit();
}

String read_value(int addr) {
   byte lectura;
   String strlectura;
   for (int i = addr; i < addr+30; i++) {
      lectura = EEPROM.read(i);
      if (lectura != 255) {
        strlectura += (char)lectura;
      }
   }
   return strlectura;
}

void setup() {
  init_configs();

  read_value(0).toCharArray(ssid, 30);
  read_value(30).toCharArray(pass, 30);
  Serial.println("");
  Serial.println(ssid);
  Serial.println(pass);

  setup_client();
  setup_AP();
}
 
void loop() {}
