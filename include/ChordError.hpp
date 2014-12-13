#ifndef __CHORD_ERROR__
#define __CHORD_ERROR__

const unsigned int NO_ERROR = 0;
const unsigned int ERR_INVALID_KEY = 1;
const unsigned int ERR_CONN_LOST = 2;
const unsigned int ERR_CANNOT_CONNECT = 3;
const unsigned int ERR_CANNOT_JOIN_CHORD = 4;
const unsigned int ERR_CANNOT_START_THREAD = 5;
const unsigned int ERR_NOT_INITIALIZED = 6;
const unsigned int ERR_NOT_IN_SERVICE = 7;
const unsigned int ERR_NO_SUCCESSOR = 8;
const unsigned int ERR_LOCAL_KEY = 9;

/**
 * Chord errors wrapper, used for organizing error code and their explanatory strings
 */
class ChordError {
protected:
    unsigned int err;

    void setErrorno(unsigned int e) {
        err = e;
    }
    
public:
    unsigned int getErrno() {
        return err;
    }
    
    const char *getError() {
        switch (err) {
            case ERR_INVALID_KEY:
                return "Invalid key supplied";
            case ERR_CONN_LOST:
                return "Connection lost";
            case ERR_CANNOT_CONNECT:
                return "Cannot connect";
            case ERR_CANNOT_JOIN_CHORD:
                return "Cannot join the Chord network";
            case ERR_CANNOT_START_THREAD:
                return "Cannot start thread";
            case ERR_NOT_INITIALIZED:
                return "Chord service not initialized";
            case ERR_NOT_IN_SERVICE:
                return "Chord service not running";
            case ERR_NO_SUCCESSOR:
                return "No successor in chord ring";
            case ERR_LOCAL_KEY:
                return "The key appears to be local";
            case NO_ERROR:
                return "No error number was set";
            default:
                return "Undefined error number";
        }
    }
};

#endif
