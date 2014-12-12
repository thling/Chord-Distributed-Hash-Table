#ifndef __CHORD_ERROR__
#define __CHORD_ERROR__

const unsigned int NO_ERROR = 100000;
const unsigned int ERR_INVALID_KEY = 100001;
const unsigned int ERR_CONN_LOST = 100002;
const unsigned int ERR_CANNOT_CONNECT = 100003;
const unsigned int ERR_CANNOT_JOIN_CHORD = 100004;
const unsigned int ERR_CANNOT_START_THREAD = 100005;
const unsigned int ERR_NOT_INITIALIZED = 100006;

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
            case NO_ERROR:
                return "No error number was set";
            default:
                return "Undefined error number";
        }
    }
};

#endif
