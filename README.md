# EJA Onboard Gateway - Firmware

EJA Onboard Gateway - Firmware - 2020 Hackaday Dream Team Challenge for Conservation X Labs

This repository contains the firmware (Arduino) project used with the [electronic design of the Onboard Gateway](https://github.com/leonardoward/eja-onboard-gateway-electronics).

## Requisites ##

1. [ESP32 Add-on in Arduino IDE](https://randomnerdtutorials.com/installing-the-esp32-board-in-arduino-ide-windows-instructions/)
2. [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer)
3. [AsyncTCP](https://github.com/me-no-dev/AsyncTCP)
4. [SPIFFS File System](https://randomnerdtutorials.com/esp32-web-server-spiffs-spi-flash-file-system/)
5. [ESP32 Async Over The Air](https://github.com/ayushsharma82/AsyncElegantOTA)
6. [LoRa](https://randomnerdtutorials.com/esp32-lora-rfm95-transceiver-arduino-ide/)

## User Interface ##

The ESP32 creates an Asynchronous Web Server, with the following credentials:

```
const char* ssid     = "EJA_Onboard_Gateway";
const char* password = "123456789";
```

With those credentials it is possible to access the WiFi network.

![alt text](./img/ssid.jpg "Wifi network")

The ESP32 creates a Domain Name System (DNS) to assign a domain name to the web server. That means that it is not necessary to know the server IP, it is possible to access the web server using the defined domain name for the host, in this case, the name is:

```
const char* host = "www.onboard_gateway.eja";
```

Once the device (phone/computer) is connected to the server (EJA_Onboard_Gateway), open a browser and visit the following url:


```
http://www.onboard_gateway.eja/
```

The previous link displays the web page used for HOME:

![alt text](./img/home.jpg "Home")

There is a header with the name "EJA Onboard Gateway" that opens a sidebar with links to the different test functionalities. It is also possible to access those pages using the buttons in HOME.

![alt text](./img/menu.jpg "Menu")

The page http://www.onboard_gateway.eja/gps shows the data from the GPS module.

![alt text](./img/gps.jpg "GPS")

The page http://www.onboard_gateway.eja/lora shows internal messages related to LoRa. In the ESP32 that information is stored in the variable:

```
String lora_all_msg = "";
```

![alt text](./img/lora_01.jpg "LoRa")

The page http://www.onboard_gateway.eja/terminal shows internal messages. In the ESP32 that information is stored in the variable:

```
String terminal_messages = "";
```

![alt text](./img/terminal.jpg "Terminal")

There is an additional page http://www.onboard_gateway.eja/toggle_led_on that can be use to change the state of a LED, the GPIO port used is define in the following variable of the script:

```
#define LED_TOGGLE LED4         // Led use for togle example (toggle from web page)
```

The buttons in the page (ON and OFF) can be used to change the state of the LED.

![alt text](./img/led_test_on.jpg "LED Test On")

![alt text](./img/led_test_off.jpg "LED Test Off")
