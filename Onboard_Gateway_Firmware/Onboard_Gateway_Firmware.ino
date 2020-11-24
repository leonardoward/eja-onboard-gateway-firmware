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
#define TIME_PARAM_INPUT_1 "hours"  // Params used to parse the html form submit
#define TIME_PARAM_INPUT_2 "min"    // Params used to parse the html form submit
#define TIME_PARAM_INPUT_3 "sec"    // Params used to parse the html form submit

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

// Variables of the timer
int timer_end_hours = 0;
int timer_end_min = 0;
int timer_end_sec = 0;
unsigned long timer_init_millis = 0;
unsigned long timer_end_millis = 0;
unsigned long timer_current = 0;

bool send_updated_timer = false;
bool reseting_timer = false;

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
    String message = "";
    String messageid = "";
    if(send_updated_timer == true){ // Update the timer values
      message = "{\"type\": \"timer\",";
      message += " \"init_ms\": \"" + String(timer_init_millis) + "\",";
      message += " \"end_ms\": \"" + String(timer_end_millis) + "\"}";
      messageid = "Timer Update";
    }else{                          // Send default message
      // Prepare LoRa TX message
      message = "HeLoRa World! ";
      message += "I'm a Gateway! Runtime:";
      message += millis();
      messageid = "Default";
    }
    
    LoRa_sendMessage(message);    // Send a message

    lora_all_msg += "LoRaTX " + messageid + "<br>";
    Serial.println("LoRaTX "+messageid);

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

  Serial.print("LoRaRX Timestamp: ");
  Serial.println(millis());
  // Show the received message in serial monitor
  Serial.print("LoRaRX Received: ");
  Serial.println(message);

  // Parse message
  int init_index = 0;
  int end_index = message.indexOf(':', init_index);
  String json_param = message.substring(message.indexOf('"', init_index) + 1, end_index - 1);
  String json_value = message.substring(message.indexOf('"', end_index) + 1 , message.indexOf(',', end_index) - 1);
  unsigned long new_timer_init_millis = 0;
  unsigned long new_timer_end_millis = 0;
  
  if(json_param.equals("type")){
    if(json_value.equals("timer")){     
      init_index = message.indexOf(',', end_index);
      end_index = message.indexOf(':', init_index);
      json_param = message.substring(message.indexOf('"', init_index) + 1, end_index - 1);
      json_value = message.substring(message.indexOf('"', end_index) + 1 , message.indexOf(',', end_index) - 1);
      if(json_param.equals("init_ms")) new_timer_init_millis = strtoul(json_value.c_str(), NULL, 10);
      init_index = message.indexOf(',', end_index);
      end_index = message.indexOf(':', init_index);
      json_param = message.substring(message.indexOf('"', init_index) + 1, end_index - 1);
      json_value = message.substring(message.indexOf('"', end_index) + 1 , message.indexOf(',', end_index) - 1);
      if(json_param.equals("end_ms")) new_timer_end_millis = strtoul(json_value.c_str(), NULL, 10);
      Serial.println("Type:timer");
      Serial.println("Init:"+String(new_timer_init_millis)+" ms");
      Serial.println("End:"+String(new_timer_end_millis)+" ms");
      if(send_updated_timer == true){               // The device is sending an updater timer
        Serial.println("Checking timer variables...");
        // Check if the timer has been updated in the Buoy
        if(timer_init_millis == new_timer_init_millis && timer_end_millis == new_timer_end_millis){
          // The timer variables have been updated
          send_updated_timer = false;
          reseting_timer = false;
          Serial.println("Updated timer variables");
        }else{
          Serial.println("The timer variables haven't been updated");
        }
      }else{
        timer_init_millis = new_timer_init_millis;
        timer_end_millis = new_timer_end_millis;
      }
      String timer_data = get_remaining_time(timer_end_millis);     
      Serial.println("Timer Data:"+timer_data);
    } 
  }
  
  
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

  // Route to change the timer (web page)
  server.on("/timer", HTTP_GET, [](AsyncWebServerRequest *request) {
    if(timer_end_millis > timer_init_millis){
      request->send(SPIFFS, "/gettimer.html", String(), false, processor);
    } else {
      request->send(SPIFFS, "/settimer.html", String(), false, processor);
    }
  });

  // Route to delete the timer (web page)
  server.on("/deletetimer", HTTP_GET, [](AsyncWebServerRequest *request) {
    timer_init_millis = 0;
    timer_end_millis = 0;
    send_updated_timer = true; 
    reseting_timer = true;
    request->send(SPIFFS, "/settimer.html", String(), false, processor);
  });

  // Send a GET request to <ESP_IP>/get?input1=<inputMessage>
  server.on("/setTime", HTTP_GET, [] (AsyncWebServerRequest *request) {
    
    String inputHours = "0";
    String inputMin = "0";
    String inputSec = "0";

    // GET input1 value on <ESP_IP>/get?input1=<inputMessage>
    if (request->hasParam(TIME_PARAM_INPUT_1)) {
      inputHours = request->getParam(TIME_PARAM_INPUT_1)->value();
      timer_end_hours = inputHours.toInt();
      if (0 > timer_end_hours || 23 < timer_end_hours) timer_end_hours = 0;
    }
    // GET input2 value on <ESP_IP>/get?input2=<inputMessage>
    if (request->hasParam(TIME_PARAM_INPUT_2)) {
      inputMin = request->getParam(TIME_PARAM_INPUT_2)->value();
      timer_end_min = inputMin.toInt();
      if (0 > timer_end_min || 59 < timer_end_min) timer_end_min = 0;
    }
    // GET input3 value on <ESP_IP>/get?input3=<inputMessage>
    if (request->hasParam(TIME_PARAM_INPUT_3)) {
      inputSec = request->getParam(TIME_PARAM_INPUT_3)->value();
      timer_end_sec = inputSec.toInt();
      if (0 > timer_end_sec || 59 < timer_end_sec) timer_end_sec = 0;
    }
    timer_init_millis = millis();
    timer_end_millis = timer_init_millis + timer_end_sec * 1000 + timer_end_min * 60 * 1000 + timer_end_hours * 60 * 60 * 1000;
    timer_end_hours = 0;
    timer_end_min = 0;
    timer_end_sec = 0;
    //Serial.println("Time Input - Hours : " + inputHours + " - Min : " + inputMin + " - Sec : " + inputSec);
    terminal_messages += "Time Input - H:" + inputHours + " - M:" + inputMin + " - S:" + inputSec;
    //request->send(200, "text/html", "HTTP GET request sent to Buoy B<br>Hours : " + inputHours + "<br>Min : " + inputMin + "<br>Sec : " + inputSec + "<br><a href=\"/\">Return to Home Page</a>");
    if(timer_end_millis > timer_init_millis){
      send_updated_timer = true;                                            // Update timer in Buoy
      request->send(SPIFFS, "/gettimer.html", String(), false, processor);
    } else {
      request->send(SPIFFS, "/settimer.html", String(), false, processor);
    }
  });

  // Route to load a json with the timer data
  server.on("/timer_data", HTTP_GET, [](AsyncWebServerRequest *request) {
    String timer_data = "";
    int enable_button = 1;
    String timer_json = "";
    if(send_updated_timer == true){
      timer_data = "Updating timer in Buoy...";
      enable_button = 0;
    }else{
      timer_data = get_remaining_time(timer_end_millis);
    }
    timer_json = "{\"remaining_time\": \"" + timer_data +"\",";
    timer_json += " \"enable_delete_button\": "+String(enable_button)+"}";
    request->send(200, "application/json", timer_json);
  });

  // Route to load a json with the timer data
  server.on("/deletetimer_data", HTTP_GET, [](AsyncWebServerRequest *request) {
    String message = "";
    if(reseting_timer == true){
      message = "{\"reseting_timer\": 1}";
    }else{
      message = "{\"reseting_timer\": 0}";
    }
    request->send(200, "application/json", message);
  });

}

String get_remaining_time(unsigned long timer_end_millis){
  unsigned long timer_current = millis();
  String timer_data = "";
  if(timer_end_millis > timer_current){
    int remainig_hours = int((timer_end_millis-timer_current)/(60 * 60 * 1000));
    String remainig_hours_str = String(remainig_hours);
    if(remainig_hours < 10) remainig_hours_str = "0" + remainig_hours_str;
    int remainig_min = int((timer_end_millis-timer_current)/(60 * 1000)) - remainig_hours * 60;
    String remainig_min_str = String(remainig_min);
    if(remainig_min < 10) remainig_min_str = "0" + remainig_min_str;
    int remainig_sec = int((timer_end_millis-timer_current)/(1000)) - remainig_hours * 60 * 60 - remainig_min * 60;
    String remainig_sec_str = String(remainig_sec);
    if(remainig_sec < 10) remainig_sec_str = "0" + remainig_sec_str;
    timer_data = remainig_hours_str + ":" + remainig_min_str + ":" + remainig_sec_str;
  }else{
    timer_data = "00:00:00";
  }
  return timer_data;
}
