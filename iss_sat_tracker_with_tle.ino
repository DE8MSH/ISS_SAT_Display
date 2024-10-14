// TODO: ZOMM OF MAP ON SAT > minel°

#include <Sgp4.h>
#include <Ticker.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <TFT_eSPI.h>
#include <PNGdec.h>
#include "map.h"
#include "cedens.h"

struct Satellite {
  Sgp4 sat;
  const char* tle_url;
  uint16_t color;
  double satLon;
  double satLat;
  double satAz;
  double satEl;
  char name[30];
};

Ticker tkSecond;  // Ticker für regelmäßige Aufrufe
unsigned long unixtime;  // Unix-Zeit
int timezone = 2;  // Zeitzone (UTC +2)
int framerate;  // Framerate für die Berechnung
int elapsedSeconds = 0;  // Zähler für die vergangenen Sekunden

int year, mon, day, hr, minute;  // Variablen zur Zeitverwaltung
double sec;

PNG png;  // PNG-Dekoder
#define TFT_GREY 0x303030  // Definierter Grauton
#define MAX_IMAGE_WIDTH 320  // Maximale Bildbreite
int16_t xpos = 0;  // X-Position für das Zeichnen
int16_t ypos = 0;  // Y-Position für das Zeichnen
int minel = 30;

WiFiUDP ntpUDP;  // UDP-Objekt für NTP
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 0);  // NTP-Client zur Zeitsynchronisation

TFT_eSPI tft = TFT_eSPI();  // TFT-Objekt für die Anzeige

Satellite satellites[] = {

  //{ Sgp4(), "https://celestrak.org/NORAD/elements/gp.php?INTDES=2005-018&FORMAT=tle", TFT_BLUE },
  //{ Sgp4(), "https://celestrak.org/NORAD/elements/gp.php?INTDES=1998-030&FORMAT=tle", TFT_BLUE },
  //{ Sgp4(), "https://celestrak.org/NORAD/elements/gp.php?INTDES=2009-005&FORMAT=tle", TFT_BLUE },
  { Sgp4(), "https://celestrak.org/NORAD/elements/gp.php?CATNR=27607&FORMAT=tle", TFT_YELLOW }, // SO50
  { Sgp4(), "https://celestrak.org/NORAD/elements/gp.php?CATNR=7530&FORMAT=tle", TFT_YELLOW }, // OSCAR 7
  { Sgp4(), "https://celestrak.org/NORAD/elements/gp.php?CATNR=43017&FORMAT=tle", TFT_YELLOW }, // FOX1B
  { Sgp4(), "https://celestrak.org/NORAD/elements/gp.php?CATNR=23439&FORMAT=tle", TFT_YELLOW }, // RS15
  { Sgp4(), "https://celestrak.org/NORAD/elements/gp.php?CATNR=22825&FORMAT=tle", TFT_YELLOW }, // EYESAT APRS
  //{ Sgp4(), "https://celestrak.org/NORAD/elements/gp.php?CATNR=40069&FORMAT=tle", TFT_BLUE },
  { Sgp4(), "https://celestrak.org/NORAD/elements/gp.php?CATNR=48274&FORMAT=tle", TFT_RED }, // China Raumstation
  { Sgp4(), "https://live.ariss.org/iss.txt", TFT_RED }, // ISS
  
};

const int numSatellites = sizeof(satellites) / sizeof(satellites[0]);  // Anzahl der Satelliten

char namesWithElevation[300]; // Adjust size as needed

// Funktion zum Abrufen der TLE-Daten (Two-Line Element) für die Satelliten
void fetchTLE() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;  
    for (int i = 0; i < numSatellites; i++) {
      int attempt = 0;
      bool success = false;
      while (attempt < 3 && !success) {  // Retry up to 3 times
        http.begin(satellites[i].tle_url);
        int httpResponseCode = http.GET();  
        if (httpResponseCode > 0) {
          String payload = http.getString();  
          char satname[30];
          char tle_line1[70];
          char tle_line2[70];
          sscanf(payload.c_str(), "%[^\n]\n%[^\n]\n%[^\n]", satname, tle_line1, tle_line2);
          satellites[i].sat.init(satname, tle_line1, tle_line2);
          strncpy(satellites[i].name, satname, sizeof(satellites[i].name) - 1);
          tft.print("> ");tft.println(satellites[i].name);
          success = true;  // Mark as successful
          // Log epoch time
        } else {
          Serial.println("Fehler beim Abrufen des TLE für Satelliten " + String(i) + ": " + String(httpResponseCode));
          tft.println("Fehler beim Abrufen des TLE für Satelliten " + String(i) + ": " + String(httpResponseCode));
          attempt++;
          delay(1000);  // Wait before retrying
        }
        http.end();  
      }
    }
  } else {
    Serial.println("WiFi nicht verbunden");
  }
}


// Funktion zum Entfernen aufeinanderfolgender Leerzeichen im Satellitennamen
void removeConsecutiveSpaces(char* name) {
  int srcIndex = 0;  // Quellindex
  int destIndex = 0;  // Zielindex
  bool lastWasSpace = false;  // Flag, um zu überprüfen, ob das letzte Zeichen ein Leerzeichen war

  while (name[srcIndex] != '\0' && destIndex < sizeof(satellites[0].name) - 1) {
    if (name[srcIndex] != ' ') {
      name[destIndex++] = name[srcIndex];  // Nicht-Leerzeichen-Zeichen kopieren
      lastWasSpace = false;  // Flag zurücksetzen
    } else if (!lastWasSpace) {
      name[destIndex++] = ' ';  // Ein einzelnes Leerzeichen kopieren
      lastWasSpace = true;  // Flag setzen
    }
    srcIndex++;
  }
  if (name[destIndex] == ' ') {
    name[destIndex - 2] = '\0';  // Null-Terminierung
  } else {
    name[destIndex] = '\0';  // Null-Terminierung
  }
}

// Funktion zum Zeichnen der Satelliten auf dem Bildschirm
void drawSatellites() {
  tft.fillScreen(TFT_BLACK);  // Bildschirm schwarz füllen
  int16_t rc = png.openFLASH((uint8_t*)karte, sizeof(karte), pngDraw);  // PNG-Bild laden
  if (rc == PNG_SUCCESS) {
    tft.startWrite();
    rc = png.decode(NULL, 0);  // PNG dekodieren
    tft.endWrite();
    png.close();  // PNG schließen
  }

  for (int i = 0; i < numSatellites; i++) {
    int x_sat = map(satellites[i].satLon, -180, 180, 0, 320);  // X-Position berechnen
    int y_sat = map(satellites[i].satLat, 90, -90, 0, 240);  // Y-Position berechnen
    char nameWithoutSpaces[30];
    strncpy(nameWithoutSpaces, satellites[i].name, sizeof(nameWithoutSpaces) - 1);  // Satellitennamen kopieren
    removeConsecutiveSpaces(nameWithoutSpaces);  // Leerzeichen entfernen
    tft.setCursor(x_sat - (strlen(nameWithoutSpaces) * 3), y_sat + 9);  // Cursorposition setzen
    tft.setTextColor(satellites[i].color, TFT_BLACK);  // Textfarbe setzen
    tft.setTextColor(TFT_WHITE, TFT_BLACK);  // Textfarbe setzen
    tft.setTextSize(1);  // Schriftgröße setzen
    tft.print(nameWithoutSpaces);  // Satellitennamen drucken
    //tft.fillCircle(x_sat, y_sat, 5, satellites[i].color);  // Satellitenposition zeichnen
    //tft.drawCircle(x_sat, y_sat, 24, TFT_WHITE);  // Außenkreis zur Hervorhebung zeichnen
  }

  for (int i = 0; i < numSatellites; i++) {
    int x_sat = map(satellites[i].satLon, -180, 180, 0, 320);  // X-Position berechnen
    int y_sat = map(satellites[i].satLat, 90, -90, 0, 240);  // Y-Position berechnen

    tft.fillCircle(x_sat, y_sat, 5, satellites[i].color);  // Satellitenposition zeichnen
    tft.drawCircle(x_sat, y_sat, 6, TFT_BLUE);  // Außenkreis zur Hervorhebung zeichnen
    //tft.drawCircle(x_sat, y_sat, 24, TFT_WHITE);  // Außenkreis zur Hervorhebung zeichnen
  }


  int siteX = map(8.893867310691851, -180, 180, 0, 320);  // X-Position des Standorts berechnen
  int siteY = map(53.04543841406317, 90, -90, 0, 240);  // Y-Position des Standorts berechnen
  tft.fillCircle(siteX, siteY, 5, TFT_GREEN);  // Standort zeichnen
  tft.drawCircle(siteX, siteY, 6, TFT_BLACK);  // Außenkreis zur Hervorhebung zeichnen
  tft.drawCircle(siteX, siteY, 7, TFT_BLACK);  // Außenkreis zur Hervorhebung zeichnen
}

// Funktion zum Zeichnen von PNG-Daten auf dem Bildschirm
void pngDraw(PNGDRAW* pDraw) {
  uint16_t lineBuffer[MAX_IMAGE_WIDTH];  // Puffer für die Bilddaten
  png.getLineAsRGB565(pDraw, lineBuffer, PNG_RGB565_BIG_ENDIAN, 0xffffffff);  // Bildzeile abrufen
  tft.pushImage(xpos, ypos + pDraw->y, pDraw->iWidth, 1, lineBuffer);  // Bild auf den TFT übertragen
}

// Funktion, die jede Sekunde aufgerufen wird, um die Satellitenposition zu aktualisieren
void Second_Tick() {
  unixtime += 1;  // Unix-Zeit erhöhen
    namesWithElevation[0] = '\0';  // Clear the buffer

  for (int i = 0; i < numSatellites; i++) {
    invjday(satellites[i].sat.satJd, timezone, true, year, mon, day, hr, minute, sec);  // Julianisches Datum umwandeln
    satellites[i].sat.findsat(unixtime);  // Satellitenposition berechnen
    satellites[i].satLon = satellites[i].sat.satLon;  // Longitude speichern
    satellites[i].satLat = satellites[i].sat.satLat;  // Latitude speichern
    satellites[i].satAz = satellites[i].sat.satAz;  // Azimut speichern
    satellites[i].satEl = satellites[i].sat.satEl;  // Elevation speichern

    if (satellites[i].satEl >= minel) {
      // Append the satellite name to the buffer
      strncat(namesWithElevation, satellites[i].name, sizeof(namesWithElevation) - strlen(namesWithElevation) - 1);
      //strncat(namesWithElevation, " ", sizeof(namesWithElevation) - strlen(namesWithElevation) - 1);  // Add comma and space
    }

    Serial.println("Satellit " + String(i) + ": Azimut = " + String(satellites[i].satAz) + " Elevation = " + String(satellites[i].satEl));
  }

  Serial.println();
  framerate = 0;  // Framerate zurücksetzen

  elapsedSeconds++;  // Vergangene Sekunden erhöhen
  if (elapsedSeconds >= 30) {
    drawSatellites();  // Satelliten zeichnen
    displaySatelliteNames();
    elapsedSeconds = 0;  // Zähler zurücksetzen
  }
}
// Function to display satellite names on the screen
void displaySatelliteNames() {
  // Set position for names
  tft.setCursor(0, 0);  // Cursor position
  tft.setTextColor(TFT_WHITE, TFT_BLACK);  // Set text color
  tft.setTextSize(1);  // Set text size
  //tft.println("Satellites with Elevation >= 1°:");
  tft.println(namesWithElevation);  // Print names
}
// Setup-Funktion, die einmal beim Start aufgerufen wird
void setup() {
  tft.begin();  // TFT initialisieren
  tft.setRotation(1);  // Bildschirmrotation setzen
  tft.fillScreen(TFT_BLACK);  // Bildschirm schwarz füllen
  tft.println("TFT ok. Rot ok. Schwarzer Hintergrund ok.");
  tft.println("");
  tft.setTextSize(2);  // Schriftgröße setzen
  tft.println("ISS / SAT Display");
  tft.println("By DO7OO and ChatGPT");
  tft.setTextSize(1);  // Schriftgröße setzen
  tft.println("");
  tft.println("Seriell ok.");
  tft.println("WiFi verbinden...");  // WiFi-Verbindung versuchen
  WiFi.begin(ssid, password);  // WiFi mit SSID und Passwort starten
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);  // Warten, bis die Verbindung hergestellt ist
    tft.print("*");  // Stern anzeigen
  }
  tft.println("");
  tft.println("WiFi ok.");  // Verbindung hergestellt
  timeClient.begin();  // NTP-Client starten
  tft.println("Zeit-Client ok.");
  timeClient.update();  // Zeit aktualisieren
  tft.println("Zeit-Client aktualisiert ok.");
  unixtime = timeClient.getEpochTime();  // Aktuelle Zeit abrufen

  satellites[0].sat.site(53.04543841406317, 8.893867310691851, 9);  // Standort für den Satelliten setzen

  tft.println("TLE abrufen...");  // TLE-Daten abrufen
  fetchTLE();  // TLE-Daten abrufen
  tft.println("TLE ok.");

  tkSecond.attach(1, Second_Tick);  // Ticker für die Funktion setzen
  tft.println("Tick ok.");
  tft.println("");
  tft.println("Bitte warten...");
}

// Hauptschleife
void loop() {
  // framerate += 1;  // Framerate erhöhen
   delay(1000);
}
