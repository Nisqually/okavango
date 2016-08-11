#include "Adafruit_FONA.h"
#include <SoftwareSerial.h>
#include "Platforms.h"

#define PIN_FONA_RX         9
#define PIN_FONA_TX         8
#define PIN_FONA_RST        4

// #define FONA_DRIVER_USB_SERIAL

SoftwareSerial fonaSerial(PIN_FONA_TX, PIN_FONA_RX);
Adafruit_FONA fona(PIN_FONA_RST);
bool available = false;

class Buffer {
private:
    char buffer[256];
    size_t length;

public:
    Buffer() : length(0) {
        memzero(buffer, sizeof(buffer));
    }

    void append(char c) {
        if (length < sizeof(buffer) - 1) {
            buffer[length++] = c;
            buffer[length] = 0;
        }
    }

    const char *c_str() {
        return buffer;
    }

    void clear() {
        length = 0;
    }
};

Buffer buffer;

#ifdef FONA_DRIVER_USB_SERIAL
Serial_ &commandSerial = Serial;
#else
HardwareSerial &commandSerial = Serial1;
#endif

void setup() {
    fonaSerial.begin(4800);
    Serial1.begin(4800);
    #ifdef FONA_DRIVER_USB_SERIAL
    Serial.begin(115200);
    #endif

    if (!fona.begin(fonaSerial)) {
        #ifdef FONA_DRIVER_USB_SERIAL
        Serial.println(F("No FONA"));
        #endif
        available = false;
    }
    else {
        available = true;
    }
}

void fona_echo_type() {
    uint8_t type = fona.type();
    commandSerial.print(F("+TYPE,"));
    commandSerial.print(type);
    commandSerial.print(F(","));
    switch (type) {
        case FONA800L:   commandSerial.print(F("FONA 800L")); break;
        case FONA800H:   commandSerial.print(F("FONA 800H")); break;
        case FONA808_V1: commandSerial.print(F("FONA 808 (v1)")); break;
        case FONA808_V2: commandSerial.print(F("FONA 808 (v2)")); break;
        case FONA3G_A:   commandSerial.print(F("FONA 3G (American)")); break;
        case FONA3G_E:   commandSerial.print(F("FONA 3G (European)")); break;
        default:         commandSerial.print(F("?")); break;
    }
    commandSerial.print(F("\r"));
}

bool handle(String command) {
    if (command.startsWith("~HELLO")) {
        commandSerial.print(F("OK\r"));
    }
    else if (command.startsWith("~POWER")) {
        if (!fona.begin(fonaSerial)) {
            commandSerial.print(F("ER\r"));
        }
        else {
            fona_echo_type();

            char imei[15] = { 0 };
            uint8_t length = fona.getIMEI(imei);
            if (length > 0) {
                commandSerial.print(F("+IMEI: "));
                commandSerial.print(imei);
                commandSerial.print(F("\r"));
                commandSerial.print(F("OK\r"));
            }
            else {
                commandSerial.print(F("ER\r"));
            }
        }
    }
    else if (command.startsWith("~TYPE")) {
        fona_echo_type();
        commandSerial.print(F("OK\r"));
    }
    else if (command.startsWith("~STATUS")) {
        uint8_t n = fona.getNetworkStatus();
        commandSerial.print(F("+STATUS,"));
        commandSerial.print(n);
        commandSerial.print(F(","));
        if (n == 0) commandSerial.print(F("Not registered"));
        if (n == 1) commandSerial.print(F("Registered (home)"));
        if (n == 2) commandSerial.print(F("Not registered (searching)"));
        if (n == 3) commandSerial.print(F("Denied"));
        if (n == 4) commandSerial.print(F("Unknown"));
        if (n == 5) commandSerial.print(F("Registered roaming"));
        commandSerial.print(F("\r"));
        commandSerial.print(F("OK\r"));
    }
    else if (command.startsWith("~SMS")) {
        commandSerial.print(F("+TRY\r"));
        String end = command.substring(5);
        end.trim();
        uint32_t i = end.indexOf(' ');
        String number = end.substring(0, i);
        String message = end.substring(i + 1);

        commandSerial.print(F("+NUMBER="));
        commandSerial.print(number);
        commandSerial.print(F("\r"));

        commandSerial.print(F("+MESSAGE="));
        commandSerial.print(message);
        commandSerial.print(F("\r"));

        if (fona.sendSMS((char *)number.c_str(), (char *)message.c_str())) {
            commandSerial.print(F("OK\r"));
        }
        else {
            commandSerial.print(F("ER\r"));
        }
    }
    else if (command.startsWith("~OFF")) {
        commandSerial.print(F("OK\r"));
    }
    else if (command.startsWith("~ON")) {
        commandSerial.print(F("OK\r"));
    }
    else {
        commandSerial.print(F("+UNKNOWN '"));
        commandSerial.print(command);
        commandSerial.print(F("' "));
        commandSerial.print(command.length());
        commandSerial.print(F("\r"));
        commandSerial.print(F("ER\r"));
    }

    return true;
}

void loop() {
    delay(10);

    if (commandSerial.available()) {
        int16_t c = (int16_t)commandSerial.read();
        if (c < 0) {
            return;
        }

        if (c == '\r') {
            handle(buffer.c_str());

            #ifdef FONA_DRIVER_USB_SERIAL
            Serial.println();
            #endif
            buffer.clear();
        }
        else {
            buffer.append((char)c);
        }
    }
}

// vim: set ft=cpp:
