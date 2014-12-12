#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <sstream>

#include <gmp.h>
#include <openssl/sha.h>
#include <pthread.h>

#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "../include/Chord.hpp"
#include "../include/MessageHandler.hpp"
#include "../include/MessageTypes.hpp"
#include "../include/ThreadFactory.hpp"
#include "../include/Utils.hpp"

using namespace std;

pthread_mutex_t sqrMutex;

Chord::Chord(unsigned int appPort, unsigned int chordPort, char *thisIpaddr) {
    if (ipaddr == NULL) {
        this->ipaddr = getIpAddr();
    } else {
        this->ipaddr = thisIpaddr;
    }
    
    this->chordPort = chordPort;
    this->appPort = appPort;
    
    pthread_mutex_init(&sqrMutex, NULL);
    
    this->joinPointIp = NULL;
    this->state = UNINITIALIZED;
}

Chord::~Chord() {
    this->stop();
}

/**
 * Initializes the chord service
 * 
 * @return  True if init succeeded; false otherwise and sets ChordError number
 */
bool Chord::init() {
    this->hashedId = this->getConsistentHash(this->ipaddr, strlen(this->ipaddr) + 1);
    this->predecessor = NULL;
    this->successor = NULL;
    this->hostname = getHostname();
    
    this->state = INITIALIZED;
    return true;
}

void Chord::stop() {
    this->state = SERVICE_CLOSING;
    close(this->chord_sfd);
    this->waitExit();
}

/**
 * Starts the chord thread. This includes setting up socket,
 * binding to port, and attempting to join an existing chord network
 * (if a joinPointIp was specified by setJoinPointIp())
 * 
 * @return  True if successfully started. False on error, and sets ChordError number
 */
bool Chord::start() {
    if (this->state != INITIALIZED) {
        dprt << "Attempted to start an uninitialzed chord service";
        this->setErrorno(ERR_NOT_INITIALIZED);
        return false;
    }
    
    // Set up UDP socket
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    stringstream ss;
    ss << this->chordPort;
    
    getaddrinfo(this->hostname, ss.str().c_str(), &hints, &res);
    
    this->chord_sfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (this->chord_sfd < 0) {
        dprt << "Call to socket failed: " << strerror(errno);
        this->setErrorno(ERR_CANNOT_CONNECT);
        this->state = SERVICE_FAILED;
        return false;
    }
    
    if (bind(this->chord_sfd, res->ai_addr, res->ai_addrlen) < 0) {
        dprt << "Call to bind failed: " << strerror(errno);
        this->setErrorno(ERR_CANNOT_CONNECT);
        this->state = SERVICE_FAILED;
        return false;
    }
    
    // Attempt to join, if specified which IP to join
    if (!this->join()) {
        this->setErrorno(ERR_CANNOT_JOIN_CHORD);
        this->state = SERVICE_FAILED;
        return false;
    }
    
    // Attempt to start receiver thread
    if (!this->startThread()) {
        dprt << "Cannot start receiving thread: " << strerror(errno);
        this->setErrorno(ERR_CANNOT_START_THREAD);
        this->state = SERVICE_FAILED;
        return false;
    }
    
    this->state = SERVICING;
    return true;
}

void Chord::threadWorker() {
    dprt << "Starting thread worker...";
    while (true) {
        int recvSize = 0;
        
        void *msg = this->receiveMessage(recvSize);
        if (msg == NULL) {
            if (recvSize == -1) {
                dprt << "Cannot listen to socket: " << strerror(errno);
            } else if (recvSize == -2) {
                // Timeout
                usleep(100000);
                continue;
            } else {
                dprt << "Socket closed, returning";
                break;
            }
        }
        
        unsigned int type = MessageHandler::getType(msg);
        switch (type) {
            case MTYPE_KEY_QUERY:
            {
                break;
            }
            case MTYPE_KEY_QUERY_RESPONSE:
            {
                break;
            }
            case MTYPE_SUCCESSOR_QUERY:
            {
                dprt << "New successor query";
                SuccessorQuery *sq = (SuccessorQuery *) msg;
                
                if (this->successor == NULL && this->predecessor == NULL) {
                    SuccessorQueryResponse *sqr = MessageHandler::createSQR(
                            sq->searchTerm,
                            this->appPort,
                            this->ipaddr
                    );
                    
                    this->successor = createNode(sq->sender);
                    this->successor->appPort = sq->appPort;
                    this->send(this->successor, MessageHandler::serialize(sqr), sqr->size);
                    
                    delete[] sqr->responder;
                    delete sqr;
                } else if ((sq->searchTerm > this->hashedId && sq->searchTerm <= this->successor->hashedId)
                        || (sq->searchTerm > this->hashedId && this->hashedId > this->successor->hashedId)) {
                    SuccessorQueryResponse *sqr = MessageHandler::createSQR(
                            sq->searchTerm,
                            this->successor->appPort,
                            this->successor->ipaddr
                    );
 
                    this->successor = createNode(sq->sender);
                    this->successor->appPort = sq->appPort;
                    this->send(this->successor, MessageHandler::serialize(sqr), sqr->size);
                    
                    delete[] sqr->responder;
                    delete sqr;
                } else {
                    this->send(this->successor, MessageHandler::serialize(sq), sq->size);
                }
                
                delete[] sq->sender;
                delete sq;
                break;
            }
            case MTYPE_SUCCESSOR_QUERY_RESPONSE:
            {   
                dprt << "New successor query response";
                SuccessorQueryResponse *sqr = (SuccessorQueryResponse *) msg;
                this->pushSqr(sqr);
                break;
            }
            default:
                dprt << "Cannot identify type";
        }
        
        usleep(100000);
    }
}

char *Chord::query(char *key, char *hostip, unsigned int &hostport) {
    if (key == NULL) {
        return NULL;
    }
    
    if (this->successor == NULL) {
        hostip = NULL;
        hostport = 0;
        return NULL;
    }
    
    unsigned int keyhash = this->getConsistentHash(key, strlen(key) + 1);
    SuccessorQuery *sq = MessageHandler::createSQ(keyhash, this->appPort, this->ipaddr);
    
    /** change this **/
    this->send(this->successor, MessageHandler::serialize(sq), sq->size);
    
    SuccessorQueryResponse *sqr = NULL;
    while (true) {
        sqr = this->popSqr();
        if (sqr == NULL) {
            usleep(100000);
            continue;
        }
        
        break;
    }
    
    if (sqr->searchTerm == keyhash) {
        hostip = sqr->responder;
        hostport = sqr->appPort;
    } else {
        hostip = NULL;
        hostport = 0;
        return NULL;
    }
    
    return hostip;
}

void *Chord::receiveMessage(int &size, unsigned int timeout) {
    // For storing things
    unsigned char *buffer = new unsigned char [1024];
    // Calculate timeout if specified
    struct timeval tv = {timeout / 1000, (timeout - (timeout / 1000) * 1000) * 1000};
    // Populate fd_sets for select
    fd_set readfds, exceptfds;
    FD_SET(this->chord_sfd, &readfds);
    exceptfds = readfds;
    
    // Select on socket fds for given timeout
    int ret = select(this->chord_sfd + 1, &readfds, NULL, &exceptfds, &tv);
    if (ret == -1) {
        cerr << "Cannot receive: " << strerror(errno) << endl;
        this->setErrorno(ERR_CANNOT_CONNECT);
        size = ret;
        return NULL;
    } else if (ret == 0) {
        // Timeout, set size to -2
        size = -2;
        return NULL;
    } else {
        // Try to receive
        size = recvfrom(this->chord_sfd, buffer, 1024, 0, NULL, 0);

        if (size == -1) {
            dprt << "Cannot receive: " << strerror(errno);
            this->setErrorno(ERR_CANNOT_CONNECT);
            return NULL;
        } else if (size == 0) {
            return NULL;
        }
    }
    
    // Unserialze and return message
    void *retval = MessageHandler::unserialize(buffer);
    delete[] buffer;
    
    return retval;
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

void Chord::setJoinPointIp(char *toJoin) {
    this->joinPointIp = toJoin;
}

bool Chord::join() {
    if (this->state != INITIALIZED) {
        this->setErrorno(ERR_NOT_INITIALIZED);
        return false;
    }
    
    this->state = WAITING_TO_JOIN;
    
    if (this->joinPointIp == NULL || strcmp(this->joinPointIp, this->ipaddr) == 0) {
        // If ipaddr == NULL or == self, create new Chord ring
        // maybe don't have to handle this, check later
    } else {
        // Construct successor request
        SuccessorQuery *squery = MessageHandler::createSQ(this->hashedId, this->appPort, this->ipaddr);
        unsigned char *serializedData = MessageHandler::serialize(squery);

        // Create a node struct for sending, using the receiver's ipaddress
        node *sendto = this->createNode(this->joinPointIp);
        
        void *msg = NULL;
        int timeoutCount = 0;
        
        while (timeoutCount < 3) {
            this->send(sendto, serializedData, squery->size);
            
            int recvd = 0;
            msg = this->receiveMessage(recvd, 1500);
            
            if (recvd == -1) {
                cerr << "Cannot receive: " << strerror(errno) << endl;
                this->setErrorno(ERR_CANNOT_CONNECT);
                return false;
            } else if (recvd == -2) {
                // Timeout event
                timeoutCount++;
            } else if (recvd == 0) {
                cerr << "Socket was closed" << endl;
                return false;
            } else {
                if (MessageHandler::getType(msg) != MTYPE_SUCCESSOR_QUERY_RESPONSE) {
                    continue;   // Ignore if not expected type
                }

                break;
            }
        }

        if (timeoutCount == 3) {
            dprt << "Timeout occured while attempted to join " << ipaddr;
            this->setErrorno(ERR_CANNOT_JOIN_CHORD);
            return false;
        }

        // Received a proper response;
        SuccessorQueryResponse *sqr = (SuccessorQueryResponse *) msg;
        if (sqr->responder == NULL) {
            this->successor = NULL;
        } else {
            this->successor = this->createNode(sqr->responder);
            this->successor->appPort = sqr->appPort;
        }
        
        delete squery;
        delete sendto;
        delete sqr;
    }
    
    this->state = IN_NETWORK;
    return true;
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

void Chord::pushSqr(SuccessorQueryResponse *sqr) {
    pthread_mutex_lock(&sqrMutex);
    this->sqrQueue.push_back(sqr);
    pthread_mutex_unlock(&sqrMutex);
}

SuccessorQueryResponse *Chord::popSqr() {
    SuccessorQueryResponse *ret = NULL;
    
    pthread_mutex_lock(&sqrMutex);
    vector<SuccessorQueryResponse *>::iterator it = this->sqrQueue.begin();
    if (it != this->sqrQueue.end()) {
        // Has item
        ret = (*it);
        this->sqrQueue.erase(it);
    }
    pthread_mutex_unlock(&sqrMutex);
    
    return ret;
}

unsigned int Chord::getState() {
    return this->state;
}
