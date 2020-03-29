#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoWebsockets.h>
#include <EEPROM.h>

#define LED 2

using namespace websockets;
WebsocketsClient webSocket;
ESP8266WebServer server (80);
WiFiEventHandler DisConnectHandler;
WiFiEventHandler GotIpHandler;

unsigned long previousMillis0;
unsigned long previousMillis1;
unsigned long previousMillis2;
unsigned long previousMillis3;

//const char* serial     = "5RWF6UA3";
const char* serial     = "BH6DQZNU";
const char* APssid     = "ESPap";
const char* APpassword = "0123456789";
char*     ssid         = "HOME-93DF";
char*     pass         = "159753852";
//char        ssid[30]    = "";
//char        pass[30]    = "";

unsigned long Millis;
bool          wsConnection = false;  // Dice si el web socket esta conectado o no
bool          isConnecte   = false;  // Dice si se pueden enviar datos al servidor o no
bool          wasFound     = false;  // Dice si debe seguir intentando conectar al wifi luego de no poder hacerlo
bool          isBroken     = false;  // Rompe la ejecución del hilo de conexión de modo cliente
const char*   websockets_server = "wss://192.168.1.100:443/"; // Direccion del servidor

// Evento wifi desconectado
void onDisconnectHandler(const WiFiEventStationModeDisconnected& event){
  if (isConnecte == true) {
    Serial.println ( "WiFi disconnect." );
    isBroken = true;
    isConnecte = false;
  }
}

// Evento wifi conectado
void onGotIPHandler(const WiFiEventStationModeGotIP& event){
  Serial.print ("Got Ip: ");
  Serial.println(WiFi.localIP());
  isConnecte = true;
}

// Busca el ssid guardado dentro de las redes disponibles
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
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.println(WiFi.SSID(i));
      delay(10);
    }
  }
  return F;
}

void wsConnector (const char* url) {
  if(webSocket.connect(url)) {
    Serial.println("Connecetd!");
    webSocket.send(serial);
    wsConnection = true;
  } else {
      Serial.println("Not Connected!");
  }
}

void setup_client() {

  if (findWifi()) {
    Serial.printf("Connecting to %s ", ssid);
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);
    
    int  countConexion = 0;   // Cuenta para determinar si se puede conectar a una red o no
    while (WiFi.status() != WL_CONNECTED and countConexion <= 150) {
      countConexion++;
      delay(100);
      Serial.print(".");
      digitalWrite(LED, HIGH);
      delay(100);
      digitalWrite(LED, LOW);
      wdt_reset();
    }
    if (countConexion > 150) {
      Serial.println("Error de conexion");
      wasFound = true;
    }
    if (countConexion <= 150) {
      isBroken = false;

      // Setup Callbacks
      webSocket.onEvent(onEventsCallback);
      webSocket.onMessage(onMessageCallback);
      
      while (!isBroken) {
        if (isConnecte == true) {
          if (!wsConnection) {
            // Connect to web socket server
            if (millis() - previousMillis0 >= 10000) {
              previousMillis0 = millis();
              wsConnector(websockets_server);
            }
          } else {
            webSocket.poll();
            clientLoop();
            if (millis() - previousMillis1 >= 10000) {
              previousMillis1 = millis();
              webSocket.ping();
            }
          }
          wdt_reset();
          server.handleClient();
        }
      }
    }
  }
}

void setup_AP () {
  digitalWrite(LED, LOW);
  Serial.println("Configuring access point...");
  WiFi.mode(WIFI_AP);
  WiFi.softAP(APssid, APpassword);

  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
  
  while (true) {
    wdt_reset();
    if (millis() - previousMillis2 >= 10000) {
      previousMillis2 = millis();
      if (!wasFound) {
        if (findWifi()) {
          break;
        }
      }
    }
    server.handleClient();
  }
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
  pinMode (LED, OUTPUT);
  digitalWrite (LED, HIGH);
  Serial.begin (74880);
  EEPROM.begin(100);

  GotIpHandler       = WiFi.onStationModeGotIP(&onGotIPHandler);
  DisConnectHandler  = WiFi.onStationModeDisconnected(&onDisconnectHandler);

  server.on("/"      , handleRoot);
  server.on("/config", save_config);
  server.on("/apply" , apply_config);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started");

  //read_value(0).toCharArray(ssid, 30);
  //read_value(30).toCharArray(pass, 30);
  Serial.println("");
  Serial.println(ssid);
  Serial.println(pass);
}

void onMessageCallback(WebsocketsMessage message) {
    Serial.print("Got Message: ");
    Serial.println(message.data());
}

void onEventsCallback(WebsocketsEvent event, String data) {
    if(event == WebsocketsEvent::ConnectionOpened) {
        Serial.println("Connnection Opened");
        wsConnection = true;
    } else if(event == WebsocketsEvent::ConnectionClosed) {
        Serial.println("Connnection Closed");
        wsConnection = false;
    } else if(event == WebsocketsEvent::GotPing) {
        Serial.println("Got a Ping!");
    } else if(event == WebsocketsEvent::GotPong) {
        Serial.println("Got a Pong!");
    }
}

void clientLoop () {
  if (Serial.available()) {
    String texto = Serial.readString();
    webSocket.send(texto);
  }

  if (millis() - previousMillis3 >= 2000) {
    previousMillis3 = millis();

    digitalWrite(LED, !digitalRead(LED));
    int randNumber = random(200);
    Serial.println("[{\"value\":" + (String)randNumber + "}]");
    webSocket.send("[{\"value\":" + (String)randNumber + "}]");
  }
}
 
void loop() {
  setup_client();
  setup_AP();
}
