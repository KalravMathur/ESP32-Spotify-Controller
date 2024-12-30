#include <WiFi.h>
#include <SpotifyEsp32.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <ArduinoJson.h>
#include <string.h>

// Buttons definations
const int prevPin = 19;      // GPIO for "Previous Song"
const int playPausePin = 18; // GPIO for "Play/Pause"
const int nextPin = 5;       // GPIO for "Next Song"

// oled definations
#define i2c_Address 0x3c
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET -1    //   QT-PY / XIAO
Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Scrolling text variables
#define firstWordDelay 1500
int scrollX = 0;
unsigned long lastScrollTime = 0;
bool initialDelayDone = false;

// wifi creds
const char *WIFI_SSID = "WIFI SSID";
const char *WIFI_PASS = "WIFI PASS";

// spotify declarations
const char *client_id = "CLIENT ID";
const char *client_secret = "CLIENT SECRET";
const char *refresh_token = "REFRESH TOKEN";
Spotify sp(client_id, client_secret, refresh_token);
int pauseCount = 0;
#define pauseCountThreshold 3
#define apiInterval 3000
unsigned long lastAccess = -apiInterval;
response r;

// song info variables
const char *songName;
const char *songArtist;
int duration;
int progress;
bool isPlaying;

void setup()
{
    Serial.begin(112500);

    // Display initialisation
    display.begin(i2c_Address, true);
    display.clearDisplay();
    // text settings
    display.setTextColor(SH110X_WHITE);
    display.setTextWrap(false);

    loadAnimation();

    // Button pins as input
    pinMode(prevPin, INPUT);
    pinMode(playPausePin, INPUT);
    pinMode(nextPin, INPUT);

    // wifi connect
    connectToWiFi();

    // Spotify API Setup
    sp.begin();
    while (!sp.is_auth())
        sp.handle_client();
    // Serial.println("Authenticated");

    delay(50);
    resetLastScroll();
}
void loop()
{
    // fetch song info with a non blocking delay
    unsigned long currentMillis = millis();
    if (currentMillis - lastAccess > apiInterval)
    {
        fetchSongInfo();
        lastAccess = currentMillis;
    }
    buttonControl();
    oledControl();
}

// function declarations
void connectToWiFi()
{
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(1000);
    }
    delay(100);
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
void resetLastScroll()
{
    scrollX = 0; // reset the last scroll
    lastScrollTime = millis();
    initialDelayDone = false;
}
void fetchSongInfo()
{
    // getting response from spotify
    r = sp.currently_playing();

    // Extracting data
    JsonDocument &doc = r.reply;
    songName = doc["item"]["name"];
    songArtist = doc["item"]["artists"][0]["name"]; // only first artist
    duration = doc["item"]["duration_ms"];
    progress = doc["progress_ms"];
    isPlaying = doc["is_playing"];
}
void buttonControl()
{
    // prev song
    if (digitalRead(prevPin) == HIGH)
    {
        sp.previous();
        fetchSongInfo();
        resetLastScroll();
    }

    // play/pause toggle
    else if (digitalRead(playPausePin) == HIGH)
    {
        if (isPlaying)
            sp.pause_playback();
        else
            sp.start_resume_playback();
        fetchSongInfo();
        resetLastScroll();
    }

    // next song
    else if (digitalRead(nextPin) == HIGH)
    {
        sp.skip();
        fetchSongInfo();
        resetLastScroll();
    }
}
void oledControl()
{
    // update screen only when valid response
    if (r.status_code == 200)
    {
        // paused music (only hits after pause count hits threshold otherwise continues)
        if (!isPlaying && pauseCount > pauseCountThreshold)
        {
            display.clearDisplay();
            display.setCursor(3, 15);
            display.setTextSize(2);
            display.print("Paused");
            display.display();
            resetLastScroll();
        }

        else
        {
            if (!isPlaying) // if getting not playing then increment pause count
                pauseCount++;
            else
                pauseCount = 0;

            display.clearDisplay();

            // Constant now playing
            display.setCursor(0, 0);
            display.setTextSize(1);
            display.print("Now Playing:");

            // print songName
            // scroll text
            int nameLength = String(songName).length() * 12;

            // if long song name then scroll it
            if (nameLength > 128)
            {
                int maxScroll = -nameLength + 50;

                // Draw the text at the current position
                display.setCursor(scrollX, 15);
                display.setTextSize(2);
                display.print(songName);

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
                display.print(songName);
            }

            // print songArtist
            display.setCursor(0, 35);
            display.setTextSize(1);
            display.print(songArtist);

            // calculate progress bar
            progressBar();
            display.display();
        }
    }
}
void progressBar()
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
    if (duration != 0)
    {
        int progressPercentage = (progress * 100) / duration;

        // calculate the width of the filled part
        int filledWidth = (progressPercentage * barWidth) / 100;

        // filled portion
        display.fillRoundRect(barX, barY, filledWidth, barHeight, barRadius, SH110X_WHITE);
    }
}
