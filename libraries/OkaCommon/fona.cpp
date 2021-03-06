#include "fona.h"

FonaChild::FonaChild() :
    phoneNumber(""), message(""),
    NonBlockingSerialProtocol(60 * 1000, true, false), tries(0), smsTries(0) {
}

FonaChild::FonaChild(String phoneNumber, String message) :
    phoneNumber(phoneNumber), message(message),
    NonBlockingSerialProtocol(60 * 1000, true, false), tries(0), smsTries(0) {
}

bool FonaChild::tick() {
    if (NonBlockingSerialProtocol::tick()) {
        return true;
    }

    if (getSendsCounter() == 5) {
        transition(FonaDone);
        return true;
    }

    switch (state) {
        case FonaStart: {
            registered = false;
            if (tries++ > 3) {
                transition(FonaPowerOffBeforeFailed);
            }
            else {
                drain();
                sendCommand("~HELLO");
            }
            break;
        }
        case FonaPower: {
            sendCommand("~POWER");
            break;
        }
        case FonaNetworkStatus: {
            if (tries++ > 10) {
                transition(FonaPowerOffBeforeFailed);
            }
            else {
                sendCommand("~STATUS");
            }
            break;
        }
        case FonaWaitForNetwork: {
            if (millis() - lastStateChange > 2000) {
                transition(FonaNetworkStatus);
            }
            break;
        }
        case FonaNumber: {
            smsTries++;
            String command = "~NUMBER " + phoneNumber;
            sendCommand(command.c_str());
            break;
        }
        case FonaMessage: {
            if (message.length() > 0) {
                String command = "~MESSAGE " + message;
                sendCommand(command.c_str());
            }
            else {
                sendCommand("~STATUS");
            }
            break;
        }
        case FonaSendSms: {
            if (message.length() > 0) {
                sendCommand("~SEND");
            }
            else {
                sendCommand("~STATUS");
            }
            break;
        }
        case FonaPowerOffBeforeFailed: {
            sendCommand("~OFF");
            break;
        }
        case FonaPowerOffBeforeDone: {
            sendCommand("~OFF");
            break;
        }
    }
    return true;
}

NonBlockingHandleStatus FonaChild::handle(String reply) {
    if (reply.length() > 0) {
        DEBUG_PRINT(state);
        DEBUG_PRINT(">");
        DEBUG_PRINTLN(reply);
    }
    if (reply.startsWith("OK")) {
        switch (state) {
            case FonaStart: {
                transition(FonaPower);
                break;
            }
            case FonaPower: {
                if (phoneNumber.length() == 0) {
                    transition(FonaPowerOffBeforeDone);
                }
                else {
                    transition(FonaNetworkStatus);
                }
                tries = 0;
                break;
            }
            case FonaNetworkStatus: {
                if (registered) {
                    transition(FonaNumber);
                }
                else {
                    transition(FonaWaitForNetwork);
                }
                break;
            }
            case FonaNumber: {
                transition(FonaMessage);
                break;
            }
            case FonaMessage: {
                transition(FonaSendSms);
                break;
            }
            case FonaSendSms: {
                transition(FonaPowerOffBeforeDone);
                break;
            }
            case FonaPowerOffBeforeFailed: {
                transition(FonaFailed);
                break;
            }
            case FonaPowerOffBeforeDone: {
                transition(FonaDone);
                break;
            }
            case FonaDone: {
                break;
            }
        }
        return NonBlockingHandleStatus::Handled;
    }
    else if (reply.startsWith("ER")) {
        switch (state) {
            case FonaSendSms: {
                if (smsTries == 3) {
                    transition(FonaPowerOffBeforeFailed);
                }
                else {
                    transition(FonaNetworkStatus);
                }
                break;
            }
            case FonaPowerOffBeforeDone:
            case FonaPowerOffBeforeFailed: {
                transition(FonaFailed);
                break;
            }
        }
        return NonBlockingHandleStatus::Handled;
    }
    else if (reply.startsWith("+STATUS")) {
        int8_t comma = reply.indexOf(",");
        int8_t status = reply.substring(comma + 1, comma + 2).toInt(); 
        registered = status == 1 || status == 5; // Home or Roaming
    }

    return NonBlockingHandleStatus::Ignored;
}

