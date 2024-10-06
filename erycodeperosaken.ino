#include <ESP8266WiFi.h>
#include "HTTPSRedirect.h"
#include "DebugMacros.h"
#include <DHT.h>
#include <MQ135.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27,16,2);
const int DHTPIN = 2;                                                       // Connect to Pin D2 in ESP8266
#define DHTTYPE DHT11                                                       // select dht type as DHT 11 or DHT22
DHT dht(DHTPIN, DHTTYPE);

int air_quality = A0;
String sheetHumid = "";
String sheetTemp = "";
String sheetairquality="";

// variables for getting the average per 10 mins

unsigned long prev = 0;

unsigned long duration = 600000; // 10 mins in milliseconds
float m = 0; // gets the total value of air quality we recorded
float h = 0; // gets the total value of humidity we recorded
float t = 0; // gets the total value of temperature we recorded
float m_ave = 0;
float h_ave = 0;
float t_ave = 0;
int n_count = 0; // number of records we recorded in our sensor

const char* ssid = "Chance the Router";                //replace with our wifi ssid // CONVERGE_MGM // Juls
const char* password = "p0l4r_B34R8632";         //replace with your wifi password // magnait_3695926 // 00000000

const char* host = "script.google.com";
const char *GScriptId = "AKfycbxVXNBzHDU5xlp4R7kGEHG1INDunafcsHuX_mtsCeZZDRV123jAvhuuKjN1w6SYFFkL"; // Replace with your own google script id
const int httpsPort = 443; //the https port is same

// echo | openssl s_client -connect script.google.com:443 |& openssl x509 -fingerprint -noout
const char* fingerprint = "";

//const uint8_t fingerprint[20] = {};

String url = String("/macros/s/") + GScriptId + "/exec?value=Temperature";  // Write Teperature to Google Spreadsheet at cell A1
// Fetch Google Calendar events for 1 week ahead
String url2 = String("/macros/s/") + GScriptId + "/exec?cal";  // Write to Cell A continuosly

//replace with sheet name not with spreadsheet file name taken from google
String payload_base =  "{\"command\": \"appendRow\", \
                    \"sheet_name\": \"airqdatsheet\", \
                       \"values\": ";
String payload = "";

HTTPSRedirect* client = nullptr;

// used to store the values of free stack and heap before the HTTPSRedirect object is instantiated
// so that they can be written to Google sheets upon instantiation

void setup() {
  lcd.init();      
  lcd.backlight();      // Make sure backlight is on

  Serial.begin(115200);
  dht.begin();     //initialise DHT11

  // Print a message on both lines of the LCD.
  lcd.setCursor(0,0);   //Set cursor to character 2 on line 0
  lcd.print("PagNababasaMoTo,");
  
  lcd.setCursor(0,1);   //Move cursor to character 2 on line 1
  lcd.print("DiDapatToGumagana");


  Serial.println();
  Serial.print("Connecting to wifi: ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // Use HTTPSRedirect class to create a new TLS connection
  client = new HTTPSRedirect(httpsPort);
  client->setInsecure();
  client->setPrintResponseBody(true);
  client->setContentTypeHeader("application/json");
  Serial.print("Connecting to ");
  Serial.println(host);          //try to connect with "script.google.com"

  // Try to connect for a maximum of 5 times then exit
  bool flag = false;
  for (int i = 0; i < 5; i++) {
    int retval = client->connect(host, httpsPort);
    if (retval == 1) {
      flag = true;
      break;
    }
    else
      Serial.println("Connection failed. Retrying...");
  }

  if (!flag) {
    Serial.print("Could not connect to server: ");
    Serial.println(host);
    Serial.println("Exiting...");
    return;
  }
// Finish setup() function in 1s since it will fire watchdog timer and will reset the chip.
//So avoid too many requests in setup()

  Serial.println("\nWrite into cell 'A1'");
  Serial.println("------>");
  // fetch spreadsheet data
  client->GET(url, host);
  
  Serial.println("\nGET: Fetch Google Calendar Data:");
  Serial.println("------>");
  // fetch spreadsheet data
  client->GET(url2, host);

 Serial.println("\nStart Sending Sensor Data to Google Spreadsheet");

  
  // delete HTTPSRedirect object
  delete client;
  client = nullptr;
}

void loop() {

  unsigned long currmill = millis();

  MQ135 gasSensor = MQ135(air_quality);
  m += gasSensor.getRZero();
  h += dht.readHumidity();                                              // Reading temperature or humidity takes about 250 milliseconds!
  t += dht.readTemperature();  
  // Read temperature as Celsius (the default)
  if (isnan(h) || isnan(t) || isnan(m)) {                                                // Check if any reads failed and exit early (to try again).
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }
 
  Serial.print("Humidity: ");  Serial.print(h);
  sheetHumid = String(h) + String(" %");                                         //convert integer humidity to string humidity
  Serial.print("%  Temperature: ");  Serial.print(t);  Serial.println("°C ");
  sheetTemp = String(t) + String(" °C");
  Serial.print("Air Quality: "); Serial.println(air_quality);
  sheetairquality = String(air_quality) + String(" PPM");
  Serial.println(millis());

  n_count++; // gets the number of recordings we will need it when getting the average

  static int error_count = 0;
  static int connect_count = 0;
  const unsigned int MAX_CONNECT = 20;
  static bool flag = false;
  if (currmill - prev >= duration){
    Serial.println("the recording has gone 10 minutes");
    Serial.println("Getting the average");

    m_ave = (m/n_count);
    h_ave = (h/n_count);
    t_ave = (t/n_count);

    Serial.println("Average air quality");
    Serial.println(m_ave);
    Serial.println("Average humidity");
    Serial.println(h_ave);
    Serial.println("Average Temperature");
    Serial.println(t_ave);

    payload = payload_base + "\"" + t_ave + "," + h_ave + ","   + m_ave + "\"}";

    if (!flag) {
      client = new HTTPSRedirect(httpsPort);
      client->setInsecure();
      flag = true;
      client->setPrintResponseBody(true);
      client->setContentTypeHeader("application/json");
    }

    if (client != nullptr) {
      if (!client->connected()) {
        client->connect(host, httpsPort);
        client->POST(url2, host, payload, false);
        Serial.print("Sent : ");  Serial.println("Temp, Humid and Air Quality");
      }
    }
    else {
      DPRINTLN("Error creating client object!");
      error_count = 5;
    }

    if (connect_count > MAX_CONNECT) {
      connect_count = 0;
      flag = false;
      delete client;
      return;
    }

    //  Serial.println("GET Data from cell 'A1':");
    //  if (client->GET(url3, host)) {
    //    ++connect_count;
    //  }
    //  else {
    //    ++error_count;
    //    DPRINT("Error-count while connecting: ");
    //    DPRINTLN(error_count);
    //  }

    Serial.println("POST or SEND Sensor data to Google Spreadsheet:");
    if (client->POST(url2, host, payload)) {
      ;
    }
    else {
      ++error_count;
      DPRINT("Error-count while connecting: ");
      DPRINTLN(error_count);
    }

    if (error_count > 3) {
      Serial.println("Halting processor...");
      delete client;
      client = nullptr;
      Serial.printf("Final free heap: %u\n", ESP.getFreeHeap());
      Serial.printf("Final stack: %u\n", ESP.getFreeContStack());
      Serial.flush();
      ESP.deepSleep(0);
    }
    delay(3000);    // keep delay of minimum 2 seconds as dht allow reading after 2 seconds interval and also for google sheet

     // reset the values for the next recording of the average
    m = 0;
    h = 0;
    t = 0;
    n_count = 0;

    // extends the duration for the next reading
    prev = currmill;
  }
  delay(60000); // records the device per minute 
}