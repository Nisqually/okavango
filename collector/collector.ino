#include "Platforms.h"
#include "core.h"
#include "protocol.h"
#include "network.h"

int32_t freeRam() {
    extern int __heap_start, *__brkval; 
    int v; 
    return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}

void setup() {
    platformLowPowerSleep(LOW_POWER_SLEEP_BEGIN);

    Serial.begin(115200);

    #ifdef WAIT_FOR_SERIAL
    while (!Serial) {
        delay(100);
        if (millis() > WAIT_FOR_SERIAL) {
            break;
        }
    }
    #endif

    Serial.println(F("Begin"));
    Serial.println(freeRam());

    CorePlatform corePlatform;
    corePlatform.setup();

    platformPostSetup();

    Serial.println(F("Loop"));
}

void loop() {
    Queue queue;
    LoraRadio radio(PIN_RFM95_CS, PIN_RFM95_INT, PIN_RFM95_RST);
    NetworkProtocolState networkProtocol(NetworkState::EnqueueFromNetwork, &radio, &queue);

    Serial.println(F("Begin"));
    Serial.println(freeRam());

    if (radio.setup()) {
        radio.sleep();
    }

    uint32_t last = 0;
    while (1) {
        networkProtocol.tick();
        delay(10);

        if (millis() - last > 1000) {
            DEBUG_PRINTLN(queue.size());
            last = millis();
        }
    }
}

// vim: set ft=cpp:
