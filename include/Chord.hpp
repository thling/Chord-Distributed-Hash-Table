#ifndef __CHORD_HPP__
#define __CHORD_HPP__
#define CHORD_LENGTH_BIT 32

#include <vector>

#include <sys/socket.h>

#include "ChordError.hpp"
#include "MessageHandler.hpp"
#include "ThreadFactory.hpp"

const unsigned int UNINITIALIZED = 1000;
const unsigned int INITIALIZED = 1001;
const unsigned int WAITING_TO_JOIN = 1002;
const unsigned int IN_NETWORK = 1003;
const unsigned int SERVICING = 1004;
const unsigned int SERVICE_CLOSING = 1005;
const unsigned int SERVICE_FAILED = 1006;

using namespace std;

typedef struct {
    char *ipaddr;
    char *hostname;
    unsigned int hashedId;
    
    int sfd;
    unsigned int appPort;
    struct sockaddr *addr;
    socklen_t len;
} node;

class Chord : public ChordError, ThreadFactory {
public:
    Chord(unsigned int appPort, unsigned int chordPort, char *ipaddr = NULL);
    ~Chord();
    
    bool init();
    bool start();
    void stop();
    
    char *query(char *key, char *hostip, unsigned int &port);
    char *getChordMap();
    
    void setJoinPointIp(char *toJoin);
    
    unsigned int getState();
    
private:
    unsigned int state;
    unsigned int hashedId;
    unsigned int appPort, chordPort;
    int chord_sfd;
    
    char *ipaddr, *hostname, *joinPointIp;
    node *successor, *predecessor;
    
    vector<SuccessorQueryResponse *> sqrQueue;
    
    bool join();
    void threadWorker();
    
    unsigned int getHashedId();
    unsigned int getHashedKey(char *key);
    
    node *createNode(char *ipaddr);
    node *getSuccessor();
    
    void *receiveMessage(int &size, unsigned int timeout = 0);
    size_t send(node *n, unsigned char *data, size_t len, int flag = 0);
    
    void pushSqr(SuccessorQueryResponse *sqr);
    SuccessorQueryResponse *popSqr();
    
    unsigned int getConsistentHash(char *, size_t len);
};

#endif
