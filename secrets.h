// secrets.h
// =============================================================
// Put your real WiFi and ThingSpeak credentials in THIS file only.
// Add "secrets.h" to your .gitignore so it never gets committed
// or shared alongside the sketch.
//
// In Arduino IDE / PlatformIO, this file sits next to your .ino
// and gets #include'd — but stays out of git.
// =============================================================

#ifndef SECRETS_H
#define SECRETS_H

// ----- WiFi -----
const char* WIFI_SSID     = "mazx";
const char* WIFI_PASSWORD = "mazx.1910";   // <-- change this password too, it's been shared in plaintext already

// ----- ThingSpeak -----
// Use your NEW, rotated Read API key here (not the one shared earlier).
const char* THINGSPEAK_CHANNEL_ID = "3236956";
const char* THINGSPEAK_READ_API_KEY = "PUT_YOUR_ROTATED_READ_KEY_HERE";

#endif
