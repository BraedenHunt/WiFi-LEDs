#include <Adafruit_NeoPixel.h>
#include <WiFi.h>

#define N_LEDS 150
#define N_STATES 4

#define N_TASKS 2

// Auxiliar variables to store the current output state
enum State { OFF, CHASE, ON, RAINBOW};
String stateNames[N_STATES] = {"Off", "Chase", "Solid", "Rainbow"};
State stripState = OFF;
State prevStripState;
const int stripOutput = 16;

const char* ssid = "NHMT2G";
const char* pwd = "losttruth738";

WiFiServer server(80);

// Variable to store the HTTP request
String header;

// Current time
unsigned long currentTime = millis();
// Previous time
unsigned long previousTime = 0;
// Define timeout time in milliseconds (example: 2000ms = 2s)
const long timeoutTime = 2000;

TaskHandle_t tasks[N_TASKS];

Adafruit_NeoPixel strip = Adafruit_NeoPixel(N_LEDS, stripOutput, NEO_GRB + NEO_KHZ800);

uint32_t wait_time = 10;
int LedR = 0;
int LedG = 179;
int LedB = 255;

int prevR;
int prevG;
int prevB;

void setup() {
  strip.begin();
  strip.show();
  Serial.begin(115200);

  Serial.print("setup() running on core ");
  Serial.println(xPortGetCoreID());


  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, pwd);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  server.begin();
  Serial.print("loop() running on core ");
  Serial.println(xPortGetCoreID());

  // Initialize Tasks
  xTaskCreatePinnedToCore(
    theaterChase, /* Function to implement the task */
    "theaterChaseTask", /* Name of the task */
    10000,  /* Stack size in words */
    NULL,  /* Task input parameter */
    0,  /* Priority of the task */
    &tasks[0],  /* Task handle. */
    0); /* Core where the task should run */
  xTaskCreatePinnedToCore(
    rainbow, /* Function to implement the task */
    "rainbowTask", /* Name of the task */
    10000,  /* Stack size in words */
    NULL,  /* Task input parameter */
    0,  /* Priority of the task */
    &tasks[1],  /* Task handle. */
    0); /* Core where the task should run */
  for (int i = 0; i < N_TASKS; i++) {
    if (tasks[i] != NULL) {
      vTaskSuspend(tasks[i]);
    }
  }

}

void loop() {
  prevStripState = stripState;
  prevR = LedR;
  prevG = LedG;
  prevB = LedB;
  handleWiFiClient();

  if (prevStripState != stripState || prevR != LedR || prevG != LedG || prevB != LedB) {
    Serial.println("Deleting Task");
    for (int i = 0; i < N_TASKS; i++) {
      if (tasks[i] != NULL) {
        vTaskSuspend(tasks[i]);
      }
    }
    Serial.println("Past deletion");
    switch (stripState) {
      case OFF: setAll(0, 0, 0); break;
      case CHASE:
        Serial.println("Starting Rainbow");
        vTaskResume(tasks[0]);
        break;
      case ON: setAll(LedR, LedG, LedB); break;
      case RAINBOW:
        Serial.println("Starting Rainbow");
        vTaskResume(tasks[1]);
        break;
    }
  }
}

void handleWiFiClient() {
  WiFiClient client = server.available();

  if (client) {
    currentTime = millis();
    previousTime = currentTime;
    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected() && currentTime - previousTime <= timeoutTime) {  // loop while the client's connected
      currentTime = millis();
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();

            Serial.println("Header: " + header);
            // turns the GPIOs on and off
            //stripState = static_cast<State>(parseState(header));
            if (header.indexOf("GET /state?state=0") >= 0) {
              Serial.println("GPIO 16 off");
              stripState = OFF;
            } else if (header.indexOf("GET /state?state=1") >= 0) {
              Serial.println("GPIO 16 chase");
              stripState = CHASE;
            }
            else if (header.indexOf("GET /state?state=2") >= 0) {
              Serial.println("GPIO 16 on");
              stripState = ON;
            }
            else if (header.indexOf("GET /state?state=3") >= 0) {
              Serial.println("GPIO 16 on");
              stripState = RAINBOW;
            }
            int clr_s_i = header.indexOf("color=%23");
            if (clr_s_i >= 0) {
              clr_s_i += 9;
              // Get rid of '#' and convert it to integer
              int number = (int) strtol( &header[clr_s_i], NULL, 16);

              // Split them up into r, g, b values
              LedR = number >> 16;
              LedG = number >> 8 & 0xFF;
              LedB = number & 0xFF;

            }

            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            // CSS to style the on/off buttons
            // Feel free to change the background-color and font-size attributes to fit your preferences
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;");
            client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
            client.println(".button2 {background-color: #555555;}</style></head>");

            // Web Page Heading
            client.println("<body><h1>ESP32 Web Server</h1>");

            // Display current state, and ON/OFF buttons for GPIO 16
            client.println("<p>Strip - State " + stateNames[stripState] + "</p>");
            // If the output26State is off, it displays the ON button
            client.println("<form action=\"/state\"><label for=\"state\">Choose a state:</label><select name=\"state\" id=\"state\">");
            for (int i = 0; i < N_STATES; i++) {
              if (i == stripState) {
                client.println(String("<option value=\"") + i + "\" selected>" + stateNames[i] + "</option>");
              }
              else {
                client.println(String("<option value=\"") + i + "\">" + stateNames[i] + "</option>");
              }
            }
            long RGB_hex = ((long)LedR << 16L) | ((long)LedG << 8L) | (long)LedB;
            char RGB_hex_str[8];
            sprintf(RGB_hex_str, "%06X", RGB_hex);
            client.println(String("</select><br><br><input type='color' name='color' value='#") + RGB_hex_str + "'><br><br><input type=\"submit\" value=\"Submit\"></form>");
            client.println("</body></html>");

            // The HTTP response ends with another blank line
            client.println();
            // Break out of the while loop
            break;
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
}

int parseState(String header) {
  Serial.println("Header To Parse: " + header);
  int startIndex = header.indexOf("GET /state?state=");
  Serial.println("Index of equals: " + header.indexOf("="));
  Serial.println("start Index: " + startIndex);
  if (startIndex < 0) {
    return 0;
  }
  int endIndex = header.indexOf(" ", startIndex);
  Serial.println("end Index: " + endIndex);

  return header.substring(startIndex + 1, endIndex).toInt();
}

void rainbow(void * wait) {
  Serial.println("starting rainbow");
  while (true) {
    for (long firstPixelHue = 0; firstPixelHue < 5 * 65536; firstPixelHue += 256) {
      for (int i = 0; i < strip.numPixels(); i++) {
        int pixelHue = firstPixelHue + (i * 65536L / strip.numPixels());
        strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
      }
      strip.show();
      delay(wait_time);
    }
  }
}

void theaterChase(void * params) {
  int SpeedDelay = 75;
  for (;;) { // Loop forever
    for (int q = 0; q < 3; q++) {
      for (int i = 0; i < N_LEDS; i = i + 3) {
        setPixel(i + q, (byte) LedR, (byte) LedG, (byte) LedB);  //turn every third pixel on
      }
      strip.show();

      delay(SpeedDelay);

      for (int i = 0; i < N_LEDS; i = i + 3) {
        setPixel(i + q, 0, 0, 0);    //turn every third pixel off
      }
    }
  }
}

void setAll(byte red, byte green, byte blue) {
  for (int i = 0; i < N_LEDS; i++ ) {
    setPixel(i, red, green, blue);
  }
  strip.show();
}

void setPixel(int Pixel, byte red, byte green, byte blue) {
  strip.setPixelColor(Pixel, strip.Color(red, green, blue));
}
