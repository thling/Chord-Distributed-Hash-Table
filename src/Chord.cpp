#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <sstream>

#include <gmp.h>
#include <openssl/sha.h>

#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "../include/Chord.hpp"
#include "../include/MessageHandler.hpp"
#include "../include/MessageTypes.hpp"
#include "../include/Utils.hpp"

using namespace std;

Chord::Chord(unsigned int appPort, unsigned int chordPort, char *ipaddr) {
    if (ipaddr == NULL) {
        this->ipaddr = getIpAddr();
    } else {
        this->ipaddr = ipaddr;
    }
    
    this->chordPort = chordPort;
    this->appPort = appPort;
}

bool Chord::init() {
    this->hashedId = this->getConsistentHash(this->ipaddr, strlen(this->ipaddr) + 1);
    this->predecessor = NULL;
    this->successor = NULL;
    this->mhandler = new MessageHandler();

    return true;
}

char *Chord::query(char *key) {
    if (key == NULL) {
        return NULL;
    }
    
    return NULL;
}

unsigned int Chord::getHashedId() {
    return this->hashedId;
}

unsigned int Chord::getHashedKey(char *key){
    if (key == NULL) {
        this->setErrorno(ERR_INVALID_KEY);
        return 0;
    }
    
    return 0;
}

node *Chord::getSuccessor() {
    return this->successor;
}

bool Chord::join(char *ipaddr) {
    if (ipaddr == NULL || strcmp(ipaddr, this->ipaddr) == 0) {
        //  If ipaddr == NULL or == self, create new Chord ring
        // maybe don't have to handle this, check later
        
        return true;
    } else {
        // Construct successor request
        SuccessorQuery *squery = new SuccessorQuery();
        squery->type = MTYPE_SUCCESSOR_QUERY;
        squery->size = 8 + strlen(this->ipaddr) + 1;
        squery->sender = new char[strlen(this->ipaddr) + 1];
        memcpy(squery->sender, this->ipaddr, strlen(this->ipaddr) + 1);
        
        unsigned char *serializedData = this->mhandler->serialize(squery);
        unsigned char *buffer = new unsigned char[1024];
        
        // Create a node struct for sending, using the receiver's ipaddress
        node *sendto = this->createNode(ipaddr);

        int timeoutCount = 0;
        while (timeoutCount < 3) {
            this->send(sendto, serializedData, squery->size);
            
            // Wait on 1.5 secs timeout
            struct timeval tv = {1, 500000};
            fd_set readfds;
            FD_SET(this->chordSfd, &readfds);

            int ret = select(this->chordSfd + 1, &readfds, NULL, NULL, &tv);
            
            if (ret == -1) {
                cerr << "Cannot receive: " << strerror(errno) << endl;
                this->setErrorno(ERR_CANNOT_CONNECT);
                return false;
            } else if (ret == 0) {
                // Timeout event, 3 timeout = can't join
                timeoutCount++;
            } else {
                // Try to receive
                int recvd = recvfrom(this->chordSfd, buffer, 1024, 0, NULL, 0);
                
                if (recvd == -1) {
                    cerr << "Cannot receive: " << strerror(errno) << endl;
                    this->setErrorno(ERR_CANNOT_CONNECT);
                    return false;
                } else if (mhandler->getType(buffer) != MTYPE_SUCCESSOR_QUERY_RESPONSE) {
                    continue;   // Ignore if not expected type
                }

                break;
            }
        }

        if (timeoutCount == 3) {
            cerr << "Timeout occured while attempted to join " << ipaddr << endl;
            this->setErrorno(ERR_CANNOT_JOIN_CHORD);
            return false;
        }

        // Received a proper response;
        SuccessorQueryResponse *sqr = (SuccessorQueryResponse *) mhandler->unserialize(buffer);
        this->successor = this->createNode(sqr->responder);
        
        delete squery;
        delete sendto;
        delete sqr;
        
        return true;
    }
}

node *Chord::createNode(char *ipaddr = NULL) {
    char *hostname;
    if (ipaddr == NULL) {
        ipaddr = this->ipaddr;
        dprt << "Warning: creating a node of self";
    }
    
    hostname = getHostname(ipaddr);
    
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    
    stringstream ss;
    ss << this->chordPort;
    
    int ret = getaddrinfo(hostname, ss.str().c_str(), &hints, &res);
    
    if (ret != 0) {
        cerr << "[ERROR] Cannot lookup " << hostname << ": " << gai_strerror(ret) << endl;
        this->setErrorno(ERR_CANNOT_CONNECT);
        return NULL;
    }

    int socket_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (socket_fd == -1) {
        cerr << "[ERROR] Cannot setup socket to " << hostname << ": " << strerror(errno) << endl;
        this->setErrorno(ERR_CANNOT_CONNECT);
        return NULL;
    }

    node *n = new node();
    n->hashedId = this->getConsistentHash(ipaddr, strlen(ipaddr) + 1);
    n->sfd = socket_fd;
    n->addr = res->ai_addr;
    n->len = res->ai_addrlen;
    n->ipaddr = new char[strlen(ipaddr) + 1];
    n->hostname = new char[strlen(hostname) + 1];
    strcpy(n->ipaddr, ipaddr);
    strcpy(n->hostname, hostname);
    
    return n;
}

char *Chord::getChordMap() {
    
    return NULL;
}

size_t Chord::send(node *n, unsigned char *data, size_t len, int flag) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t size = sendto(n->sfd, data + sent, len, flag, n->addr, n->len);
        
        if (size == -1) {
            cerr << "[ERROR] Problem sending data: " << strerror(errno) << endl;
            return size;
        }
        
        sent += size;
    }
    
    return sent;
}

unsigned int Chord::getConsistentHash(char *tohash, size_t len) {
    unsigned char *hash = new unsigned char[160];
    SHA1((unsigned char *) tohash, len, hash);
    
    stringstream ss;
    for(int i = 0; i < 20; ++i) {
        ss << hex << setw(2) << setfill('0') << (int)hash[i];
    }
    
    mpz_t chash, largenum;
    mpz_inits(chash, largenum, NULL);
    mpz_set_str(largenum, cstr(ss.str()), 16);
    mpz_mod_ui(chash, largenum, pow((long double) 2, (long double) CHORD_LENGTH_BIT));
    
    unsigned int pos = (unsigned int) mpz_get_ui(chash);
    dprt << "Key/Node " << tohash << " maps to " << pos;
    
    return pos;
}
