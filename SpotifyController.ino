#include <WiFi.h>
#include <SpotifyEsp32.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <ArduinoJson.h>
#include <string.h>
#include "buttonInterrupt.h"

// spotify api declarations
const TickType_t apiHitInterval = 1500 / portTICK_PERIOD_MS; // api interval, edit here in millis
const char *client_id = "0819e81fd6de4b139db9bc6f5ba69ce0";
const char *client_secret = "e6327880dc594d05bd62bb3ba5bb5bf4";
const char *refresh_token = "AQA821MHkqz4-IR-cgdC__pUxvVD4XDTKhB7-Or9CqkpQ7KntXFMBNWXqfTgLXmraXUL3PK4Qsy2Q88dCX5omKR477Y-4JAZ22Cp48qJQwS6Gt0YJ1qk-agXlsZTNp6IFjY";
Spotify sp(client_id, client_secret, refresh_token);
response r;
TaskHandle_t fetchSp;              // spotify fetch task on core 0
bool apiRequestInProgress = false; // flag for coordinating between spfetch calls and button control calls

// song info variables
struct songInfo
{
    String name;
    String artist;
    unsigned int duration;
    unsigned int progress;
    bool isPlaying;
};
songInfo sp1;                // for core 0 (fetch api task)
songInfo sp2;                // for oled
songInfo lastTrack;          // for updateData() to check if its the same
SemaphoreHandle_t sp1Access; // mutex for sync between spotify fetch and updateData()
bool sameTrack = false;
bool newDataUpdated = false;
bool noContent = true;

// non blocking delay variables for update data function
unsigned long lastUpdate = 0;
unsigned long updateDelay = 1000;
const TickType_t sp1MutexWait = 300 / portTICK_PERIOD_MS;

// oled
#define i2c_Address 0x3c
#define SCREEN_WIDTH 128 // width, in pixels
#define SCREEN_HEIGHT 64 // height, in pixels
#define OLED_RESET -1
Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
// Scrolling text variables
#define firstWordDelay 2000
int scrollX = 0;
unsigned long lastScrollTime = 0;
bool initialDelayDone = false;

// wifi
const char *WIFI_SSID = "B1201A";
const char *WIFI_PASS = "13011301";
void connectToWiFi()
{
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(1000);
    }
    delay(100);
    Serial.println("wifi connected");
}

void setup()
{
    Serial.begin(112500);

    // Display initialisation
    display.begin(i2c_Address, true);
    display.clearDisplay();
    display.setTextColor(SH110X_WHITE);
    display.setTextWrap(false);
    loadAnimation();

    // Button setups
    pinMode(prevB.PIN, INPUT);
    pinMode(pauseB.PIN, INPUT);
    pinMode(nextB.PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(prevB.PIN), prevSP, FALLING);
    attachInterrupt(digitalPinToInterrupt(pauseB.PIN), pauseSP, FALLING);
    attachInterrupt(digitalPinToInterrupt(nextB.PIN), nextSP, FALLING);

    // wifi connect
    connectToWiFi();

    // Spotify API Setup
    sp.begin();
    while (!sp.is_auth())
        sp.handle_client();
    Serial.println("Authenticated");

    // mutex for songInfo struct sp1
    sp1Access = xSemaphoreCreateMutex();

    // spotify fetch task pinned to core 0
    xTaskCreatePinnedToCore(fetchSpotify, "SpotifyRequest", 8000, NULL, 1, &fetchSp, 0);
}
void loop()
{
    updateData();
    buttonControl();
    oledControl();
}

// song info functions
void fetchSpotify(void *parameter) // task function running in infinite loop
{
    for (;;)
    {
        fetchSongInfo();
        vTaskDelay(apiHitInterval);
    }
}
void fetchSongInfo() // spotify api fetch
{
    // if button press is sending req then wait infinitely
    // a blocking delay for core 0 if button press
    while (1)
    {
        if (!apiRequestInProgress)
            break;
        Serial.println("waiting for button press call");
    }

    // filter for requied data
    StaticJsonDocument<200> filter;
    filter["item"]["name"] = true;
    filter["item"]["artists"][0]["name"] = true;
    filter["item"]["duration_ms"] = true;
    filter["progress_ms"] = true;
    filter["is_playing"] = true;

    apiRequestInProgress = true; // set flag for button control
    Serial.println("fetching");
    r = sp.currently_playing(filter);
    apiRequestInProgress = false;

    JsonDocument &doc = r.reply;

    if (isValidResponse(doc)) // if valid response then save data in sp1
    {
        // taking mutex for sp1
        if (xSemaphoreTake(sp1Access, portMAX_DELAY) == pdTRUE)
        {
            sp1.name = doc["item"]["name"].as<String>();
            sp1.artist = doc["item"]["artists"][0]["name"].as<String>(); // only first artist
            sp1.duration = doc["item"]["duration_ms"];
            sp1.progress = doc["progress_ms"];
            sp1.isPlaying = doc["is_playing"];

            if (lastTrack.name == sp1.name) // set flag if current song is same as last song
                sameTrack = true;
            else
                sameTrack = false;

            lastTrack.name = sp1.name;
            newDataUpdated = false; // set flag for newDataUpdated in sp2
            xSemaphoreGive(sp1Access);
        }
        if (noContent) // reset flag when valid response
            noContent = false;
    }
    else // if invalid response
    {
        print_response(r);
    }
}
void updateData() // update variables for oled control
{
    if (xSemaphoreTake(sp1Access, 0) == pdTRUE)
    {
        if (newDataUpdated) // dont update sp2 if there is no new data
        {
            xSemaphoreGive(sp1Access);
            return;
        }
        if (sameTrack) // only update isPlaying and progress state if the track is same
        {
            Serial.println("sameTrack");
            sp2.progress = sp1.progress;
            sp2.isPlaying = sp1.isPlaying;
        }
        else
        {
            sp2.name = sp1.name;
            sp2.artist = sp1.artist;
            sp2.duration = sp1.duration;
            sp2.progress = sp1.progress;
            sp2.isPlaying = sp1.isPlaying;
            Serial.println("DIFFERENT TRACK");
            resetLastScroll();
        }
        newDataUpdated = true; // set flag that newData has been updated
        xSemaphoreGive(sp1Access);
    }
}
bool isValidResponse(JsonDocument &doc) // returns true/false for response
{
    if (r.status_code == 204)
    {
        noContent = true; // for displaying no content
        return false;
    }
    if (r.status_code != 200)
        return false;

    if (!doc.containsKey("is_playing"))
        return false;

    if (!doc.containsKey("progress_ms"))
        return false;

    if (!doc.containsKey("item"))
        return false;

    JsonObject item = doc["item"];
    if (!item.containsKey("artists"))
        return false;

    if (!item.containsKey("duration_ms"))
        return false;

    if (!item.containsKey("name"))
        return false;

    JsonArray artists = item["artists"];
    if (artists.size() == 0)
        return false;

    for (JsonObject artist : artists)
    {
        if (!artist.containsKey("name"))
            return false;
    }
    // All required fields are present and valid
    return true;
}
// button control
void buttonControl() // button presses and api calls for next prev play/pause
{
    if (apiRequestInProgress) // if fetching then return to loop (non block delay for next/pause/prev) cant call 2 api in parallel
        return;

    if (prevB.pressed || pauseB.pressed || nextB.pressed)
    {
        apiRequestInProgress = true;

        if (prevB.pressed)
        {
            prevB.pressed = false;
            Serial.println("prev");
            sp.previous();
        }
        else if (pauseB.pressed)
        {
            pauseB.pressed = false;
            Serial.println("pause");
            if (sp2.isPlaying)
            {
                sp.pause_playback();
            }
            else
            {
                sp.start_resume_playback();
            }
        }
        else
        {
            nextB.pressed = false;
            Serial.println("next");
            sp.skip();
        }
        apiRequestInProgress = false; // set flag for spotify fetch
        resetLastScroll();
    }
}
// oled functions
void oledControl() // display songinfo according to playback conditions
{
    display.clearDisplay();

    // no content in playing (no active session on spotify api)
    if (noContent)
    {
        display.setCursor(0, 15);
        display.setTextSize(2);
        display.print("Play Song");
        display.display();
        resetLastScroll();
    }
    // paused
    else if (!sp2.isPlaying)
    {
        display.setCursor(3, 15);
        display.setTextSize(2);
        display.print("Paused");
        display.display();
        resetLastScroll();
    }
    // playing
    else
    {
        // Constant now playing
        display.setCursor(0, 0);
        display.setTextSize(1);
        display.print("Now Playing:");

        // print songName
        // scroll text
        int nameLength = String(sp2.name).length() * 12;

        // if long song name then scroll it
        if (nameLength > 128)
        {
            int maxScroll = -nameLength + 50; // how much negatve to go when scrolling before reset

            // Draw the text at the current position
            display.setCursor(scrollX, 15);
            display.setTextSize(2);
            display.print(sp2.name);

            // Update text position
            // non blocking delay for first word
            unsigned long currentMillis = millis();
            if (scrollX == 0 && !initialDelayDone)
            {
                if (currentMillis - lastScrollTime >= firstWordDelay)
                {
                    initialDelayDone = true;
                    lastScrollTime = currentMillis;
                }
            }
            else
            {
                scrollX -= 1; // Move left by 1 pixel (adjust speed by changing this value)
                if (scrollX <= maxScroll)
                {
                    scrollX = 0;
                    initialDelayDone = false;
                }
                lastScrollTime = currentMillis;
            }
        }

        // else normal print
        else
        {
            display.setCursor(0, 15);
            display.setTextSize(2);
            display.print(sp2.name);
        }

        // print songArtist
        display.setCursor(0, 35);
        display.setTextSize(1);
        display.print(sp2.artist);

        // calculate progress bar
        progressBar();
        display.display();
    }
}
void progressBar() // calculate progress bar
{
    // bar dimensions
    const int barX = 10;      // X-coordinate of the bar
    const int barY = 52;      // Y-coordinate of the bar
    const int barWidth = 108; // Total width of the bar
    const int barHeight = 7;  // Height of the bar
    const int barRadius = 1;  // Corner radius of the bar

    // hollow outer bar
    display.drawRoundRect(barX, barY, barWidth, barHeight, barRadius, SH110X_WHITE);

    // calculate progress percentage
    if (sp2.duration != 0)
    {
        int progressPercentage = (sp2.progress * 100) / sp2.duration;

        // calculate the width of the filled part
        int filledWidth = (progressPercentage * barWidth) / 100;

        // filled portion
        display.fillRoundRect(barX, barY, filledWidth, barHeight, barRadius, SH110X_WHITE);
    }
}
void resetLastScroll() // resets the scroll condition for long names
{
    scrollX = 0; // reset the last scroll
    initialDelayDone = false;
    lastScrollTime = millis();
}
void loadAnimation()
{
    for (int i = 0; i <= 33; i++)
    {
        display.clearDisplay();

        display.setCursor(20, 20);
        display.setTextSize(1);
        display.print("Spotify Remote");

        display.setCursor(0, 42);
        display.setTextSize(1);
        display.print("Made by Kalrav Mathur");

        display.fillRect(0, -1, 128, 33 - i, SH110X_WHITE);
        display.fillRect(0, 32 + i, 128, 32, SH110X_WHITE);
        display.display();
        delay(50);
    }
}
