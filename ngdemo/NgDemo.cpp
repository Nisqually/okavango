#include "NgDemo.h"
#include <IridiumSBD.h>

NgDemo::NgDemo()
    : gps(&Serial1) {
}

bool NgDemo::setup() {
    platformSerial2Begin(9600);
    Serial1.begin(9600);

    state = NgDemoState::WaitingGpsFix;
    stateChangedAt = millis();

    return true;
}

class WatchdogCallbacks : public IridiumCallbacks  {
public:
    virtual void tick() override {
        Watchdog.reset();
    }
};

void NgDemo::failPreflight(uint8_t kind) {
    Watchdog.reset();

    logPrinter.flush();

    digitalWrite(PIN_RED_LED, LOW);

    while (true) {
        for (uint8_t i = 0; i < kind; ++i) {
            digitalWrite(PIN_RED_LED, HIGH);
            delay(250);
            digitalWrite(PIN_RED_LED, LOW);
            delay(250);
        }
        delay(1000);
    }
}

bool NgDemo::configure() {
    gps.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
    gps.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);
    gps.sendCommand(PGCMD_ANTENNA);

    return true;
}

bool NgDemo::preflight() {
    DEBUG_PRINTLN("preflight: Start");

    digitalWrite(PIN_RED_LED, HIGH);

    // Check RockBlock
    rockBlockSerialBegin();
    IridiumSBD rockBlock(Serial2, PIN_ROCK_BLOCK, new WatchdogCallbacks());
    if (Serial) {
        rockBlock.attachConsole(Serial);
        rockBlock.attachDiags(Serial);
    }
    else {
        rockBlock.attachConsole(logPrinter);
        rockBlock.attachDiags(logPrinter);
    }
    rockBlock.setPowerProfile(1);
    if (rockBlock.begin(10) != ISBD_SUCCESS) {
        DEBUG_PRINTLN("preflight: RockBlock failed");
        failPreflight(1);
    }

    DEBUG_PRINTLN("preflight: RockBlock good");

    // Check Sensor
    DHT dht(PIN_DHT, DHT22);
    dht.begin();
    if (dht.readHumidity() == NAN || dht.readTemperature() == NAN) {
        DEBUG_PRINTLN("preflight: Sensors failed");
        failPreflight(2);
    }

    DEBUG_PRINTLN("preflight: DHT22 good");

    // Check GPS
    uint32_t startTime = millis();
    uint32_t queryTime = millis();

    while (true) {
        delay(10);

        while (Serial1.available()) {
            gps.read();
        }

        if (millis() - startTime > STATE_MAX_PREFLIGHT_GPS_FIX_TIME) {
            DEBUG_PRINTLN("preflight: GPS failed");
            failPreflight(3);
        }
        else if (millis() - queryTime > 2000) {
            gps.sendCommand(PMTK_Q_RELEASE);
        }

        if (gps.newNMEAreceived()) {
            const char *nmea = gps.lastNMEA();
            if (strstr(nmea, "$PMTK705") >= 0) {
                break;
            }
        }

        Watchdog.reset();
    }

    DEBUG_PRINTLN("preflight: GPS good");

    DEBUG_PRINTLN("preflight: Passed");

    digitalWrite(PIN_RED_LED, LOW);

    return true;
}

void NgDemo::tick() {
    switch (state) {
    case NgDemoState::WaitingGpsFix: {
        Watchdog.reset();
        if (checkGps()) {
            DEBUG_PRINTLN("GPS: Fix");
            state = NgDemoState::ReadingSensors;
            logPrinter.flush();
        }
        else if (millis() - stateChangedAt > STATE_MAX_GPS_FIX_TIME) {
            DEBUG_PRINTLN("GPS: No Fix");
            state = NgDemoState::ReadingSensors;
            logPrinter.flush();
        }
        break;
    }
    case NgDemoState::ReadingSensors: {
        DEBUG_PRINTLN("ReadingSensors:");

        DHT dht(PIN_DHT, DHT22);
        dht.begin();
        humidity = dht.readHumidity();
        temperature = dht.readTemperature();
        batteryLevel = platformBatteryLevel();

        DEBUG_PRINTLN(humidity);
        DEBUG_PRINTLN(temperature);
        DEBUG_PRINTLN(batteryLevel);

        state = NgDemoState::Transmitting;
        stateChangedAt = millis();
        logPrinter.flush();
        break;
    }
    case NgDemoState::Transmitting: {
        uint32_t uptime = millis() / (1000 * 60);
        String message(SystemClock->now());
        message += ",";
        message += "NGD";
        message += ",";
        message += latitude;
        message += ",";
        message += longitude;
        message += ",";
        message += altitude;
        message += ",";
        message += temperature;
        message += ",";
        message += humidity;
        message += ",";
        message += batteryLevel;
        message += ",";
        message += uptime;
        transmission(message);

        state = NgDemoState::Sleep;
        stateChangedAt = millis();
        logPrinter.flush();
        break;
    }
    case NgDemoState::Sleep: {
        Watchdog.reset();
        if (millis() - stateChangedAt > STATE_MAX_SLEEP_TIME) {
            DEBUG_PRINTLN("Waking up...");
            state = NgDemoState::WaitingGpsFix;
            stateChangedAt = millis();
            logPrinter.flush();
        }
        break;
    }
    }
}

bool NgDemo::transmission(String message) {
    DEBUG_PRINT("Message: ");
    DEBUG_PRINTLN(message.c_str());

    digitalWrite(PIN_RED_LED, HIGH);

    bool success = false;
    uint32_t started = millis();
    if (message.length() > 0) {
        RockBlock rockBlock(message);
        rockBlockSerialBegin();
        SerialType &rockBlockSerial = RockBlockSerial;
        rockBlock.setSerial(&rockBlockSerial);
        while (!rockBlock.isDone() && !rockBlock.isFailed()) {
            if (millis() - started < THIRTY_MINUTES) {
                Watchdog.reset();
            }
            rockBlock.tick();
            delay(10);
        }
        success = rockBlock.isDone();
        DEBUG_PRINT("RockBlock: ");
        DEBUG_PRINTLN(success);
    }

    digitalWrite(PIN_RED_LED, LOW);
    logPrinter.flush();

    return success;
}

bool NgDemo::checkGps() {
    Watchdog.reset();

    while (Serial1.available()) {
        char c = gps.read();
        Serial.print(c);
    }

    if (gps.newNMEAreceived()) {
        if (gps.parse(gps.lastNMEA())) {
            if (gps.fix) {
                DEBUG_PRINTLN("GOT NMEA");
                DateTime dateTime = DateTime(gps.year, gps.month, gps.year, gps.hour, gps.minute, gps.seconds);
                uint32_t time = dateTime.unixtime();
                SystemClock->set(time);
                latitude = gps.latitudeDegrees;
                longitude = gps.longitudeDegrees;
                altitude = gps.altitude;
                return true;
            }
        }
    }
    return false;
}