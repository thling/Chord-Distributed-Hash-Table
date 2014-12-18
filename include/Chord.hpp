#ifndef __CHORD_HPP__
#define __CHORD_HPP__
#define CHORD_LENGTH_BIT 32

#include <map>
#include <vector>

#include <sys/socket.h>

#include "ChordError.hpp"
#include "MessageHandler.hpp"
#include "ServiceNotification.hpp"
#include "ThreadFactory.hpp"

namespace ChordStatus {
    enum status {
        UNINITIALIZED,
        INITIALIZED,
        WAITING_TO_JOIN,
        IN_NETWORK,
        SERVICING,
        MAPPING_CHORD,
        MAPPING_COMPLETED,
        STABILIZING,
        SERVICE_CLOSING,
        SERVICE_FAILED,
        UPDATING_FINGER
    };
};


// How long to wait before resend
const unsigned int SEND_TIMEOUT = 1500000;  // 1.5 seconds
// How long between stabilizing
const unsigned int PERIODIC_JOBS_TIMEOUT = 1500000;  // 1.5 seconds
// How many times to try to join
const unsigned int JOIN_TRIALS = 5;

using namespace std;

typedef struct {
    char *ipaddr;
    char *hostname;
    unsigned int hashedId;
    bool isSelf;
    
    int sfd;
    unsigned int appPort;
    struct sockaddr *addr;
    socklen_t len;
} node;

typedef struct {
    uint32_t timestamp;
    node *recipient;
    unsigned char *context;
} msgTimer;

/**
 * ChordNotification class
 * 
 * Used to structure the notification system. Contains all the information
 * needed for a useful notification for this class instance
 * 
 * @extends Notification    Base class for notification object abstraction
 */
class ChordNotification : public Notification {
public:
    const static unsigned int NTYPE_SYNC_NOTIFICATION = 0;
    
    ChordNotification(unsigned int type, const char *fromIp, unsigned int appPort) {
        this->type = type;
        this->hostIp = fromIp;
        this->appPort = appPort;
    }
    
    unsigned int getType() { return this->type; }
    unsigned int getPort() { return this->appPort; }
    const char *getIp() { return this->hostIp; }
    
protected:
    unsigned int type;
    unsigned int appPort;
    const char *hostIp;
    
};

/**
 * Chord main class
 * 
 * Simplified implementation of the chord distributed hash table
 * 
 * @extends ChordError              The error wrapper for this chord service
 * @extends ServiceNotification     Base class for notification service abstraction
 * @extends ThreadFactory           For object oriented threading environment using pthread
 */
class Chord : public ChordError, public ServiceNotification, public ThreadFactory {
public:
    Chord(unsigned int appPort, unsigned int chordPort, char *ipaddr = NULL);
    ~Chord();
    
    bool init();
    bool start();
    void stop();
    
    char *query(char *key, char **hostip, unsigned int &port, unsigned int timeout = 0);
    char *getChordMap();
    char *getFingerTable();
    unsigned int getHashedKey(char *key);
    
    void setJoinPointIp(char *toJoin);
    
    ChordStatus::status getState();
    
    /**
     * Implements the parent function
     * 
     * @return  A ChordNotification object
     */
    ChordNotification *popNotification() { return (ChordNotification *) ServiceNotification::popNotification(); }
    
private:
    pthread_mutex_t successorResponseQueueMutex, sendTimerMutex, chordMapResponseQueueMutex, fingerMutex;

    ChordStatus::status state, substate;
    unsigned int hashedId;
    unsigned int appPort, chordPort;
    unsigned int lastStabilizedTimestamp;
    unsigned int lastFingerUpdateTimestamp;
    int chord_sfd;
    
    char *ipaddr, *hostname, *joinPointIp;
    node *successor, *predecessor;
    map<uint32_t, node *> fingers;
    
    map<uint32_t, msgTimer *> sendTimers;
    vector<SuccessorResponse *> successorResponseQueue;
    vector<ChordMapResponse *> chordMapResponseQueue;
    
    bool join();
    void notifySuccessor();
    
    void processPeriodicJobs();
    void threadWorker();
    void stabilize();
    
    unsigned int getHashedId();
    
    node *createNode(char *ipaddr = NULL);
    node *getSuccessor();
    
    void *receiveMessage(int &size, unsigned int timeout = 0);
    size_t send(node *n, unsigned char *data, size_t len, int flag = 0);
    
    void pushSuccessorResponse(SuccessorResponse *sr);
    SuccessorResponse *popSuccessorResponse();
    void pushChordMapResponse(ChordMapResponse *cmr);
    ChordMapResponse *popChordMapResponse();
    
    void pushSendTimer(node *sendTo, uint32_t searchTerm, unsigned char *data, size_t len);
    void unsetSendTimer(uint32_t searchTerm);
    
    unsigned int getConsistentHash(char *, size_t len);
    bool isInSuccessor(uint32_t key, uint32_t start = 0, uint32_t end = 0);
    node *getSuccessorOf(uint32_t key, bool useFinger = true);
};

#endif
