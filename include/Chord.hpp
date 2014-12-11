#ifndef __CHORD_HPP__
#define __CHORD_HPP__
#define CHORD_LENGTH_BIT 32

#include <sys/socket.h>

#include "ChordError.hpp"
#include "MessageHandler.hpp"

typedef struct {
    char *ipaddr;
    char *hostname;
    unsigned int hashedId;
    
    int sfd;
    struct sockaddr *addr;
    socklen_t len;
} node;

class Chord : public ChordError {
public:
    Chord(unsigned int appPort, unsigned int chordPort, char *ipaddr = NULL);
    
    bool init();
    char *query(char *key);
    char *getChordMap();
    
    unsigned int getHashedId();
    unsigned int getHashedKey(char *key);
    
    unsigned int getConsistentHash(char *, size_t len);
    
    bool join(char *ipaddr = NULL);
    
private:
    MessageHandler *mhandler;
    
    unsigned int hashedId;
    unsigned int appPort, chordPort;
    int chordSfd;
    
    char *ipaddr;
    node *successor, *predecessor;
    unsigned int numServers;
    
    node *createNode(char *ipaddr);
    node *getSuccessor();
    size_t send(node *n, unsigned char *data, size_t len, int flag = 0);
};

#endif
