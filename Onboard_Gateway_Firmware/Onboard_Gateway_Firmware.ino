/*LoRa Config
 * This code uses InvertIQ function to create a simple Gateway/Node logic.
  Gateway - Sends messages with enableInvertIQ()
          - Receives messages with disableInvertIQ()
  Node    - Sends messages with disableInvertIQ()
          - Receives messages with enableInvertIQ()
  With this arrangement a Gateway never receive messages from another Gateway
  and a Node never receive message from another Node.
  Only Gateway to Node and vice versa.
  This code receives messages and sends a message every second.
 * 
 */
// include libraries
#include <WiFi.h>
#include <DNSServer.h>
#include <AsyncTCP.h>
#include "ESPAsyncWebServer.h"
#include <AsyncElegantOTA.h>
#include "SPIFFS.h"
#include <SPI.h>
#include <LoRa.h>

#define LED1 22                 // ESP32 GPIO connected to LED1
#define LED2 5                  // ESP32 GPIO connected to LED2
#define LED3 16                 // ESP32 GPIO connected to LED3
#define LED4 17                 // ESP32 GPIO connected to LED4
#define LED_LORA_RX LED1        // Led that notifies LoRa RX
#define LED_LORA_TX LED2        // Led that notifies LoRa TX
#define LED_AP_REQUEST LED3     // Led that notifies if there is an AP client
#define LED_TOGGLE LED4         // Led use for togle example (toggle from web page)
#define CS_LORA 2               // LoRa radio chip select
#define RST_LORA 15             // LoRa radio reset
#define IRQ_LORA 13             // LoRa Hardware interrupt pin
#define PERIOD_TX_LORA 1000     // Period between Lora Transmissions
#define PERIOD_ERASE_BUFF 80000 // Period between the erase of terminal msgs
#define DNS_PORT 53             // Port used by the DNS server
#define WEB_SERVER_PORT 80      // Port used by the Asynchronous Web Server
#define LORA_FREQUENCY 915E6    // Frequency used by the LoRa module

// Server credentials
const char* host = "www.onboard_gateway.eja";
const char* ssid     = "EJA_Onboard_Gateway";
const char* password = "123456789";

// Asynchronous web server object
AsyncWebServer server(WEB_SERVER_PORT);

// DNS Server object
DNSServer dnsServer;

// State of the led use for the toogle example
String toggle_led_state;

// Buffers that store internal messages
String terminal_messages = "";
String lora_all_msg = "";

// Time (millis) counters for timed operations
unsigned long last_msg_rx = 0;
unsigned long lora_last_tx = 0;
unsigned long last_erase_buffers = 0;
unsigned long last_ap_request = 0;

//GPS received with LoRa
String lora_rx_message = "";

void setup() {
  // Initialize serial communication (used for debugging)
  Serial.begin(115200);

  // Initialize the output variables as outputs
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT);
  pinMode(LED4, OUTPUT);

  // Setup LoRa module
  LoRa.setPins(CS_LORA, RST_LORA, IRQ_LORA);

  // Initialize the LoRa module
  if (!LoRa.begin(LORA_FREQUENCY)) {
    Serial.println("LoRa init failed. Check your connections.");
    lora_all_msg += "LoRa init failed. Check your connections.<br>";
    while (true);                       // if failed, do nothing
  }

  // Initialize SPIFFS
  if(!SPIFFS.begin(true)){
    Serial.println("An Error has occurred while mounting SPIFFS");
    terminal_messages += "An Error has occurred while mounting SPIFFS <br>";
    return;
  }

  // Connect to Wi-Fi network with SSID and password
  Serial.print("Setting AP (Access Point)…");
  terminal_messages += "Setting AP (Access Point)…<br>";
  // Remove the password parameter, if you want the AP (Access Point) to be open
  WiFi.softAP(ssid, password);

  // Get the IP address
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  terminal_messages += "AP IP address: <br>";
  Serial.println(IP);

  // Start the dns server
  dnsServer.start(DNS_PORT, host, IP);

  //Add the html, css and js files to the web server
  web_server_config();
  //web_server_config_2();
  
  // Start ElegantOTA
  AsyncElegantOTA.begin(&server);

  // Start server
  server.begin();

  // Print LoRa initialization messages
  Serial.println("LoRa init succeeded.");
  Serial.println();
  Serial.println("LoRa Simple Gateway");
  Serial.println("Only receive messages from nodes");
  Serial.println("Tx: invertIQ enable");
  Serial.println("Rx: invertIQ disable");
  Serial.println();

  // Store LoRa initialization messages
  lora_all_msg += "LoRa init succeeded.<br><br>";
  lora_all_msg += "LoRa Simple Gateway<br>";
  lora_all_msg += "Only receive messages from nodes<br>";
  lora_all_msg += "Tx: invertIQ enable<br>";
  lora_all_msg += "Rx: invertIQ disable<br><br>";

  LoRa.onReceive(onReceiveLora);  // Config LoRa RX routines
  LoRa.onTxDone(onTxDoneLoRa);    // Config LoRa TX routines
  LoRa_rxMode();                  // Activate LoRa RX

}

void loop(){
  dnsServer.processNextRequest(); // Process next DNS request
  AsyncElegantOTA.loop();         // Process next OTA request

  // Transmit LoRa msg every PERIOD_TX_LORA millis
  if (runEvery(PERIOD_TX_LORA, &lora_last_tx)) {
    // Prepare LoRa TX message
    String message = "HeLoRa World! ";
    message += "I'm a Gateway! ";
    message += millis();

    LoRa_sendMessage(message);    // Send a message

    lora_all_msg += "Send Message!<br>";
    Serial.println("Send Message!");

  }

  // Erase terminal buff every PERIOD_ERASE_BUFF millis
  if(runEvery(PERIOD_ERASE_BUFF, &last_erase_buffers)){
    terminal_messages = "";
    lora_all_msg = "";
    Serial.println("Erased terminal buffers");
  }

  // Turn on LED if there is a recent AP Request
  if (millis()-last_ap_request < 1000){
    digitalWrite(LED_AP_REQUEST, HIGH);
  }else{
    digitalWrite(LED_AP_REQUEST, LOW);
  }
}

/**
 * [runEvery Check if a time interval has passed since the last previousMillis]
 * @param  interval       [Time interval]
 * @param  previousMillis [Time of the previous activation]
 * @return                [True if the time interval has passed since the last
 *                         previousMillis, False if not]
 */
boolean runEvery(unsigned long interval, unsigned long *previousMillis)
{
  unsigned long currentMillis = millis(); // Store the current time
  // Check if a time interval has passed since the last previousMillis
  if (currentMillis - *previousMillis >= interval)
  {
    *previousMillis = currentMillis;      // Update the time of *previousMillis
    return true;
  }
  return false;
}

//########################################
//##      Asynchronous Web Server       ##
//########################################

/**
 * [processor Process requests from web pages and return data]
 * @param  var [description]
 * @return     [description]
 */
String processor(const String& var){
  String result = "";                 // Create a variable to store the result

  Serial.println(var);                // Show the requested var in the serial monitor
  terminal_messages += var + "<br>";  // Add the requested var to the terminal buffer

  if(var == "STATE"){                 // Return the states of the toggle led
    if(digitalRead(LED_TOGGLE)){
      toggle_led_state = "ON";
    }
    else{
      toggle_led_state = "OFF";
    }
    Serial.print(toggle_led_state);
    terminal_messages += toggle_led_state + " <br>";
    result = toggle_led_state;
  }else if(var == "TERMINAL"){        // return the terminal messages buffer
    result = terminal_messages;
  }else if(var == "TERMINAL_LORA"){   // return the LoRa messages buffer
    result = lora_all_msg;
  }
  
  return result;
}

//########################################
//##              LoRa                  ##
//########################################

/**
 * [LoRa_rxMode Setup Lora's Receiver Mode]
 */
void LoRa_rxMode(){
  digitalWrite(LED_LORA_RX, HIGH); // turn on the LoRa RX LED
  digitalWrite(LED_LORA_TX, LOW);  // turn off the LoRa TX LED
  LoRa.disableInvertIQ();          // normal mode
  LoRa.receive();                  // set receive mode
}

/**
 * [LoRa_txMode Setup Lora's Transmitter Mode]
 */
void LoRa_txMode(){
  digitalWrite(LED_LORA_RX, LOW);  // turn off the LoRa RX LED
  digitalWrite(LED_LORA_TX, HIGH); // turn on the LoRa TX LED
  LoRa.idle();                     // set standby mode
  LoRa.enableInvertIQ();           // active invert I and Q signals
}

/**
 * [LoRa_sendMessage Transmit a String using LoRa]
 * @param message [String that will be transmitted]
 */
void LoRa_sendMessage(String message) {
  LoRa_txMode();          // set tx mode
  LoRa.beginPacket();     // start packet
  LoRa.print(message);    // add payload
  LoRa.endPacket(true);   // finish packet and send it
}

/**
 * [onReceiveLora Interrupt that activates when LoRa receives a message]
 * @param packetSize [Size of the package received]
 */
void onReceiveLora(int packetSize) {
  String message = "";            //Variable used to store the received message

  while (LoRa.available()) {      // Loop while there is data in the RX buffer
    message += (char)LoRa.read(); // Read a new value from the RX buffer
  }

  // Show the received message in serial monitor
  Serial.print("Gateway Receive: ");
  Serial.println(message);

  // Store the message in the string with all messages associated to lora
  lora_all_msg += "Gateway Receive: <br>";
  lora_all_msg += message + "<br>";

  lora_rx_message = message;      // Store message in a global variable

  last_msg_rx = millis();         // Store the time of the last reception
}

/**
 * [onTxDoneLoRa Interrupt that activates when LoRa ends transmission]
 */
void onTxDoneLoRa() {
  Serial.println("TxDone");     // Notify the end of transmission in the serial monitor
  lora_all_msg += "TxDone<br>"; // Store a message from the Lora process
  LoRa_rxMode();                // Activate lora's reception mode
}

//########################################
//##              Web server            ##
//########################################
void web_server_config(){
  // Route to load the sidebar.js file
  server.on("/sidebar.js", HTTP_GET, [](AsyncWebServerRequest *request){
    last_ap_request = millis();
    request->send(SPIFFS, "/sidebar.js", "text/javascript");
  });
  
  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    last_ap_request = millis();
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });

  // Route to load style.css file
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
    last_ap_request = millis();
    request->send(SPIFFS, "/style.css", "text/css");
  });

  // Route to load sidebar.css file
  server.on("/sidebar.css", HTTP_GET, [](AsyncWebServerRequest *request){
    last_ap_request = millis();
    request->send(SPIFFS, "/sidebar.css", "text/css");
  });

  // Route to load header.css file
  server.on("/header.css", HTTP_GET, [](AsyncWebServerRequest *request){
    last_ap_request = millis();
    request->send(SPIFFS, "/header.css", "text/css");
  });


  // Route to set TOGGLE GPIO to HIGH
  server.on("/toggle_led_on", HTTP_GET, [](AsyncWebServerRequest *request){
    digitalWrite(LED_TOGGLE, HIGH);
    last_ap_request = millis();
    request->send(SPIFFS, "/toggle_led.html", String(), false, processor);
  });

  // Route to set TOGGLE GPIO to LOW
  server.on("/toggle_led_off", HTTP_GET, [](AsyncWebServerRequest *request){
    digitalWrite(LED_TOGGLE, LOW);
    last_ap_request = millis();
    request->send(SPIFFS, "/toggle_led.html", String(), false, processor);
  });

  // Route to load the jquery.min.js file
  server.on("/jquery.min.js", HTTP_GET, [](AsyncWebServerRequest *request){
    last_ap_request = millis();
    request->send(SPIFFS, "/jquery.min.js", "text/javascript");
  });

  // Route to load the lora web page
  server.on("/lora", HTTP_GET, [](AsyncWebServerRequest *request) {
    last_ap_request = millis();
    request->send(SPIFFS, "/lora.html", String(), false, processor);
  });

  // Route to load a json with the global terminal messages from lora
  server.on("/terminal_messages", HTTP_GET, [](AsyncWebServerRequest *request) {
    last_ap_request = millis();
    request->send(200, "application/json", "{\"term\": \""+ terminal_messages+"\"}");
  });

  // Route to load a json with the terminal messages from lora
  server.on("/lora_terminal_messages", HTTP_GET, [](AsyncWebServerRequest *request) {
    last_ap_request = millis();
    request->send(200, "application/json", "{\"term\": \""+ lora_all_msg+"\"}");
  });

  // Route to load the terminal web page
  server.on("/terminal", HTTP_GET, [](AsyncWebServerRequest *request) {
    last_ap_request = millis();
    request->send(SPIFFS, "/terminal.html", String(), false, processor);
  });

  // Route to load the GPS web page
  server.on("/gps", HTTP_GET, [](AsyncWebServerRequest *request) {
    last_ap_request = millis();
    request->send(SPIFFS, "/gps.html", String(), false, processor);
  });

  // Route to load a json with the GPS data
  server.on("/gps_data", HTTP_GET, [](AsyncWebServerRequest *request) {
    last_ap_request = millis();
    request->send(200, "application/json", lora_rx_message);
  });
}

void web_server_config_2(){
  // Route to load adminlte.min.css file
  server.on("/adminlte.min.css", HTTP_GET, [](AsyncWebServerRequest *request){
    last_ap_request = millis();
    request->send(SPIFFS, "/adminlte.min.css", "text/css");
  });
  // Route to load adminlte.min.css file
  server.on("/all.min.css", HTTP_GET, [](AsyncWebServerRequest *request){
    last_ap_request = millis();
    request->send(SPIFFS, "/all.min.css", "text/css");
  });
  // Route to load preloader.css file
  server.on("/preloader.css", HTTP_GET, [](AsyncWebServerRequest *request){
    last_ap_request = millis();
    request->send(SPIFFS, "/preloader.css", "text/css");
  });
  // Route to load adminlte.js file
  server.on("/adminlte.js", HTTP_GET, [](AsyncWebServerRequest *request){
    last_ap_request = millis();
    request->send(SPIFFS, "/adminlte.js", "text/javascript");
  });
  // Route to load bootstrap.bundle.min.js file
  server.on("/bootstrap.bundle.min.js", HTTP_GET, [](AsyncWebServerRequest *request){
    last_ap_request = millis();
    request->send(SPIFFS, "/bootstrap.bundle.min.js", "text/javascript");
  });
  // Route to load bootstrap.bundle.min.js file
  server.on("/jquery.min.js", HTTP_GET, [](AsyncWebServerRequest *request){
    last_ap_request = millis();
    request->send(SPIFFS, "/jquery.min.js", "text/javascript");
  });
  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    last_ap_request = millis();
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });
  // Route to set TOGGLE GPIO to HIGH
  server.on("/toggle_led_on", HTTP_GET, [](AsyncWebServerRequest *request){
    digitalWrite(LED_TOGGLE, HIGH);
    last_ap_request = millis();
    request->send(SPIFFS, "/toggle_led.html", String(), false, processor);
  });

  // Route to set TOGGLE GPIO to LOW
  server.on("/toggle_led_off", HTTP_GET, [](AsyncWebServerRequest *request){
    digitalWrite(LED_TOGGLE, LOW);
    last_ap_request = millis();
    request->send(SPIFFS, "/toggle_led.html", String(), false, processor);
  });
  // Route to load the lora web page
  server.on("/lora.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    last_ap_request = millis();
    request->send(SPIFFS, "/lora.html", String(), false, processor);
  });

  // Route to load a json with the global terminal messages from lora
  server.on("/terminal_messages", HTTP_GET, [](AsyncWebServerRequest *request) {
    last_ap_request = millis();
    request->send(200, "application/json", "{\"term\": \""+ terminal_messages+"\"}");
  });

  // Route to load a json with the terminal messages from lora
  server.on("/lora_terminal_messages", HTTP_GET, [](AsyncWebServerRequest *request) {
    last_ap_request = millis();
    request->send(200, "application/json", "{\"term\": \""+ lora_all_msg+"\"}");
  });

  // Route to load the terminal web page
  server.on("/terminal.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    last_ap_request = millis();
    request->send(SPIFFS, "/terminal.html", String(), false, processor);
  });

  // Route to load the GPS web page
  server.on("/gps.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    last_ap_request = millis();
    request->send(SPIFFS, "/gps.html", String(), false, processor);
  });

  // Route to load a json with the GPS data
  server.on("/gps_data", HTTP_GET, [](AsyncWebServerRequest *request) {
    last_ap_request = millis();
    request->send(200, "application/json", lora_rx_message);
  });
}
