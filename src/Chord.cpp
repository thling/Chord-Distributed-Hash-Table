#include <cerrno>
#include <climits>
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

/**
 * Simplified chord service implementation.
 * 
 * Please see SampleApp.cpp on how to use this simplified implementation.
 * 
 * The implementation follows the paper by Stocia et al [1], lacking the
 * failure recovery/fault tolerant features.
 * 
 * The simplified service will allow joining of new nodes, querying of new nodes,
 * and printing the chord map on smaller networks. As per the specification,
 * the required "lookup" API will return the node responsible for the
 * key (the successor of key), and the application using the service is
 * responsible for transferring data between teh hosts.
 * 
 * The standard key lookup API will return a host IP address and application
 * port number to the application. The implementing application shall not
 * need to depend on the service for transferring files.
 * 
 * Other reference: Chord presentation by Professor Ali Yildiz [2]
 * 
 * [1] http://pdos.csail.mit.edu/papers/chord:sigcomm01/chord_sigcomm.pdf
 * [2] http://www.cs.nyu.edu/courses/fall07/G22.2631-001/Chord.ppt
 */
Chord::Chord(unsigned int appPort, unsigned int chordPort, char *thisIpaddr) {
    if (ipaddr == NULL) {
        this->ipaddr = getIpAddr();
    } else {
        this->ipaddr = thisIpaddr;
    }
    
    this->chordPort = chordPort;
    this->appPort = appPort;
    
    pthread_mutex_init(&(this->successorResponseQueueMutex), NULL);
    pthread_mutex_init(&(this->chordMapResponseQueueMutex), NULL);
    pthread_mutex_init(&(this->sendTimerMutex), NULL);
    pthread_mutex_init(&(this->fingerMutex), NULL);

    this->joinPointIp = NULL;
    this->state = ChordStatus::UNINITIALIZED;
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
    this->lastStabilizedTimestamp = 0;
    
    this->state = ChordStatus::INITIALIZED;
    this->substate = ChordStatus::IN_NETWORK;
    return true;
}

/**
 * Stops the chord service
 */
void Chord::stop() {
    this->state = ChordStatus::SERVICE_CLOSING;
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
    if (this->state != ChordStatus::INITIALIZED) {
        dprt << "Attempted to start an uninitialzed chord service";
        this->setErrorno(ERR_NOT_INITIALIZED);
        return false;
    }
    
    // Set up UDP socket for incoming connections
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
        this->state = ChordStatus::SERVICE_FAILED;
        return false;
    }
    
    if (bind(this->chord_sfd, res->ai_addr, res->ai_addrlen) < 0) {
        dprt << "Call to bind failed: " << strerror(errno);
        this->setErrorno(ERR_CANNOT_CONNECT);
        this->state = ChordStatus::SERVICE_FAILED;
        return false;
    }
    
    // Attempt to join, if specified which IP to join
    if (!this->join()) {
        this->setErrorno(ERR_CANNOT_JOIN_CHORD);
        this->state = ChordStatus::SERVICE_FAILED;
        return false;
    }
    
    // Attempt to start receiver thread
    if (!this->startThread()) {
        dprt << "Cannot start receiving thread: " << strerror(errno);
        this->setErrorno(ERR_CANNOT_START_THREAD);
        this->state = ChordStatus::SERVICE_FAILED;
        return false;
    }
    
    this->state = ChordStatus::SERVICING;
    
    // Allow time for everything to run before returning to app
    usleep(500000);
    return true;
}

/**
 * Processes jobs that require periodic operations
 */
void Chord::processPeriodicJobs() {
    // Resend timed out messages
    pthread_mutex_lock(&(this->sendTimerMutex));
    for (map<uint32_t, msgTimer *>::iterator it = this->sendTimers.begin(); it != this->sendTimers.end(); ++it) {
        if (it->second->timestamp + SEND_TIMEOUT <= getTimeInUSeconds()) {
            dprt << "Resending timed out message...";
            this->send(it->second->recipient, it->second->context, MessageHandler::getSize(it->second->context));
            it->second->timestamp = getTimeInUSeconds();
        }
    }
    pthread_mutex_unlock(&(this->sendTimerMutex));
    
    // Stabilize
    this->stabilize();
    
    // Do the fingering
    if (!this->successor->isSelf && this->lastFingerUpdateTimestamp + PERIODIC_JOBS_TIMEOUT * 2 <= getTimeInUSeconds()) {
        pthread_mutex_lock(&(this->fingerMutex));
        
        for (unsigned int i = 0; i < CHORD_LENGTH_BIT; ++i) {
            unsigned int searchTerm = pow(2, i);
            if (UINT_MAX - searchTerm < this->hashedId) {
                // May overflow the max UINT
                searchTerm = this->hashedId - (UINT_MAX - searchTerm);
            } else {
                searchTerm = this->hashedId + searchTerm;
            }
            
            if (this->isInSuccessor(searchTerm)) {
                this->fingers[searchTerm] = this->successor;
            } else {
                SuccessorQuery *sq_finger = MessageHandler::createSuccessorQuery(searchTerm, this->appPort, this->ipaddr);
                sq_finger->type = MTYPE_FINGER_QUERY;
                
                this->send(this->successor, MessageHandler::serialize(sq_finger), sq_finger->size);
            }
        }   
        
        pthread_mutex_unlock(&(this->fingerMutex));
        this->lastFingerUpdateTimestamp = getTimeInUSeconds();
    }
}

/**
 * Send out stabilize requests periodically
 */
void Chord::stabilize() {
    if (this->successor != NULL && 
            this->lastStabilizedTimestamp + PERIODIC_JOBS_TIMEOUT <= getTimeInUSeconds()) {
        if (this->successor->isSelf) {
            // If my successor is myself, see if I have a predecessor yet. If so, it is my successor
            if (this->predecessor != NULL) {
                this->successor = this->predecessor;
                this->lastStabilizedTimestamp = getTimeInUSeconds();
            }
        } else {
            // Otherwise, send stabilize request
            this->substate = ChordStatus::STABILIZING;
            
            StabilizeRequest *streq = MessageHandler::createStabilizeRequest(this->appPort, this->ipaddr);
            
            this->send(this->successor, MessageHandler::serialize(streq), streq->size);
            this->lastStabilizedTimestamp
                    = getTimeInUSeconds() + PERIODIC_JOBS_TIMEOUT - 200000;   // 200 ms to receive stablize response
        }
    }
}

/**
 * Implementing ThreadFactory::threadWorker() method for threading
 */
void Chord::threadWorker() {
    dprt << "Starting thread worker...";
    while (true) {
        int recvSize = 0;
        
        // Process timers
        this->processPeriodicJobs();
        
        // Get new messages
        void *msg = this->receiveMessage(recvSize);
        if (msg == NULL) {
            if (recvSize == -1) {
                dprt << "Cannot listen to socket: " << strerror(errno);
                break;
            } else if (recvSize == -2) {
                // Timeout
                usleep(100000);
                continue;
            } else {
                dprt << "Socket closed, returning";
                break;
            }
        }
        
        // Process each message by type
        unsigned int type = MessageHandler::getType(msg);
        switch (type) {
            case MTYPE_UPDATE_PREDECESSOR:
            {
                dprt << "New UpdatePredcessor";
                UpdatePredcessor *up = (UpdatePredcessor *) msg;
                
                // If predecessor is NULL or IP addresses/Port do not match, update predecessor
                if (this->predecessor == NULL
                        || (strcmp(this->predecessor->ipaddr, up->predecessor) != 0
                                || this->predecessor->appPort != up->appPort)) {
                    this->predecessor = this->createNode(up->predecessor);
                    this->predecessor->appPort = up->appPort;
                }
                
                // Acknowledge the update
                UpdatePredcessorAck *upAck = MessageHandler::createUpdatePredecessorAck(this->predecessor->hashedId);
                this->send(this->predecessor, MessageHandler::serialize(upAck), upAck->size);
                
                // Notify the implementing application about the change, have to move files
                this->pushNotification(new ChordNotification(
                        ChordNotification::NTYPE_SYNC_NOTIFICATION,
                        this->predecessor->ipaddr,
                        this->predecessor->appPort
                ));
                
                break;
            }
            case MTYPE_UPDATE_PREDECESSOR_ACK:
            {
                dprt << "New UpdatePredcessorAck";
                UpdatePredcessorAck *upAck = (UpdatePredcessorAck *) msg;
                
                // Remove timers
                if (upAck->hashedId == this->hashedId) {
                    this->unsetSendTimer(upAck->hashedId);
                }
                
                break;
            }
            case MTYPE_STABILIZE_REQUEST:
            {
                dprt << "New StabilizeRequest";
                StabilizeRequest *streq = (StabilizeRequest *) msg;
                
                if (this->predecessor == NULL) {
                    // If no predecessor, then this is the requestor's successor, so we can update safely
                    this->predecessor = this->createNode(streq->sender);
                    this->predecessor->appPort = streq->appPort;
                }
                
                StabilizeResponse *stres = MessageHandler::createStabilizeResponse(
                        this->predecessor->appPort, this->predecessor->ipaddr
                );
                
                this->send(this->createNode(streq->sender), MessageHandler::serialize(stres), stres->size);
                
                break;
            }
            case MTYPE_STABILIZE_RESPONSE:
            {
                dprt << "New StabilizeResponse";
                
                if (this->substate == ChordStatus::STABILIZING) {
                    // Proceed only if in STABILIZING state
                    StabilizeResponse *stres = (StabilizeResponse *) msg;
                    if (strcmp(stres->predecessor, this->ipaddr) != 0
                            || stres->appPort != this->appPort) {
                        this->successor = this->createNode(stres->predecessor);
                        this->successor->appPort = stres->appPort;
                    }
                    
                    this->lastStabilizedTimestamp = getTimeInUSeconds();
                    this->substate = ChordStatus::IN_NETWORK;
                }
                
                break;
            }
            case MTYPE_CHORD_MAP_QUERY:
            {
                dprt << "New ChordMapQuery";
                ChordMapQuery *cmq = (ChordMapQuery *) msg;
                
                if (strcmp(cmq->sender, this->ipaddr) == 0) {
                    // Query looped back to self
                    if (this->state == ChordStatus::MAPPING_CHORD) {
                        this->state = ChordStatus::MAPPING_COMPLETED;
                    }
                    
                    break;
                }
                
                // Pass onto the successor with next sequence (used for tracking which one came first
                ChordMapResponse *cmr = MessageHandler::createChordMapResponse(cmq->seq + 1, this->ipaddr);
                if (this->successor == NULL) {
                    // Set sequence to 0 to indicate deadend (broken ring)
                    cmr->seq = 0;
                }
                
                // Send my information back to the originator
                this->send(createNode(cmq->sender), MessageHandler::serialize(cmr), cmr->size);
                
                if (this->successor != NULL) {
                    cmq->seq++;
                    this->send(this->successor, MessageHandler::serialize(cmq), cmq->size);
                }
                
                break;
            }
            case MTYPE_CHORD_MAP_RESPONSE:
            {
                dprt << "New ChordMapResponse";
                if (this->state != ChordStatus::MAPPING_CHORD) {
                    // If I did not originate it, I should not receive a response
                    break;
                }
                
                ChordMapResponse *cmr = (ChordMapResponse *) msg;
                this->pushChordMapResponse(cmr);
                
                if (cmr->seq == 0) {
                    // A deadend response
                    this->state = ChordStatus::MAPPING_COMPLETED;
                }
                
                break;
            }
            case MTYPE_JOIN_SUCCESSOR_QUERY:
                dprt << "New JoinSuccessorQuery";
            case MTYPE_FINGER_QUERY:
                dprt << "New FingerQuery";
            case MTYPE_SUCCESSOR_QUERY:
            {
                dprt << "New SuccessorQuery";
                SuccessorQuery *sq = (SuccessorQuery *) msg;
                
                if (strcmp(sq->sender, this->ipaddr) == 0) {
                    // Happens if the packet I sent looped back to me
                    SuccessorResponse *sr = MessageHandler::createSuccessorResponse(
                            sq->searchTerm,
                            this->appPort,
                            this->ipaddr
                    );
                    
                    if (type == MTYPE_FINGER_QUERY) {
                        this->fingers[sq->searchTerm] = createNode();
                    } else {
                        this->pushSuccessorResponse(sr);
                    }
                } else if (this->successor->isSelf) {
                    // If no successor and predecessor, this is a single node or first node in chord
                    SuccessorResponse *sr = MessageHandler::createSuccessorResponse(
                            sq->searchTerm,
                            this->appPort,
                            this->ipaddr
                    );
                    
                    if (type == MTYPE_FINGER_QUERY) {
                        sr->type = MTYPE_FINGER_RESPONSE;
                    }
                    
                    node *tmp = createNode(sq->sender);
                    this->successor = tmp;
                    this->successor->appPort = sq->appPort;
                    this->send(tmp, MessageHandler::serialize(sr), sr->size);
                    
                    delete[] sr->responder;
                    delete sr;
                } else if (this->isInSuccessor(sq->searchTerm)) {
                    /*
                     * If ID satisfies successor requirement: > this id && <= successor id
                     * If ID > my id and, successor id < my id, then this is the last node clockwise in chord
                     * In both cases, send successor info to the requestor
                     */
                    SuccessorResponse *sr = MessageHandler::createSuccessorResponse(
                            sq->searchTerm,
                            this->successor->appPort,
                            this->successor->ipaddr
                    );
                    
                    if (type == MTYPE_FINGER_QUERY) {
                        sr->type = MTYPE_FINGER_RESPONSE;
                    }
                    
                    this->send(this->createNode(sq->sender), MessageHandler::serialize(sr), sr->size);
                    
                    delete[] sr->responder;
                    delete sr;
                } else {
                    // Forward the request to the successor
                    if (type == MTYPE_SUCCESSOR_QUERY || type == MTYPE_JOIN_SUCCESSOR_QUERY) {
                        if (type == MTYPE_JOIN_SUCCESSOR_QUERY) {
                            this->send(this->getSuccessorOf(sq->searchTerm, false), MessageHandler::serialize(sq), sq->size);
                        } else {
                            this->send(this->getSuccessorOf(sq->searchTerm, true), MessageHandler::serialize(sq), sq->size);
                        }
                    }
                }
                
                delete[] sq->sender;
                delete sq;
                break;
            }
            case MTYPE_FINGER_RESPONSE:
            case MTYPE_SUCCESSOR_RESPONSE:
            {   
                //dprt << "New SuccessorResponse";
                SuccessorResponse *sr = (SuccessorResponse *) msg;
                
                if (type == MTYPE_FINGER_RESPONSE) {
                    pthread_mutex_lock(&(this->fingerMutex));
                    this->fingers[sr->searchTerm] = createNode(sr->responder);
                    this->fingers[sr->searchTerm]->appPort = sr->appPort;
                    
                    dprt << "Finger Response from " << fingers[sr->searchTerm]->hostname;
                    
                    pthread_mutex_unlock(&(this->fingerMutex));
                } else {
                    this->pushSuccessorResponse(sr);
                }

                break;
            }
            default:
                dprt << "Cannot identify type";
        }
        
        usleep(100000);
    }
}

/**
 * Start querying on the successor chains
 * 
 * @param   key         Key to search for
 * @param   *hostip     Pointer to a pointer to character array. This will be set to the host
 *                      that possesses key upon function returns
 * @param   &hostport   Indicating which port the host that possesses the key runs on
 * @param   timeout     How long to wait for query, specified in milliseconds. Default is 0
 * @return  The key being queried
 */
char *Chord::query(char *key, char **hostip, unsigned int &hostport, unsigned int timeout) {
    if (key == NULL) {
        return NULL;
    }
    
    if (this->successor == NULL || this->successor->isSelf) {
        *hostip = new char[strlen(this->ipaddr) + 1];
        strcpy(*hostip, this->ipaddr);
        hostport = this->appPort;
        return key;
    }
    
    // If this successor may have it
    unsigned int keyhash = this->getConsistentHash(key, strlen(key) + 1);
    if (this->isInSuccessor(keyhash)) {
        *hostip = new char[strlen(this->successor->ipaddr) + 1];
        strcpy(*hostip, this->successor->ipaddr);
        hostport = this->successor->appPort;
        
        dprt << "Setting hostip to " << *hostip << " and port to " << hostport;
        return key;
    }
    
    node *sendto = this->getSuccessorOf(keyhash);
    
    // If the successor does not have it, forward it to the successor and let him deal with it
    SuccessorQuery *sq = MessageHandler::createSuccessorQuery(keyhash, this->appPort, this->ipaddr);
    unsigned char *serialized = MessageHandler::serialize(sq);
    
    this->send(sendto, serialized, sq->size);
    this->pushSendTimer(sendto, keyhash, serialized, sq->size);
    
    // We deal with microseconds internally
    timeout = timeout * 1000;
    unsigned int startTime = getTimeInUSeconds();
    
    SuccessorResponse *sr = NULL;
    while (timeout == 0 || startTime + timeout > getTimeInUSeconds()) {
        // Wait for a new SuccessResponse from any node
        sr = this->popSuccessorResponse();
        if (sr == NULL) {
            usleep(100000);
            continue;
        }
        
        // If this is not for me, ignore
        if (sr->searchTerm == keyhash) {
            *hostip = new char[strlen(sr->responder) + 1];
            strcpy(*hostip, sr->responder);
            hostport = sr->appPort;
            
            dprt << "Setting hostip to " << *hostip << " and port to " << hostport;
            break;
        } else {
            usleep(100000);
            continue;
        }
    }
    
    // Cancel timer
    this->unsetSendTimer(keyhash);
    
    if (sr != NULL) {
        if (sr->responder != NULL) {
            delete[] sr->responder;
        }
        
        delete sr;
    }
    
    delete[] serialized;
    delete sq;
    
    return key;
}

/**
 * Wait for new message (blocking)
 * 
 * @param   &size       Will be set to received size on return
 * @param   timeout     How long to wait for, in milliseconds
 * @return  Pointer to the received message format (already unserialized)
 */
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
    if (ret == 0) {
        // Timeout, set size to -2
        size = -2;
        return NULL;
    } else {
        // Try to receive
        size = recvfrom(this->chord_sfd, buffer, 1024, 0, NULL, 0);

        if (size == -1) {
            dprt << "Cannot receive: " << strerror(errno);
            this->setErrorno(ERR_CONN_LOST);
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

/**
 * Return self's ID
 * 
 * @return  Integer indicating the calculated ID
 */
unsigned int Chord::getHashedId() {
    return this->hashedId;
}

/**
 * Return hashed data of parametre
 * 
 * @param   key     Key to hash
 * @return  Integer indicating the hashed value
 */
unsigned int Chord::getHashedKey(char *key){
    if (key == NULL) {
        this->setErrorno(ERR_INVALID_KEY);
        return 0;
    }
    
    return this->getConsistentHash(key, strlen(key) + 1);
}

/**
 * Return the successor
 * 
 * @return  Successor node structure. NULL if no successor
 */
node *Chord::getSuccessor() {
    return this->successor;
}

/**
 * Sets the IP of the host to join Chord network from
 * 
 * @param   toJoin  The IP address of the host to join.
 *                  If NULL, starts a new Chord network with only this node
 */
void Chord::setJoinPointIp(char *toJoin) {
    this->joinPointIp = toJoin;
}

/**
 * Attempts to join a chord network
 * 
 * @return  True if join succeeded, False otherwise (sets the ChordError number)
 */
bool Chord::join() {
    if (this->state != ChordStatus::INITIALIZED) {
        this->setErrorno(ERR_NOT_INITIALIZED);
        return false;
    }
    
    this->state = ChordStatus::WAITING_TO_JOIN;
    
    if (this->joinPointIp == NULL || strcmp(this->joinPointIp, this->ipaddr) == 0) {
        // If ipaddr == NULL or == self, create new Chord ring
        this->successor = this->createNode();
    } else {
        // Construct successor request
        SuccessorQuery *squery = MessageHandler::createSuccessorQuery(this->hashedId, this->appPort, this->ipaddr);
        squery->type = MTYPE_JOIN_SUCCESSOR_QUERY;
        unsigned char *serializedData = MessageHandler::serialize(squery);

        // Create a node struct for sending, using the receiver's ipaddress
        node *sendto = this->createNode(this->joinPointIp);
        
        void *msg = NULL;
        unsigned int timeoutCount = 0;
        
        // Try to join for JOIN_TRIALS times
        while (timeoutCount < JOIN_TRIALS) {
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
                if (MessageHandler::getType(msg) != MTYPE_SUCCESSOR_RESPONSE) {
                    continue;   // Ignore if not expected type
                }

                break;
            }
        }
        
        // If JOIN_TRIALS times of timeout occured, join failed and return
        if (timeoutCount == JOIN_TRIALS) {
            dprt << "Timeout occured while attempted to join " << ipaddr;
            this->setErrorno(ERR_CANNOT_JOIN_CHORD);
            return false;
        }

        // Received a proper response;
        SuccessorResponse *sr = (SuccessorResponse *) msg;
        if (sr->responder == NULL) {
            this->successor = NULL;
        } else {
            this->successor = this->createNode(sr->responder);
            this->successor->appPort = sr->appPort;
        }
        
        delete squery;
        delete sendto;
        delete sr;
    }
    
    this->state = ChordStatus::IN_NETWORK;
    this->notifySuccessor();
    return true;
}

/**
 * Notifies the successor of this node about the changing predecessor
 */
void Chord::notifySuccessor() {
    if (this->successor != NULL && !this->successor->isSelf) {
        UpdatePredcessor *up = MessageHandler::createUpdatePredecessor(this->appPort, this->ipaddr);
        unsigned char *serialized = MessageHandler::serialize(up);
        
        this->send(this->successor, serialized, up->size);
        this->pushSendTimer(this->successor, this->hashedId, serialized, up->size);
    }
}

/**
 * Creates a new node structure using the IP address. Also sets up UDP socket for this node
 * The port will be fixed for the same chord network, so the own chordPort will be used
 * 
 * @param   ipaddr  The IP address to create node structure for
 * @return  Pointer to node structure (node *) with the connection information for the IP
 */
node *Chord::createNode(char *ipaddr) {
    if (ipaddr == NULL) {
        ipaddr = this->ipaddr;
    }
    
    // Creates a new node object
    node *n = new node();
    char *hostname = getHostname(ipaddr);
    n->hashedId = this->getConsistentHash(ipaddr, strlen(ipaddr) + 1);
    n->ipaddr = new char[strlen(ipaddr) + 1];
    n->hostname = new char[strlen(hostname) + 1];
    strcpy(n->ipaddr, ipaddr);
    strcpy(n->hostname, hostname);
    
    if (strcmp(ipaddr, this->ipaddr) == 0) {
        // This node is myself
        n->isSelf = true;
        n->appPort = this->appPort;
        n->sfd = 0;
        n->addr = NULL;
        n->len = 0;
    } else {
        // Not myself
        n->isSelf = false;
        n->appPort = 0;
        
        // Set up connection information
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
        
        n->sfd = socket_fd;
        n->addr = res->ai_addr;
        n->len = res->ai_addrlen;
    }
    
    return n;
}

/**
 * Returns a text represented finger table, containing hashed ID and node name
 * 
 * @return  String containing the finger table entries
 */
char *Chord::getFingerTable() {
    stringstream ss;
    pthread_mutex_lock(&(this->fingerMutex));
    for (map<uint32_t, node *>::iterator it = this->fingers.begin(); it != this->fingers.end(); ++it) {
        if (it->second == NULL) {
            ss << setw(10) << setfill(' ') << it->first << ": NULL\n";
        } else {
            ss << setw(10) << setfill(' ') << it->first << ": " << getComputerName(it->second->hostname)
               << " # " << it->second->hashedId << "\n";
        }
    }
    pthread_mutex_unlock(&(this->fingerMutex));
    
    return cstr(ss.str());
}

/**
 * Get the ring map of the chord in texual format
 * e.g. [server1]=>[server2]=>[server5]=>[server4]=>[server1] (end)
 * 
 * @return  A textual representation of the current map
 */
char *Chord::getChordMap() {
    if (this->state != ChordStatus::SERVICING) {
        // If the service is not working, then there is no map
        this->setErrorno(ERR_NOT_IN_SERVICE);
        return NULL;
    } else if (this->successor == NULL || this->successor->isSelf) {
        this->setErrorno(ERR_NO_SUCCESSOR);
        return NULL;
    }
    
    this->state = ChordStatus::MAPPING_CHORD;
    // Query sequence starts with 1, 0 means deadend
    ChordMapQuery *cmq = MessageHandler::createChordMapQuery(1, this->ipaddr);
    
    // A sequence number to host IP mapping
    map<uint32_t, char *> chordMap;
    // First one is of course current node
    chordMap[1] = this->ipaddr;
    
    // Sends the query request to successor
    unsigned char *serialized = MessageHandler::serialize(cmq);
    this->send(this->successor, serialized, cmq->size);
    
    // Wait until the mapping has been completed
    while (this->state != ChordStatus::MAPPING_COMPLETED) {
        usleep(100000);
        continue;
    }
    
    // See if there is any new response
    ChordMapResponse *cmr = this->popChordMapResponse();
    while (cmr != NULL) {
        // If so, sort them into the map for printout
        chordMap[cmr->seq] = cmr->responder;
        cmr = this->popChordMapResponse();
    }
    
    // Deadend means the chord ring is broken (no successor), maybe due to crashes
    // seq of dead ended nodes are set to 0
    bool deadend = false;
    stringstream mapstr;
    for (map<uint32_t, char *>::iterator it = chordMap.begin(); it != chordMap.end(); ++it) {
        if (it->first == 0) {
            deadend = true;
            continue;
        }
        
        mapstr << "[" << getComputerName(getHostname(it->second)) << "]-->";
    }
    
    mapstr << "[" << getComputerName(this->hostname) << "]";
    if (deadend) {
        mapstr << "-->[" << chordMap[0] << "] (Deadend)";
    } else {
        mapstr << " (End)";
    }
    
    this->state = ChordStatus::SERVICING;
    return cstr(mapstr.str());
}

/**
 * Sends message to the specified node
 * 
 * @param   n       The node to send to
 * @param   data    The data to send to (must be serialized with MessageHandler)
 * @param   len     The length of data
 * @param   flag    The flag to use for sending (same as system call sendto flags)
 * @return  Size sent; -1 if error
 */
size_t Chord::send(node *n, unsigned char *data, size_t len, int flag) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t size = sendto(n->sfd, data + sent, len - sent, flag, n->addr, n->len);
        
        if (size == -1) {
            cerr << "[ERROR] Problem sending data: " << strerror(errno) << endl;
            return size;
        }
        
        sent += size;
    }
    
    return sent;
}

/**
 * Calculates the consistent hashing of the specified parametre.
 * 
 * @param   tohash  The item to calculate hash for
 * @param   len     The length of tohash
 * @return  The calculated hash
 */
unsigned int Chord::getConsistentHash(char *tohash, size_t len) {
    unsigned char *hash = new unsigned char[160];
    SHA1((unsigned char *) tohash, len, hash);
    
    stringstream ss;
    for(int i = 0; i < 20; ++i) {
        ss << hex << setw(2) << setfill('0') << (int)hash[i];
    }
    
    // Uses large number library (GMP) because SHA1 hash is longer than C++ unsigned long long bit length
    mpz_t chash, largenum;
    mpz_inits(chash, largenum, NULL);
    mpz_set_str(largenum, cstr(ss.str()), 16);
    // Calculates modulo
    mpz_mod_ui(chash, largenum, pow((long double) 2, (long double) CHORD_LENGTH_BIT));
    
    unsigned int pos = (unsigned int) mpz_get_ui(chash);
    dprt << "Key/Node " << tohash << " maps to " << pos;
    
    return pos;
}

/**
 * Pushes received successor response to queue
 * 
 * @param   sr  The message to push
 */
void Chord::pushSuccessorResponse(SuccessorResponse *sr) {
    pthread_mutex_lock(&(this->successorResponseQueueMutex));
    this->successorResponseQueue.push_back(sr);
    pthread_mutex_unlock(&(this->successorResponseQueueMutex));
}

/**
 * Returns and removes the first item in successor response queue, effectively
 * reduces the queue's size by 1
 * 
 * @return  The first item in the successor response queue. NULL if queue is emtpy
 */
SuccessorResponse *Chord::popSuccessorResponse() {
    SuccessorResponse *ret = NULL;
    
    pthread_mutex_lock(&(this->successorResponseQueueMutex));
    vector<SuccessorResponse *>::iterator it = this->successorResponseQueue.begin();
    if (it != this->successorResponseQueue.end()) {
        // Has item
        ret = (*it);
        this->successorResponseQueue.erase(it);
    }
    pthread_mutex_unlock(&(this->successorResponseQueueMutex));
    
    return ret;
}

/**
 * Pushes received chord map response to queue
 * 
 * @param   cmr  The message to push
 */
void Chord::pushChordMapResponse(ChordMapResponse *cmr) {
    pthread_mutex_lock(&(this->chordMapResponseQueueMutex));
    this->chordMapResponseQueue.push_back(cmr);
    pthread_mutex_unlock(&(this->chordMapResponseQueueMutex));
}

/**
 * Returns and removes the first item in chord map queue, effectively
 * reduces the queue's size by 1
 * 
 * @return  The first item in the chord map queue. NULL if queue is emtpy
 */
ChordMapResponse *Chord::popChordMapResponse() {
    ChordMapResponse *ret = NULL;
    
    pthread_mutex_lock(&(this->chordMapResponseQueueMutex));
    vector<ChordMapResponse *>::iterator it = this->chordMapResponseQueue.begin();
    if (it != this->chordMapResponseQueue.end()) {
        // Has item
        ret = (*it);
        this->chordMapResponseQueue.erase(it);
    }
    pthread_mutex_unlock(&(this->chordMapResponseQueueMutex));
    
    return ret;
}

/**
 * Pushes new timer with its idenifier to the list to be watched
 * 
 * @param   sendTo      The node the message was sent to
 * @param   searchTerm  The key, used to ID this message
 * @param   data        The serialized data sent
 * @param   len         The length of data
 */
void Chord::pushSendTimer(node *sendTo, uint32_t searchTerm, unsigned char *data, size_t len) {
    msgTimer *mtimer = new msgTimer();
    mtimer->recipient = sendTo;
    mtimer->context = new unsigned char[len];
    memcpy(mtimer->context, data, len);
    
    pthread_mutex_lock(&(this->sendTimerMutex));
    this->sendTimers[searchTerm] = mtimer;
    (this->sendTimers[searchTerm])->timestamp = getTimeInUSeconds();
    pthread_mutex_unlock(&(this->sendTimerMutex));
}

/**
 * Removes a timer associated with the parametre ID
 * 
 * @param   searchTerm  The item to remove
 */
void Chord::unsetSendTimer(uint32_t searchTerm) {
    pthread_mutex_lock(&(this->sendTimerMutex));
    map<uint32_t, msgTimer *>::iterator it = this->sendTimers.find(searchTerm);
    if (it != this->sendTimers.end()) {
        delete[] it->second->context;
        delete it->second;
        this->sendTimers.erase(it);
    }
    pthread_mutex_unlock(&(this->sendTimerMutex));
}

/**
 * Calculates to see if key is within successor range for the next successor
 * 
 * @param   key     The key to check
 * @param   start   Starting ID or hash value. Default is the Chord node's hash ID
 * @param   end     Ending ID or hash value. Default is the hash ID of the successor
 * @return  True if it is within the next successor, False otherwise
 */
bool Chord::isInSuccessor(uint32_t key, uint32_t start, uint32_t end) {
    if (start == 0) {
        start = this->hashedId;
    }
    
    if (end == 0) {
        end = this->successor->hashedId;
    }
    
    if ((key > start && key <= end)
            || (start > end && key <= end)
            || (start > end && key > start)){
        return true;
    }
    
    return false;
}

/**
 * Get the successor of key
 * 
 * @param   key         The key to get successor of
 * @param   useFinger   Whether to use finger table or not. Default is true
 * @return  The node pointer pointing to the successor node structure
 */
node *Chord::getSuccessorOf(uint32_t key, bool useFinger) {
    if (!useFinger) {
        return this->successor;
    }
    
    node *ret = NULL;
    map<uint32_t, node *>::reverse_iterator it;
    
    pthread_mutex_lock(&(this->fingerMutex));
    for (it = this->fingers.rbegin(); it != this->fingers.rend(); ++it) {
        if (it->second == NULL) {
            continue;
        }
        
        if (it->first < key) {
            if ((--it) != this->fingers.rbegin()) {
                ret = it->second;
            }
            
            break;
        }
    }
    pthread_mutex_unlock(&(this->fingerMutex));
    
    if (ret == NULL) {
        ret = this->successor;
    }

    return ret;
}

/**
 * Returns the current state of the service
 * 
 * @return  ChordStatus::status indicating the Chord system state
 */
ChordStatus::status Chord::getState() {
    return this->state;
}
