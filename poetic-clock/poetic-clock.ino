#include <SPI.h> // SPI communication library for the TFT ILI9136 display
#include <Adafruit_GFX.h> // Adafruit graphic 
#include <TFT_ILI9163C.h> // TFT ILI9136 display

#include <WiFi.h> // WiFi support
#include <OpenAI.h> // OpenAI API client

#include <WiFiUdp.h> // UDP support for the NTP library
#include <NTPClient.h> // NTP library for getting the current date/time

#include <Preferences.h> // We will use the non volatile memory for storing the WiFi SSID, password, API Key and poet adjective

#define DEBUG_ENABLED

// -------------------------
// Non volatile memory data
// -------------------------
// WiFi data
String ssid = "";
String password = "";
// OpenAI key data
String apiKey = "";
// Set the poet adjective
String poetAdjective = "";
// -------------------------

// Configure the TFT display
TFT_ILI9163C tft = TFT_ILI9163C(2, 4); // CS=2, DC=4

// This is the internal soft clock that we will synchronize from the NTP
byte hours = 0, minutes = 0;
bool am = false;

// We will use the preferences object to store the Wifi, API key and poet adjective configuration
Preferences preferences;

void setup() {
  
  // Serial setup at 9600 bauds ...
  Serial.begin(9600);

  // Initialize the preferences
  preferences.begin("data", false);
  // preferences.clear(); // Uncomment to reset the preferences.

  // Initialize the display
  tft.begin();
  tft.setAddrWindow(10, 10, 100, 100);
  tft.clearScreen();
  tft.setRotation(2); // Rotate the screen 180º
  tft.setFont();
  tft.setTextColor(WHITE);  
  tft.setTextSize(1);
  tft.setAddrWindow (20, 20, 100, 100);
  printText("Hi !");
  delay(5000); // Give plenty of time for the Serial config mode.

  // There's serial data, enter the config mode !
  if (Serial.available()) {
    printText("Config mode.");
    // Clear previous preferences
    preferences.clear();
    // Serial input format is: SSID|Password|API key|Poet adjective    
    for (byte i = 0; i < 4; i++) {
      String value = "";
      char c = Serial.read();
      while((c != '|') && (Serial.available())) {
        value.concat(c);
        c = Serial.read();
      }
      if (i == 0) preferences.putString("ssid", value);
      else if (i == 1) preferences.putString("password", value);
      else if (i == 2) preferences.putString("apiKey", value);
      else if (i == 3) preferences.putString("poetAdjective", value);
    }
    delay(1000);
  }

  // Get the Wifi, API Key and poet adjective data from the preferences
  ssid.concat(preferences.getString("ssid", ""));    
  password.concat(preferences.getString("password", ""));    
  apiKey.concat(preferences.getString("apiKey", ""));    
  poetAdjective.concat(preferences.getString("poetAdjective", ""));    

  // If there's no configuration loop for ever :(
  if (ssid == "") { printText("No SSID :("); while(1); } 
  if (password == "") { printText("No Password :("); while(1); } 
  if (apiKey == "") { printText("No API Key :("); while(1); } 
  if (poetAdjective == "") { printText("No Poet adjective :("); while(1); } 

  // Debug the configuration
  #ifdef DEBUG_ENABLED
  Serial.println(ssid);
  Serial.println(password);
  Serial.println(apiKey);
  Serial.println(poetAdjective);
  #endif

  // Connect to WiFi
  printText("Connecting to\nWiFi ...");
  delay(1000);

  char ssidCA[ssid.length() + 1];
  char passwordCA[password.length() + 1];
  ssid.toCharArray(ssidCA, ssid.length() + 1);
  password.toCharArray(passwordCA, password.length() + 1);  
  WiFi.begin(ssidCA, passwordCA);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
  }

  // Initialize the NTP client.
  WiFiUDP wifiUDP;
  NTPClient ntpClient(wifiUDP);
  printText("Getting the\ncurrent time.");
  ntpClient.begin();
  ntpClient.setTimeOffset(-3600 * 3); // Set the time zone, mine is -3UTC
  // Get the current time.
  while(!ntpClient.update()) {
    delay(100);
    ntpClient.forceUpdate();
  }
  delay(1000);

  // Parse the hours and minutes
  String formattedDate = ntpClient.getFormattedDate();
  String time = formattedDate.substring(formattedDate.indexOf("T")+1, formattedDate.length()-4);
  hours = time.substring(0, 2).toInt();
  minutes = time.substring(3, 5).toInt();
  am = false;
  if ((hours >= 0) && (hours <= 11)) am = true;

  // All set !
  printText("Ready :)");
  delay(1000);

  // Debug the hours and minutes
  #ifdef DEBUG_ENABLED
  Serial.println(hours);
  Serial.println(minutes);
  #endif

}

void loop(){

  // Create the Chat Completion object
  char apiKeyCA[apiKey.length() + 1];
  apiKey.toCharArray(apiKeyCA, apiKey.length() + 1);  
  OpenAI openai(apiKeyCA);
  OpenAI_ChatCompletion chatCompletion(openai);

  // Initialize the chat completion
  chatCompletion.setModel("gpt-3.5-turbo");
  chatCompletion.setMaxTokens(1000);
  chatCompletion.setTemperature(0.9);

  while(1) {

    // This is here to check the clock is ticking every 1 minute
    #ifdef DEBUG_ENABLED
    Serial.println("Tick !");
    #endif

    // We will use this variable to implement a precision 1 minute delay
    unsigned long currentMillis = millis();

    // Perform the chat completion
    chatCompletion.clearConversation();
    String currentTime = String(hours) + ":" + String(minutes) + (am ? "AM" : "PM");
    OpenAI_StringResponse result = chatCompletion.message(
      "You are a " + poetAdjective + " poet.\n" + 
      "Write a poem in English with exactly 4 lines to say that the current time is " + currentTime + ".\n", false);

    // The result has at least one completion
    if (result.length() >= 1) {
      // Print the completion.
      String completion = result.getAt(0);
      completion.trim();
      printText(completion);     
      // Debug the current time and completion
      #ifdef DEBUG_ENABLED
      Serial.println(currentTime);
      Serial.println(completion);      
      #endif      
    } else {
      // An error happened, let's give the current time anyway.
      printText("[Error]\nIt's " + currentTime + ".");    
    }

    // Wait here a minute
    while(millis() < currentMillis + (60 * 1000));

    // Increment the soft clock
    minutes++;
    if (minutes >= 60) {
      hours++;
      if (hours > 23) hours = 0;
      minutes = 0;
    }

  }

}

// Print a text in the TFT screen in the given line
void printText(String txt) {
  tft.clearScreen();
  tft.setCursor(0, 5);
  tft.println(txt);
}
