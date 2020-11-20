#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Preferences.h>
#include <U8g2lib.h> 
#include <MFRC522.h>
#include <EEPROM.h> 
#include "ESP32_MailClient.h"


const char* ssid = "WLAN";
const char* password = "xxx";

#define emailSenderAccount    "example@example.com"    
#define emailSenderPassword   "xxx"
#define emailRecipient        "example@example.com"
#define smtpServer            "mail0.example.com"
#define smtpServerPort        465
#define emailSubject          "Honigverkauf"

#define isDebug           1
#define VERSION           "0.0.1"

#define MODE_RUN          1
#define MODE_SETTINGS     0

#define SELECT_PEGEL      HIGH
#define LOCK_PEGEL        LOW

#define MAIN_MENU_MAX     4
#define SUBJECT_MENU_MAX  3
#define RFID_MENU_MAX     3

#define MAX_RETRIES       5

// Taster
#define BUTTON_SUBJECT_1  32
#define BUTTON_SUBJECT_2  33
#define BUTTON_SUBJECT_3  34

// Taster Beleuchtung
#define LED_SUBJECT_1     12
#define LED_SUBJECT_2     13
#define LED_SUBJECT_3     14

// Schloss öffner
#define OPEN_LOCK_1       25
#define OPEN_LOCK_2       26
#define OPEN_LOCK_3       27

// Schloss Kontakt
#define CLOSE_LOCK_1      35
#define CLOSE_LOCK_2      36
#define CLOSE_LOCK_3      39

// Münzzähler
#define COINAGE_COUNTER   5
#define COINAGE_LOCK      17

// RFID
#define RFID_SDA          21
#define RFID_SCK          18
#define RFID_MOSI         23
#define RFID_MISO         19
#define RFID_RST          22


// OLED fuer Heltec WiFi Kit 32 (ESP32 onboard OLED)
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, /* clock=*/ 15, /* data=*/ 4, /* reset=*/ 16);
MFRC522 mfrc522(RFID_SDA, RFID_RST);
MFRC522::MIFARE_Key key;

Preferences preferences;
SMTPData smtpData;

void sendCallback(SendStatus info);


int i_main;
int i_sub;
long checksum;
int menuPosition = 0;
int menuPositionMax = MAIN_MENU_MAX;

int currentMode = MODE_RUN;

int subject = 0;
int payed = 0;
int price = 0;
int pulseinv = 0;
int retries = 0;

float priceSubject1;
float priceSubject2;
float priceSubject3;

int quantitySubject1;
int quantitySubject2;
int quantitySubject3;

int mastercardSet;

byte readcard[4];
byte mastercard[4];
byte card1[4];
byte card2[4];
byte card3[4];

uint8_t successRead;


void getPreferences(void) {
  preferences.begin("EEPROM", false);

  priceSubject1     = preferences.getFloat("priceSubject1", 0.0);
  priceSubject2     = preferences.getFloat("priceSubject2", 0.0);
  priceSubject3     = preferences.getFloat("priceSubject3", 0.0);
  quantitySubject1  = preferences.getInt("quantity1", 0);
  quantitySubject2  = preferences.getInt("quantity2", 0);
  quantitySubject3  = preferences.getInt("quantity3", 0);
  mastercardSet     = preferences.getInt("mastercardSet", 0);
  for ( uint8_t j = 0; j < 4; j++ ) {   
    mastercard[j]   = preferences.getInt("mastercard_"+j, 0);
    card1[j]        = preferences.getInt("card1_"+j, 0);
    card2[j]        = preferences.getInt("card2_+j", 0);
    card3[j]        = preferences.getInt("card3_+j", 0);
  }
  
  checksum = priceSubject1 + priceSubject2 + priceSubject3 + quantitySubject1 + quantitySubject2 + quantitySubject3 + mastercardSet;
  
  preferences.end();

#ifdef isDebug
  Serial.println("Read Preferences:");
  Serial.print("Price Subject 1: ");   Serial.print(priceSubject1);
  Serial.print("Price Subject 2: ");   Serial.print(priceSubject2);
  Serial.print("Price Subject 3: ");   Serial.print(priceSubject3);
  Serial.print("Quantity Subject 1: ");   Serial.print(quantitySubject1);
  Serial.print("Quantity Subject 2: ");   Serial.print(quantitySubject2);
  Serial.print("Quantity Subject 3: ");   Serial.print(quantitySubject3);
  Serial.print("Mastercard: ");   Serial.print(mastercardSet);
#endif
}

void setPreferences(void) {
  long checksum_check;
  checksum_check = priceSubject1 + priceSubject2 + priceSubject3 + quantitySubject1 + quantitySubject2 + quantitySubject3 + mastercardSet;
  
  if (checksum == checksum_check) {
#ifdef isDebug
    Serial.println("Preferences unverändert");
#endif
    return;
  }

  checksum = checksum_check;
  preferences.begin("EEPROM", false);

  preferences.putFloat("price1", priceSubject1);
  preferences.putFloat("price2", priceSubject2);
  preferences.putFloat("price3", priceSubject3);
  preferences.putInt("quantity1", quantitySubject1);
  preferences.putInt("quantity2", quantitySubject2);
  preferences.putInt("quantity3", quantitySubject3);
  
  preferences.putInt("mastercardSet", mastercardSet);
  for ( uint8_t j = 0; j < 4; j++ ) {   
    preferences.putInt("mastercard_"+j, mastercard[j]);
    preferences.putInt("card1_"+j, card1[j]);
    preferences.putInt("card2_"+j, card2[j]);
    preferences.putInt("card3_"+j, card3[j]);
  }

  preferences.end();

#ifdef isDebug
  Serial.println("Save Preferences:");
  Serial.print("Price Subject 1: ");   Serial.print(priceSubject1);
  Serial.print("Price Subject 2: ");   Serial.print(priceSubject2);
  Serial.print("Price Subject 3: ");   Serial.print(priceSubject3);
  Serial.print("Quantity Subject 1: ");   Serial.print(quantitySubject1);
  Serial.print("Quantity Subject 2: ");   Serial.print(quantitySubject2);
  Serial.print("Quantity Subject 3: ");   Serial.print(quantitySubject3);
  Serial.print("Mastercard: ");   Serial.print(mastercardSet);
#endif
}

void processRun(void) {

  u8g2.clearBuffer();
  payed = 0;
  subject = 0;
  
  if (quantitySubject1 <= 0 && quantitySubject2 <= 0 && quantitySubject3 <= 0){
      u8g2.setFont(u8g2_font_courB18_tf);
      u8g2.setCursor(10, 27);
      u8g2.print("Fächer");
      u8g2.setCursor(10, 64);
      u8g2.print("leer");
      u8g2.sendBuffer();
      digitalWrite(LED_SUBJECT_1, LOW);
      digitalWrite(LED_SUBJECT_2, LOW);
      digitalWrite(LED_SUBJECT_3, LOW);
      digitalWrite(OPEN_LOCK_1, LOW);
      digitalWrite(OPEN_LOCK_2, LOW);
      digitalWrite(OPEN_LOCK_3, LOW);
      subject = 0;

  
      do {
        successRead = getID();
      }
      while (!successRead);
      if (readcard[4] == mastercard[4] || readcard[4] == card1[4] || readcard[4] == card2[4] || readcard[4] == card3[4]){
        currentMode = MODE_SETTINGS;
      }
      delay(1000);
      successRead = 0;
    
  } else {
    if (digitalRead(CLOSE_LOCK_1) != LOCK_PEGEL){
      u8g2.setFont(u8g2_font_courB10_tf);
      u8g2.setCursor(10, 27);
      u8g2.print("Fach 1");
      u8g2.setCursor(10, 64);
      u8g2.print("schließen");
      u8g2.sendBuffer();
      subject = 0;
    } else if (digitalRead(CLOSE_LOCK_2) != LOCK_PEGEL) {
      u8g2.setFont(u8g2_font_courB10_tf);
      u8g2.setCursor(10, 27);
      u8g2.print("Fach 2");
      u8g2.setCursor(10, 64);
      u8g2.print("schließen");
      u8g2.sendBuffer();
    } else if (digitalRead(CLOSE_LOCK_3) != LOCK_PEGEL) {
      u8g2.setFont(u8g2_font_courB10_tf);
      u8g2.setCursor(10, 27);
      u8g2.print("Fach 3");
      u8g2.setCursor(10, 64);
      u8g2.print("schließen");
      u8g2.sendBuffer();
    } else {
      subject = 0;
      u8g2.setFont(u8g2_font_courB14_tf);
      u8g2.setCursor(10, 27);
      u8g2.print("Bitte Fach");
      u8g2.setCursor(10, 64);
      u8g2.print("wählen");
      u8g2.sendBuffer();
  
      if (digitalRead(BUTTON_SUBJECT_1) == SELECT_PEGEL){
        delay(250);
        while(digitalRead(BUTTON_SUBJECT_1) == SELECT_PEGEL ) {
        }
        #ifdef isDebug 
            Serial.print("Fach: 1");
        #endif

        subject = 1;
      }
  
      if (digitalRead(BUTTON_SUBJECT_2) == SELECT_PEGEL){
        delay(250);
        while(digitalRead(BUTTON_SUBJECT_2) == SELECT_PEGEL ) {
        }
        #ifdef isDebug 
            Serial.print("Fach: 2");
        #endif
        subject = 2;
      }
  
      if (digitalRead(BUTTON_SUBJECT_3) == SELECT_PEGEL){
        delay(250);
        while(digitalRead(BUTTON_SUBJECT_3) == SELECT_PEGEL ) {
        }
        #ifdef isDebug 
            Serial.print("Fach: 3");
        #endif
        subject = 3;
      }


      if (subject != 0){
        doPayment();
      }
      
    }
  }
}

void doPayment(void){
  int quantity = 0;
  
  digitalWrite(LED_SUBJECT_1, LOW);
  digitalWrite(LED_SUBJECT_2, LOW);
  digitalWrite(LED_SUBJECT_3, LOW);

  switch (subject){
    case 1:
      quantity = quantitySubject1;
      price = priceSubject1 * 100;
      digitalWrite(LED_SUBJECT_1, HIGH);
      break;
    case 2:
      quantity = quantitySubject2;
      price = priceSubject2 * 100;
      digitalWrite(LED_SUBJECT_2, HIGH);
      break;
    case 3:
      quantity = quantitySubject3;
      price = priceSubject3 * 100;
      digitalWrite(LED_SUBJECT_3, HIGH);
      break;
  }
         
  if (quantity <= 0){
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_courB10_tf);
    u8g2.setCursor(10, 10);
    switch (subject){
      case 1:
        u8g2.print("Fach 1 leer");
        break;
      case 2:
        u8g2.print("Fach 2 leer");
        break;
      case 3:
        u8g2.print("Fach 3 leer");
        break;
    }
    u8g2.setCursor(10, 23);
    u8g2.print("andere");
    u8g2.setCursor(10, 36);
    u8g2.print("Auswahl");
    u8g2.sendBuffer();
    delay(2000);
  } else {
    digitalWrite(COINAGE_LOCK, HIGH);
    while (payed < price){
      if (digitalRead(COINAGE_COUNTER == LOW)){
        delay(45);
      }
      pulseinv = digitalRead(COINAGE_COUNTER);
      payed = payed + (pulseinv * 50);
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_courB10_tf);
      u8g2.setCursor(10, 10);
      u8g2.print("Preis :");
      u8g2.print((float) price / 100);
      u8g2.print("€");
      u8g2.setCursor(10, 23);
      u8g2.print("noch");
      u8g2.setCursor(10, 36);
      u8g2.print((float) (price - payed) / 100);
      u8g2.setCursor(10, 49);
      u8g2.print("einwerfen");
      u8g2.sendBuffer();
      
      if (digitalRead(BUTTON_SUBJECT_1) == SELECT_PEGEL){
        delay(250);
        while(digitalRead(BUTTON_SUBJECT_1) == SELECT_PEGEL ) {
        }
        subject = 1;
        price = priceSubject1 * 100;
        digitalWrite(LED_SUBJECT_1, LOW);
        digitalWrite(LED_SUBJECT_2, LOW);
        digitalWrite(LED_SUBJECT_3, LOW);
        digitalWrite(LED_SUBJECT_1, HIGH);
        break;
      }
  
      if (digitalRead(BUTTON_SUBJECT_2) == SELECT_PEGEL){
        delay(250);
        while(digitalRead(BUTTON_SUBJECT_2) == SELECT_PEGEL ) {
        }
        subject = 2;
        price = priceSubject2 * 100;
        digitalWrite(LED_SUBJECT_1, LOW);
        digitalWrite(LED_SUBJECT_2, LOW);
        digitalWrite(LED_SUBJECT_3, LOW);
        digitalWrite(LED_SUBJECT_2, HIGH);
        break;
      }
  
      if (digitalRead(BUTTON_SUBJECT_3) == SELECT_PEGEL){
        delay(250);
        while(digitalRead(BUTTON_SUBJECT_3) == SELECT_PEGEL ) {
        }
        subject = 3;
        price = priceSubject3 * 100;
        digitalWrite(LED_SUBJECT_1, LOW);
        digitalWrite(LED_SUBJECT_2, LOW);
        digitalWrite(LED_SUBJECT_3, LOW);
        digitalWrite(LED_SUBJECT_3, HIGH);
        break;
      }
    }
    
    if (payed >= price){
      digitalWrite(COINAGE_LOCK, LOW);
      
      switch (subject){
        case 1:
          digitalWrite(OPEN_LOCK_1, HIGH);
          delay(200);
          digitalWrite(OPEN_LOCK_1, LOW);
          digitalWrite(LED_SUBJECT_1, LOW);
          quantitySubject1--;
          setPreferences();
          sendEmail();
          break;
        case 2:
          digitalWrite(OPEN_LOCK_2, HIGH);
          delay(200);
          digitalWrite(OPEN_LOCK_2, LOW);
          digitalWrite(LED_SUBJECT_2, LOW);
          quantitySubject2--;
          setPreferences();
          sendEmail();
          break;
        case 3:
          digitalWrite(OPEN_LOCK_3, HIGH);
          delay(200);
          digitalWrite(OPEN_LOCK_3, LOW);
          digitalWrite(LED_SUBJECT_3, LOW);
          quantitySubject3--;
          setPreferences();
          sendEmail();
          break;
      }
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_courB14_tf);
      u8g2.setCursor(25, 27);
      u8g2.print("Vielen Dank");
      u8g2.setCursor(25, 64);
      u8g2.print("Guten Appetit");
      u8g2.sendBuffer();
      delay(3000);
      payed = 0;
      subject = 0;
    }
  }
  if (subject == 0) exit;
}

void sendEmail(void){
  if (WiFi.status() != WL_CONNECTED){
    retries = 0;
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED && MAX_RETRIES > retries) {
      Serial.print(".");
      retries++;
      delay(200);
    }
  }
  if (WiFi.status() == WL_CONNECTED){
    smtpData.setLogin(smtpServer, smtpServerPort, emailSenderAccount, emailSenderPassword);
    smtpData.setSTARTTLS(true);
    smtpData.setSender("Honigautomat", emailSenderAccount);
    smtpData.setPriority("High");
    smtpData.setSubject(emailSubject);
    smtpData.setMessage("Honigglasverkauf", false);
    smtpData.addRecipient(emailRecipient);
    if (!MailClient.sendMail(smtpData)) Serial.println("Error sending Email, " + MailClient.smtpErrorReason());
    smtpData.empty();
  }
}

void setupRFID(void){
  menuPositionMax = RFID_MENU_MAX;
  u8g2.setFont(u8g2_font_courB10_tf);
  i_sub = 1;
  while (i_sub > 0) {
    getMenuPosition();
    
    if (mastercardSet != 143){
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_courB08_tf);
      u8g2.setCursor(10, 10);
      u8g2.print("Ersteinrichtung");
      u8g2.setCursor(10, 36);
      u8g2.print("Mastercard");
      u8g2.setCursor(10, 49);
      u8g2.print("vorhalten");
      u8g2.sendBuffer();
      do {
        successRead = getID();
      }
      while (!successRead);
      for ( uint8_t j = 0; j < 4; j++ ) {
        mastercard[j] = readcard[j];
      }
      mastercardSet = 143;
      successRead = 0;
      setPreferences();
    } else {
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_courB10_tf);
      u8g2.setCursor(10, 10);
      u8g2.print("Karte 1");
      u8g2.setCursor(10, 23);
      u8g2.print("Karte 2");
      u8g2.setCursor(10, 36);
      u8g2.print("Karte 3");
      u8g2.setCursor(10, 49);
      u8g2.print("Beenden");
    
      u8g2.setFont(u8g2_font_courB10_tf);
      u8g2.setCursor(0, 10 + menuPosition * 13);
      u8g2.print("*");
      u8g2.sendBuffer();

      
      if (digitalRead(BUTTON_SUBJECT_2) == SELECT_PEGEL){
        delay(250);
        while(digitalRead(BUTTON_SUBJECT_2) == SELECT_PEGEL ) {
        }
        #ifdef isDebug 
            Serial.print("Setup Position: ");
            Serial.println(menuPosition);
        #endif

        if (menuPosition != 3){
            u8g2.clearBuffer();
            u8g2.setFont(u8g2_font_courB10_tf);
            u8g2.setCursor(25, 27);
            u8g2.print("Karte");
            u8g2.setCursor(25, 64);
            u8g2.print("vorhalten");
            u8g2.sendBuffer();
        }

        switch(menuPosition){
          case 0:
            do {
              successRead = getID();
            }
            while (!successRead);
            for ( uint8_t j = 0; j < 4; j++ ) {
              card1[j] = readcard[j];
            }
            successRead = 0;
            break;
          case 1:
            do {
              successRead = getID();
            }
            while (!successRead);
            for ( uint8_t j = 0; j < 4; j++ ) {
              card2[j] = readcard[j];
            }
            successRead = 0;
            break;
          case 2:
            do {
              successRead = getID();
            }
            while (!successRead);
            for ( uint8_t j = 0; j < 4; j++ ) {
              card3[j] = readcard[j];
            }
            successRead = 0;
            break;
          case 3:
            menuPosition = 0;
            menuPositionMax = MAIN_MENU_MAX;
            subject = 0;
            setPreferences();
            u8g2.clearBuffer();
            u8g2.setFont(u8g2_font_courB10_tf);
            u8g2.setCursor(10, 10);
            u8g2.print("OK");
            u8g2.sendBuffer();
            delay(1000);
            i_sub = 0;
            break;
        }
      }
    }
  }
}

uint8_t getID() {
  if ( ! mfrc522.PICC_IsNewCardPresent()) {
    return 0;
  }
  if ( ! mfrc522.PICC_ReadCardSerial()) {
    return 0;
  }
  #ifdef isDebug 
    Serial.println(F("Scanned PICC's UID:"));
  #endif
  for ( uint8_t j = 0; j < 4; j++) {
    readcard[j] = mfrc522.uid.uidByte[j];
    #ifdef isDebug 
      Serial.print(readcard[j], HEX);
    #endif
  }
  #ifdef isDebug 
    Serial.println("");
  #endif
  mfrc522.PICC_HaltA();
  return 1;
}

void processSetup(void) {
  if (currentMode != MODE_SETTINGS){
    currentMode = MODE_SETTINGS;
    menuPosition = 0;
    menuPositionMax = MAIN_MENU_MAX;
    subject = 0;
  }
  
  digitalWrite(LED_SUBJECT_1, LOW);
  digitalWrite(LED_SUBJECT_2, LOW);
  digitalWrite(LED_SUBJECT_3, LOW);

  getMenuPosition();
  
  u8g2.setFont(u8g2_font_courB10_tf);
  u8g2.clearBuffer();
  u8g2.setCursor(10, 10);
  u8g2.print("Fach 1");
  u8g2.setCursor(10, 23);
  u8g2.print("Fach 2");
  u8g2.setCursor(10, 36);
  u8g2.print("Fach 3");
  u8g2.setCursor(10, 49);
  u8g2.print("RFID");
  u8g2.setCursor(10, 62);
  u8g2.print("Beenden");

  u8g2.setFont(u8g2_font_courB10_tf);
  u8g2.setCursor(0, 10 + menuPosition * 13);
  u8g2.print("*");
  u8g2.sendBuffer();
  
  if (digitalRead(BUTTON_SUBJECT_2) == SELECT_PEGEL){
    delay(250);
    while(digitalRead(BUTTON_SUBJECT_2) == SELECT_PEGEL ) {
    }
    #ifdef isDebug 
        Serial.print("Setup Position: ");
        Serial.println(menuPosition);
    #endif

    if (menuPosition == 3){
      getPreferences();
      menuPosition = 0;
      menuPositionMax = MAIN_MENU_MAX;
      subject = 0;
      setupRFID();
    } else if (menuPosition < menuPositionMax) {
      subject = menuPosition + 1;
      menuPosition = 0;
      menuPositionMax = SUBJECT_MENU_MAX;
      setupSubject();
    } else {
      getPreferences();
      currentMode = MODE_RUN;
      menuPosition = 0;
      menuPositionMax = MAIN_MENU_MAX;
      subject = 0;
    }
  }
}

void setup() {

  pinMode(LED_BUILTIN, OUTPUT);

  Serial.begin(115200);
  while (!Serial) {
  }

  #ifdef isDebug
    Serial.println("Start");
  #endif

  SPI.begin();
  // Init MFRC522
  mfrc522.PCD_Init(); 

  // interne pull downs Taser
  pinMode(BUTTON_SUBJECT_1, INPUT_PULLDOWN);
  pinMode(BUTTON_SUBJECT_2, INPUT_PULLDOWN);
  pinMode(BUTTON_SUBJECT_3, INPUT_PULLDOWN);

  // LED output
  pinMode(LED_SUBJECT_1, OUTPUT);
  pinMode(LED_SUBJECT_2, OUTPUT);
  pinMode(LED_SUBJECT_3, OUTPUT);
  digitalWrite(LED_SUBJECT_1, LOW);
  digitalWrite(LED_SUBJECT_2, LOW);
  digitalWrite(LED_SUBJECT_3, LOW);

  // open lock output
  pinMode(OPEN_LOCK_1, OUTPUT);
  pinMode(OPEN_LOCK_2, OUTPUT);
  pinMode(OPEN_LOCK_3, OUTPUT);
  digitalWrite(OPEN_LOCK_1, LOW);
  digitalWrite(OPEN_LOCK_2, LOW);
  digitalWrite(OPEN_LOCK_3, LOW);

  // interne pull downs close lock
  pinMode(CLOSE_LOCK_1, INPUT_PULLDOWN);
  pinMode(CLOSE_LOCK_2, INPUT_PULLDOWN);
  pinMode(CLOSE_LOCK_3, INPUT_PULLDOWN);

  // Münzzähler
  pinMode(COINAGE_COUNTER, INPUT_PULLDOWN);
  pinMode(COINAGE_LOCK, OUTPUT);
  digitalWrite(COINAGE_LOCK, LOW);

  delay(100);

  // LOAD SETTINGS
  getPreferences();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED && MAX_RETRIES > retries) {
    Serial.print(".");
    retries++;
    delay(200);
  }

  // Boot Screen
  u8g2.begin();
  u8g2.enableUTF8Print();
  u8g2.clearBuffer();
  print_boot();
  delay(3000);
}

void loop(){
  if (mastercardSet != 143){
    currentMode = MODE_SETTINGS;
  }
  
  if (currentMode == MODE_SETTINGS) {
    processSetup();
  }
  
  if (currentMode == MODE_RUN) {
    processRun();
  }
}

void print_boot(void) {
  u8g2.setFont(u8g2_font_courB18_tf);
  u8g2.setCursor(25, 27);
  u8g2.print("BOOT");
  u8g2.setFont(u8g2_font_courB08_tf);
  u8g2.setCursor(25, 64);
  u8g2.print(VERSION);
  u8g2.sendBuffer();
}


void getMenuPosition(void){
  
  if (digitalRead(BUTTON_SUBJECT_1) == SELECT_PEGEL){
    delay(250);
    while(digitalRead(BUTTON_SUBJECT_1) == SELECT_PEGEL ) {
    }
    if (menuPosition == 1) menuPosition = menuPositionMax;
    else if (menuPosition > 1) menuPosition--;
    #ifdef isDebug 
        Serial.print("Setup Position: ");
        Serial.println(menuPosition);
    #endif
  }

  if (digitalRead(BUTTON_SUBJECT_3) == SELECT_PEGEL){
    delay(250);
    while(digitalRead(BUTTON_SUBJECT_3) == SELECT_PEGEL ) {
    }
    if (menuPosition == menuPositionMax) menuPosition = 0;
    else if (menuPosition < menuPositionMax) menuPosition++;
    #ifdef isDebug 
        Serial.print("Setup Position: ");
        Serial.println(menuPosition);
    #endif
  }
}

void setupSubject(){
  
    menuPositionMax = SUBJECT_MENU_MAX;
    switch (subject){
      case 1:
        digitalWrite(LED_SUBJECT_1, HIGH);
        break;
      case 2:
        digitalWrite(LED_SUBJECT_2, HIGH);
        break;
      case 3:
        digitalWrite(LED_SUBJECT_3, HIGH);
        break;
    }

    i_main = 1;
    while (i_main > 0) {
      
      getMenuPosition();
  
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_courB10_tf);
      u8g2.setCursor(10, 10);
      u8g2.print("Preis");
      u8g2.setCursor(10, 23);
      u8g2.print("Bestand");
      u8g2.setCursor(10, 36);
      u8g2.print("Öffnen");
      u8g2.setCursor(10, 49);
      u8g2.print("Beenden");
      
      u8g2.setCursor(0, 10 + menuPosition * 13);
      u8g2.print("*");
      u8g2.sendBuffer();
  
      if (digitalRead(BUTTON_SUBJECT_2) == SELECT_PEGEL){
        delay(250);
        while(digitalRead(BUTTON_SUBJECT_2) == SELECT_PEGEL ) {
        }
        #ifdef isDebug 
            Serial.print("Setup Position: ");
            Serial.println(menuPosition);
        #endif
    
        if (menuPosition == 0){
          setupPrice();
        }
        if (menuPosition == 1){
          setupQuantity();
        }
        if (menuPosition == 2){
          switch (subject){
            case 1:
              digitalWrite(LED_SUBJECT_1, LOW);
              digitalWrite(OPEN_LOCK_1, HIGH);
              delay(500);
              digitalWrite(LED_SUBJECT_1, HIGH);
              digitalWrite(OPEN_LOCK_1, LOW);
              break;
            case 2:
              digitalWrite(LED_SUBJECT_2, LOW);
              digitalWrite(OPEN_LOCK_2, HIGH);
              delay(500);
              digitalWrite(LED_SUBJECT_2, HIGH);
              digitalWrite(OPEN_LOCK_2, LOW);
              break;
            case 3:
              digitalWrite(LED_SUBJECT_3, LOW);
              digitalWrite(OPEN_LOCK_3, HIGH);
              delay(500);
              digitalWrite(LED_SUBJECT_3, HIGH);
              digitalWrite(OPEN_LOCK_3, LOW);
              break;
          }
        }
        if (menuPosition == 3){
          menuPosition = 0;
          menuPositionMax = MAIN_MENU_MAX;
          subject = 0;
          u8g2.clearBuffer();
          u8g2.setFont(u8g2_font_courB18_tf);
          u8g2.setCursor(25, 27);
          u8g2.print("OK");
          u8g2.sendBuffer();
          delay(1000);
          i_main = 0;
        }
      }
    
    }
}

void setupPrice(void) {
    float displayPosition = 0.0;
    menuPositionMax = 999;
    switch (subject){
      case 1:
        menuPosition = priceSubject1 * 10;
        break;
      case 2:
        menuPosition = priceSubject2 * 10;
        break;
      case 3:
        menuPosition = priceSubject3 * 10;
        break;
    }

    i_sub = 1;
    while (i_sub > 0) {
      
      getMenuPosition();

      displayPosition = (float) menuPosition / 10;
      
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_courB18_tf);
      u8g2.setCursor(25, 27);
      u8g2.print(displayPosition);
      u8g2.sendBuffer();
  
      if (digitalRead(BUTTON_SUBJECT_2) == SELECT_PEGEL){
        delay(250);
        while(digitalRead(BUTTON_SUBJECT_2) == SELECT_PEGEL ) {
        }
        #ifdef isDebug 
            Serial.print("Setup Position: ");
            Serial.println(menuPosition);
        #endif
        
        switch (subject){
          case 1:
            priceSubject1 = (float) menuPosition / 10;
            break;
          case 2:
            priceSubject2 = (float) menuPosition / 10;
            break;
          case 3:
            priceSubject3 = (float) menuPosition / 10;
            break;
        }
        setPreferences();
        menuPosition = 0;
        menuPositionMax = SUBJECT_MENU_MAX;
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_courB18_tf);
        u8g2.setCursor(25, 27);
        u8g2.print("OK");
        u8g2.sendBuffer();
        delay(1000);
        i_sub = 0;
      }
    }
}

void setupQuantity(void) {
    menuPositionMax = 99;
    switch (subject){
      case 1:
        menuPosition = quantitySubject1;
        break;
      case 2:
        menuPosition = quantitySubject2;
        break;
      case 3:
        menuPosition = quantitySubject3;
        break;
    }

    i_sub = 1;
    while (i_sub > 0) {
      
      getMenuPosition();
      
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_courB18_tf);
      u8g2.setCursor(25, 27);
      u8g2.print(menuPosition);
      u8g2.sendBuffer();
  
      if (digitalRead(BUTTON_SUBJECT_2) == SELECT_PEGEL){
        delay(250);
        while(digitalRead(BUTTON_SUBJECT_2) == SELECT_PEGEL ) {
        }
        #ifdef isDebug 
            Serial.print("Setup Position: ");
            Serial.println(menuPosition);
        #endif
        
        switch (subject){
          case 1:
            quantitySubject1 = menuPosition;
            break;
          case 2:
            quantitySubject2 = menuPosition;
            break;
          case 3:
            quantitySubject3 = menuPosition;
            break;
        }
        setPreferences();
        menuPosition = 0;
        menuPositionMax = SUBJECT_MENU_MAX;
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_courB18_tf);
        u8g2.setCursor(25, 27);
        u8g2.print("OK");
        u8g2.sendBuffer();
        delay(1000);
        i_sub = 0;
      }
    }
}
