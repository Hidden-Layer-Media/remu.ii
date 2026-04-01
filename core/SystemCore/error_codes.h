#ifndef ERROR_CODES_H
#define ERROR_CODES_H

// Error codes
enum ErrorCodes {
    ERROR_NONE = 0,
    ERROR_ENTROPY = 1,
    ERROR_POWER = 2,
    ERROR_WATCHDOG = 3,
    ERROR_INITIALIZATION = 4,
    ERROR_COMMUNICATION = 5,
    ERROR_INVALID_STATE = 6,
    ERROR_LOW_HEAP = 7
};

// Error descriptions
inline const char* getErrorDescription(ErrorCodes code) {
    switch(code) {
        case ERROR_NONE: return "No error";
        case ERROR_ENTROPY: return "Entropy system failure";
        case ERROR_POWER: return "Power management failure";
        case ERROR_WATCHDOG: return "Watchdog timeout";
        case ERROR_INITIALIZATION: return "Initialization failure";
        case ERROR_COMMUNICATION: return "Communication error";
        case ERROR_INVALID_STATE: return "Invalid system state";
        case ERROR_LOW_HEAP: return "Low heap memory";
        default: return "Unknown error";
    }
}
#endif