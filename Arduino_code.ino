#include <GxEPD2_BW.h>
#include <Fonts/FreeMonoBold24pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TimeLib.h>

#define ENABLE_GxEPD2_GFX 0

#define uS_TO_S_FACTOR 1000000ULL /* Conversion factor for micro seconds to seconds */


// ESP32 e-paper display wiring
GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display(
  GxEPD2_154_D67(/*CS=*/ 5, /*DC=*/ 4, /*RES=*/ 2, /*BUSY=*/ 15)); // 200x200 e-paper

// WiFi credentials
const char* ssid = "BND_extv2";
const char* password = "VanilleeismitErdbeeren";

// API endpoint
const char* url = "http://192.168.178.51:8050/getOutputData";


struct PowerData {
    int currentPower;
    float powerToday;
    float powerTotal;
};

// Coordinates for Berlin, Germany
const char* latitude = "52.5200";  // Latitude for Berlin
const char* longitude = "13.4050"; // Longitude for Berlin


struct PowerDataStrings {
// Global variables to store formatted power data with example values
  char currentPowerStr[4];
  char powerTodayStr[15];
  char powerUntilTodayStr[15];
};

// Global variables to store the sunrise and sunset times
time_t sunset_tm;
time_t api_time_now;
time_t sunrise_tm;

bool time_is_synced = false;

RTC_DATA_ATTR int bootCount = 0;


void setup()
{
  Serial.begin(115200);
  display.init(115200, true, 50, false);
  display.setRotation(1);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
  }
  Serial.println("Connected!");
  getSunriseSunset(latitude, longitude);

  bool time_is_synced = false; // Define this variable before using it

  if (timeStatus() != timeSet || !time_is_synced) {
      Serial.println("Initiating time sync...");
      APICallCurrentTime();
      time_is_synced = true; // Remove 'bool' here since you've already declared it
      if (timeStatus() == timeSet) { // Add parentheses around the condition
          Serial.println("Time sync successfull.");
      } else { // Add space between } and else
          Serial.println("Time sync was not successfull.");
      }
  } else { // Add space between } and else
      Serial.println("Time is sync!");
  }
}

void APICallCurrentTime() {
  time_t current_time_tm;
  HTTPClient http;
  String url = "https://timeapi.io/api/time/current/zone?timeZone=Europe%2FAmsterdam";
  delay(5000);
  http.begin(url);
  int httpCode = http.GET();

  if (httpCode == 200) {
    String payload = http.getString();
    Serial.println("API Response:");
    Serial.println(payload);

    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);

    String currentTimeStr = doc["dateTime"].as<String>();

    current_time_tm = convertISO8601ToTimeT(currentTimeStr);

    Serial.print("Current time: ");
    Serial.println(current_time_tm);

  } else {
    Serial.println("Error on HTTP request for current time");
  }

  http.end();
  setTime(current_time_tm);
}


time_t convertISO8601ToTimeT(String dateTime) {
  // Extract the date and time components from the string (format: "2025-02-27T05:54:39+00:00")
  int year = dateTime.substring(0, 4).toInt();
  int month = dateTime.substring(5, 7).toInt();
  int day = dateTime.substring(8, 10).toInt();

  int hour = dateTime.substring(11, 13).toInt();
  int minute = dateTime.substring(14, 16).toInt();
  int second = dateTime.substring(17, 19).toInt();

  // Create tmElements_t structure
  tmElements_t tm;
  tm.Year = CalendarYrToTm(year);  // Convert year to tm format (e.g., 2025 to 125)
  tm.Month = month;
  tm.Day = day;
  tm.Hour = hour;
  tm.Minute = minute;
  tm.Second = second;

  // Convert to time_t
  return makeTime(tm);
}

// Helper function to convert time_t to "hh:mm" format
String timeToHHMM(time_t timestamp) {
  // Convert timestamp to time structure
  struct tm *timeInfo = localtime(&timestamp);

  // Extract hours and minutes
  int hour = timeInfo->tm_hour;
  int minute = timeInfo->tm_min;

  // Format the string with leading zeros
  String timeString = "";

  // Add leading zero for hours if needed
  if (hour < 10) {
    timeString += "0";
  }
  timeString += String(hour);

  // Add colon
  timeString += ":";

  // Add leading zero for minutes if needed
  if (minute < 10) {
    timeString += "0";
  }
  timeString += String(minute);

  return timeString;
}




void getSunriseSunset(const char* lat, const char* lon) {
  HTTPClient http;
  String url = "https://api.sunrise-sunset.org/json?lat=" + String(lat) + "&lng=" + String(lon) + "&formatted=0";

  http.begin(url);
  int httpCode = http.GET();

  if (httpCode == 200) {
    String payload = http.getString();
    Serial.println("API Response:");
    Serial.println(payload);

    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);

    String sunriseTimeStr = doc["results"]["sunrise"].as<String>();
    String sunsetTimeStr = doc["results"]["sunset"].as<String>();

    sunset_tm = convertISO8601ToTimeT(sunsetTimeStr);
    sunrise_tm = convertISO8601ToTimeT(sunriseTimeStr);

    Serial.print("Sunrise: ");
    Serial.println(sunrise_tm);
    Serial.print("Sunset: ");
    Serial.println(sunset_tm);

  } else {
    Serial.println("Error on HTTP request on sunset times");
  }
  http.end();
}

PowerData apiCallWechselrichter()
{
  PowerData api_data;
  api_data.currentPower = 999;
  api_data.powerToday = 999.9;
  api_data.powerTotal = 999.9;
  if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(url);
        int httpResponseCode = http.GET();

        if (httpResponseCode > 0) {
            String response = http.getString();
            Serial.println("Response received:");
            Serial.println(response);

            // Parse JSON
            StaticJsonDocument<512> doc;
            DeserializationError error = deserializeJson(doc, response);

            if (!error) {
                if (doc.containsKey("data")) {
                    api_data.currentPower = doc["data"]["p1"].as<int>() + doc["data"]["p2"].as<int>();
                    api_data.powerToday  = doc["data"]["e1"].as<float>() + doc["data"]["e2"].as<float>();
                    api_data.powerTotal = doc["data"]["te1"].as<float>() + doc["data"]["te2"].as<float>();


                } else {
                    Serial.println("No data found, response is empty!");
                }
            } else {
                Serial.println("JSON parsing failed!");
            }
        } else {
            Serial.printf("Connection failed to Wechselrichter. HTTP error code: %d\n", httpResponseCode);
        }
        http.end();
    } else {
        Serial.println("WiFi not connected!");
    }
  Serial.printf("Current power: %d | Power today: %.2f | Total Power until today: %.2f\n",
              api_data.currentPower, api_data.powerToday, api_data.powerTotal);
  return api_data;
}

PowerDataStrings convertPowerValuesToStrings(PowerData data)
{
    PowerDataStrings data_strings ;
    char tempStr[7];  // Single temporary buffer for both numbers

    // Convert and format current_power (max 3 digits)
    sprintf(data_strings.currentPowerStr, "%d", data.currentPower);

    // Convert and format power_today (max 99.9)
    dtostrf(data.powerToday, 5, 1, tempStr);
    sprintf(data_strings.powerTodayStr, "Today: %s", tempStr);

    // Convert and format power_until_today (max 9999.9)
    dtostrf(data.powerTotal, 5, 1, tempStr);
    sprintf(data_strings.powerUntilTodayStr, "Total: %s", tempStr);

    return data_strings;
}

void displayNumber(PowerDataStrings datastrings)
{
    display.setTextColor(GxEPD_BLACK);
    display.setFont(&FreeMonoBold24pt7b);
    display.setTextSize(2.1);

    // Define dimensions
    int maxWidth = display.width();
    int maxHeight = (display.height() * 2 / 3);

    // Get current_power_str bounds
    int16_t tbx, tby;
    uint16_t tbw, tbh;
    display.getTextBounds(datastrings.currentPowerStr, 0, 0, &tbx, &tby, &tbw, &tbh);

    // Center text in the upper 2/3 area
    uint16_t x = (maxWidth - tbw - 14) / 2; //14 = manuell offset to center
    uint16_t y = (maxHeight - tbh) / 2 + tbh -10; //10 = manuell offset

    // Use Partial Window for better performance
    display.setPartialWindow(0, y - tbh+5, maxWidth, tbh);
    display.firstPage();
    do
    {
        display.fillRect(0, y - tbh, maxWidth, tbh, GxEPD_WHITE); // Clear full line
        display.setCursor(x, y);
        display.print(datastrings.currentPowerStr);

    }
    while (display.nextPage());

    // Add two lines of text in the lower 1/3
    display.setFont(&FreeMonoBold12pt7b);
    display.setTextSize(1);

    // Set the y positions absolutely for both lines
    uint16_t y1 = 135;
    uint16_t y2 = 165;

    // Get text bounds for the lower lines
    int16_t tbx1, tby1;
    uint16_t tbw1, tbh1;
    display.getTextBounds(datastrings.powerTodayStr, 0, 0, &tbx1, &tby1, &tbw1, &tbh1);
    display.getTextBounds(datastrings.powerUntilTodayStr, 0, 0, &tbx1, &tby1, &tbw1, &tbh1);
    // Calculate center positions for the lower lines (centered horizontally)
    uint16_t x1 = (display.width() - tbw1) / 2;
    uint16_t x2 = (display.width() - tbw1) / 2;

    display.setPartialWindow(0, y1 - tbh1, display.width(), y2 + tbh1 - (y1 - tbh1));
    display.firstPage();
    do
    {
        display.fillRect(0, y1 - tbh1, display.width(), tbh1, GxEPD_WHITE);
        display.setCursor(x1, y1);
        display.print(datastrings.powerTodayStr);

        display.fillRect(0, y2 - tbh1, display.width(), tbh1, GxEPD_WHITE);
        display.setCursor(x2, y2);
        display.print(datastrings.powerUntilTodayStr);
    }
    while (display.nextPage());

    // Get current time and format it
    time_t currentTime = now();
    String timeStr = timeToHHMM(currentTime);

    // Setup for time display at bottom
    display.setFont(&FreeMonoBold9pt7b);
    int16_t tbx3, tby3;
    uint16_t tbw3, tbh3;
    display.getTextBounds(timeStr, 0, 0, &tbx3, &tby3, &tbw3, &tbh3);
    uint16_t x3 = (display.width() - tbw3) / 2;
    uint16_t y3 = 195;

    display.setPartialWindow(0, y3 - tbh3, display.width(), tbh3);
    display.firstPage();
    do
    {
        display.fillRect(0, y3 - tbh3, display.width(), tbh3, GxEPD_WHITE);
        display.setCursor(x3, y3);
        display.print(timeStr);
    }
    while (display.nextPage());

    display.hibernate();  // Save power
}

void loop() {
    //in seconds
    long timetosleep = 300;

    // // Call API to get the power data
    PowerData data = apiCallWechselrichter();

    PowerDataStrings datastrings = convertPowerValuesToStrings(data);

    // Display the power values on the e-paper display
    displayNumber(datastrings);

    if (sunset_tm != 0 && sunrise_tm != 0) {
        Serial.print("Entering Sunset Loop... \n");

        time_t currentTime = now();

        if (currentTime > sunset_tm) {
            timetosleep = (24 * 3600) - (sunset_tm - sunrise_tm);
            Serial.printf("Sun is set, sleeping %d until sunrise ...\n", timetosleep);
            bool time_is_synced = false;
        }else{
          Serial.printf("The sun is still up, sleeping %d seconds...\n", timetosleep);
        }

  esp_sleep_enable_timer_wakeup(timetosleep * uS_TO_S_FACTOR);
  Serial.println("Going to sleep now");
  Serial.flush();
  esp_deep_sleep_start();
    }
}
