#include "Platforms.h"
#include "AtlasScientific.h"
#include "SerialPortExpander.h"

SerialPortExpander portExpander(PORT_EXPANDER_SELECT_PIN_0, PORT_EXPANDER_SELECT_PIN_1, PORT_EXPANDER_SELECT_PIN_2);

AtlasScientificBoard board;

void finish() {
    portExpander.select(3);

    while (true) {
        delay(1000);
    }
}

void setup() {
    Serial.begin(115200);
    #ifdef WAIT_FOR_SERIAL
    while (!Serial) {
        delay(100);
        if (millis() > WAIT_FOR_SERIAL) {
            break;
        }
    }
    #else
    delay(3000);
    #endif

    Serial.println("Begin");

    portExpander.setup();
    portExpander.select(0);

    board.setSerial(&portExpanderSerial);
    board.start();

    platformPostSetup();

    Serial.println("Loop");
}

void loop() {
    board.tick();
    delay(50);

    if (board.isDone()) {
        byte newPort = portExpander.getPort() + 1;
        portExpander.select(newPort);
        if (newPort < 3) {
            Serial.println("Next sensor");
            board.start();
        }
        else if (newPort == 3) {
            Serial.println("Conductivity");
            board.setSerial(&conductivitySerial);
            board.start(OPEN_CONDUCTIVITY_SERIAL_ON_START);
        }
        else {
            Serial.println("Done");
            finish();
        }
    }
}

// vim: set ft=cpp:
