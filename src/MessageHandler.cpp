#include <cstdlib>
#include <cstring>

#include "../include/MessageHandler.hpp"
#include "../include/MessageTypes.hpp"
#include "../include/Utils.hpp"

using namespace std;

/**
 * Serializes the parametre message based on its type
 * 
 * @param   msg     The item to serialize
 * @return  Byte array with serialized data
 */
unsigned char *MessageHandler::serialize(void *msg) {
    if (msg == NULL) {
        dprt << "Serialize target cannot be NULL.";
        return NULL;
    }
    
    unsigned char *ret = NULL;
    
    switch (MessageHandler::getType(msg)) {
        case MTYPE_UPDATE_PREDECESSOR:
        {
            UpdatePredcessor *up = (UpdatePredcessor *) msg;
            ret = new unsigned char[up->size];
            memcpy(ret, itobc(htonl(up->type)), 4);
            memcpy(ret + 4, itobc(htonl(up->size)), 4);
            memcpy(ret + 8, itobc(htonl(up->appPort)), 4);
            if (up->size - 12 > 0) {
                memcpy(ret + 12, up->predecessor, up->size - 12);
            }
            break;
        }
        case MTYPE_UPDATE_PREDECESSOR_ACK:
        {
            UpdatePredcessorAck *upAck = (UpdatePredcessorAck *) msg;
            ret = new unsigned char[upAck->size];
            memcpy(ret, itobc(htonl(upAck->type)), 4);
            memcpy(ret + 4, itobc(htonl(upAck->size)), 4);
            memcpy(ret + 8, itobc(htonl(upAck->hashedId)), 4);
            break;
        }
        case MTYPE_STABILIZE_REQUEST:
        {
            StabilizeRequest *streq = (StabilizeRequest *) msg;
            ret = new unsigned char[streq->size];
            memcpy(ret, itobc(htonl(streq->type)), 4);
            memcpy(ret + 4, itobc(htonl(streq->size)), 4);
            memcpy(ret + 8, itobc(htonl(streq->appPort)), 4);
            if (streq->size - 12 > 0) {
                memcpy(ret + 12, streq->sender, streq->size - 12);
            }
            break;
        }
        case MTYPE_STABILIZE_RESPONSE:
        {
            StabilizeResponse *stres = (StabilizeResponse *) msg;
            ret = new unsigned char[stres->size];
            memcpy(ret, itobc(htonl(stres->type)), 4);
            memcpy(ret + 4, itobc(htonl(stres->size)), 4);
            memcpy(ret + 8, itobc(htonl(stres->appPort)), 4);
            if (stres->size - 12 > 0) {
                memcpy(ret + 12, stres->predecessor, stres->size - 12);
            }
            break;
        }
        case MTYPE_CHORD_MAP_QUERY:
        {
            ChordMapQuery *cmq = (ChordMapQuery *) msg;
            ret = new unsigned char[cmq->size];
            memcpy(ret, itobc(htonl(cmq->type)), 4);
            memcpy(ret + 4, itobc(htonl(cmq->size)), 4);
            memcpy(ret + 8, itobc(htonl(cmq->seq)), 4);
            if (cmq->size - 12 > 0) {
                memcpy(ret + 12, cmq->sender, cmq->size - 12);
            }
            break;
        }
        case MTYPE_CHORD_MAP_RESPONSE:
        {
            ChordMapResponse *cmr = (ChordMapResponse *) msg;
            ret = new unsigned char[cmr->size];
            memcpy(ret, itobc(htonl(cmr->type)), 4);
            memcpy(ret + 4, itobc(htonl(cmr->size)), 4);
            memcpy(ret + 8, itobc(htonl(cmr->seq)), 4);
            if (cmr->size - 12 > 0) {
                memcpy(ret + 12, cmr->responder, cmr->size - 12);
            }
            break;
        }
        case MTYPE_SUCCESSOR_QUERY:
        {
            SuccessorQuery *squery = (SuccessorQuery *) msg;
            ret = new unsigned char[squery->size];
            memcpy(ret, itobc(htonl(squery->type)), 4);
            memcpy(ret + 4, itobc(htonl(squery->size)), 4);
            memcpy(ret + 8, itobc(htonl(squery->searchTerm)), 4);
            memcpy(ret + 12, itobc(htonl(squery->appPort)), 4);
            if (squery->size - 16 > 0) {
                memcpy(ret + 16, squery->sender, squery->size - 16);
            }
            break;
        }
        case MTYPE_SUCCESSOR_RESPONSE:
        {
            SuccessorResponse *sqr = (SuccessorResponse *) msg;
            ret = new unsigned char[sqr->size];
            memcpy(ret, itobc(htonl(sqr->type)), 4);
            memcpy(ret + 4, itobc(htonl(sqr->size)), 4);
            memcpy(ret + 8, itobc(htonl(sqr->searchTerm)), 4);
            memcpy(ret + 12, itobc(htonl(sqr->appPort)), 4);
            if (sqr->size - 16 > 0) {
                memcpy(ret + 16, sqr->responder, sqr->size - 16);
            }
            break;
        }
        default:
            cerr << "Cannot identify message type: " << MessageHandler::getType(msg) << endl;
    }
    
    return ret;
}

/**
 * Decode received serialized data into corresponding message
 * 
 * @param   byteStream  The received byte array to unserialize
 * @return  Pointer to a type of message specified in MessageTypes.hpp; need casting
 */
void *MessageHandler::unserialize(unsigned char *byteStream) {
    switch (MessageHandler::getType(byteStream)) {
        case MTYPE_UPDATE_PREDECESSOR:
        {
            UpdatePredcessor *up = new UpdatePredcessor();
            up->type = MTYPE_UPDATE_PREDECESSOR;
            up->size = ntohl(bctoi(byteStream + 4));
            up->appPort = ntohl(bctoi(byteStream + 8));
            if (up->size - 12 > 0) {
                up->predecessor = new char[up->size - 12];
                memcpy(up->predecessor, byteStream + 12, up->size - 12);
            } else {
                up->predecessor = NULL;
            }
            
            return up;
        }
        case MTYPE_UPDATE_PREDECESSOR_ACK:
        {
            UpdatePredcessorAck *upAck = new UpdatePredcessorAck();
            upAck->type = MTYPE_UPDATE_PREDECESSOR_ACK;
            upAck->size = ntohl(bctoi(byteStream + 4));
            upAck->hashedId = ntohl(bctoi(byteStream + 8));
            
            return upAck;
        }
        case MTYPE_STABILIZE_REQUEST:
        {
            StabilizeRequest *streq = new StabilizeRequest();
            streq->type = MTYPE_STABILIZE_REQUEST;
            streq->size = ntohl(bctoi(byteStream + 4));
            streq->appPort = ntohl(bctoi(byteStream + 8));
            if (streq->size - 12 > 0) {
                streq->sender = new char[streq->size - 12];
                memcpy(streq->sender, byteStream + 12, streq->size - 12);
            } else {
                streq->sender = NULL;
            }
            
            return streq;
        }
        case MTYPE_STABILIZE_RESPONSE:
        {
            StabilizeResponse *stres = new StabilizeResponse();
            stres->type = MTYPE_STABILIZE_RESPONSE;
            stres->size = ntohl(bctoi(byteStream + 4));
            stres->appPort  = ntohl(bctoi(byteStream + 8));
            if (stres->size - 12 > 0) {
                stres->predecessor = new char[stres->size - 12];
                memcpy(stres->predecessor, byteStream + 12, stres->size - 12);
            } else {
                stres->predecessor = NULL;
            }
            
            return stres;
        }
        case MTYPE_CHORD_MAP_QUERY:
        {
            ChordMapQuery *cmq = new ChordMapQuery();
            cmq->type = MTYPE_CHORD_MAP_QUERY;
            cmq->size = ntohl(bctoi(byteStream + 4));
            cmq->seq = ntohl(bctoi(byteStream + 8));
            if (cmq->size - 12 > 0) {
                cmq->sender = new char[cmq->size - 12];
                memcpy(cmq->sender, byteStream + 12, cmq->size - 12);
            } else {
                cmq->sender = NULL;
            }
            return cmq;
        }
        case MTYPE_CHORD_MAP_RESPONSE:
        {
            ChordMapResponse *cmr = new ChordMapResponse();
            cmr->type = MTYPE_CHORD_MAP_RESPONSE;
            cmr->size = ntohl(bctoi(byteStream + 4));
            cmr->seq = ntohl(bctoi(byteStream + 8));
            if (cmr->size - 12 > 0) {
                cmr->responder = new char[cmr->size - 12];
                memcpy(cmr->responder, byteStream + 12, cmr->size - 12);
            } else {
                cmr->responder = NULL;
            }
            return cmr;
        }
        case MTYPE_SUCCESSOR_QUERY:
        {
            SuccessorQuery *squery = new SuccessorQuery();
            squery->type = MTYPE_SUCCESSOR_QUERY;
            squery->size = ntohl(bctoi(byteStream + 4));
            squery->searchTerm = ntohl(bctoi(byteStream + 8));
            squery->appPort = ntohl(bctoi(byteStream + 12));
            if (squery->size - 16 > 0) {
                squery->sender = new char[squery->size - 16];
                memcpy(squery->sender, byteStream + 16, squery->size - 16);
            } else {
                squery->sender = NULL;
            }
            return squery;
        }
        case MTYPE_SUCCESSOR_RESPONSE:
        {
            SuccessorResponse *sqr = new SuccessorResponse();
            sqr->type = MTYPE_SUCCESSOR_RESPONSE;
            sqr->size = ntohl(bctoi(byteStream + 4));
            sqr->searchTerm = ntohl(bctoi(byteStream + 8));
            sqr->appPort = ntohl(bctoi(byteStream + 12));
            if (sqr->size - 16 > 0) {
                sqr->responder = new char[sqr->size - 16];
                memcpy(sqr->responder, byteStream + 16, sqr->size - 16);
            } else {
                sqr->responder = NULL;
            }
            return sqr;
        }
        default:
            dprt << "Cannot identify message type: " << MessageHandler::getType(byteStream);
            return NULL;
    }
}

SuccessorQuery *MessageHandler::createSuccessorQuery(uint32_t searchTerm, uint32_t appPort, char *sender) {
    SuccessorQuery *sq = new SuccessorQuery();
    sq->type = MTYPE_SUCCESSOR_QUERY;
    sq->size = 16 + strlen(sender) + 1;
    sq->searchTerm = searchTerm;
    sq->appPort = appPort;
    sq->sender = new char[sq->size - 16];
    strcpy(sq->sender, sender);
    
    return sq;
}

SuccessorResponse *MessageHandler::createSuccessorResponse(uint32_t searchTerm, uint32_t appPort, char *responder) {
    SuccessorResponse *sqr = new SuccessorResponse();
    sqr->type = MTYPE_SUCCESSOR_RESPONSE;
    sqr->size = 16 + strlen(responder) + 1;
    sqr->searchTerm = searchTerm;
    sqr->appPort = appPort;
    sqr->responder = new char[sqr->size - 16];
    strcpy(sqr->responder, responder);
    
    return sqr;
}

ChordMapQuery *MessageHandler::createChordMapQuery(uint32_t seq, char *sender) {
    ChordMapQuery *cmq = new ChordMapQuery();
    cmq->type = MTYPE_CHORD_MAP_QUERY;
    cmq->size = 12 + strlen(sender) + 1;
    cmq->seq = seq;
    cmq->sender = new char[cmq->size - 12];
    strcpy(cmq->sender, sender);
    
    return cmq;
}

ChordMapResponse *MessageHandler::createChordMapResponse(uint32_t seq, char *responder) {
    ChordMapResponse *cmr = new ChordMapResponse();
    cmr->type = MTYPE_CHORD_MAP_RESPONSE;
    cmr->size = 12 + strlen(responder) + 1;
    cmr->seq = seq;
    cmr->responder = new char[cmr->size - 12];
    strcpy(cmr->responder, responder);
    
    return cmr;
}

UpdatePredcessor *MessageHandler::createUpdatePredecessor(uint32_t appPort, char *predecessor) {
    UpdatePredcessor *up = new UpdatePredcessor();
    up->type = MTYPE_UPDATE_PREDECESSOR;
    up->size = 12 + strlen(predecessor) + 1;
    up->appPort = appPort;
    up->predecessor = new char[up->size - 12];
    strcpy(up->predecessor, predecessor);
    
    return up;
}

UpdatePredcessorAck *MessageHandler::createUpdatePredecessorAck(uint32_t hashedId) {
    UpdatePredcessorAck *upAck = new UpdatePredcessorAck();
    upAck->type = MTYPE_UPDATE_PREDECESSOR_ACK;
    upAck->size = 12;
    upAck->hashedId = hashedId;
    
    return upAck;
}

StabilizeRequest *MessageHandler::createStabilizeRequest(uint32_t appPort, char *sender) {
    StabilizeRequest *streq = new StabilizeRequest();
    streq->type = MTYPE_STABILIZE_REQUEST;
    streq->size = 12 + strlen(sender) + 1;
    streq->appPort = appPort;
    streq->sender = new char[streq->size - 12];
    strcpy(streq->sender, sender);
    
    return streq;
}

StabilizeResponse *MessageHandler::createStabilizeResponse(uint32_t appPort, char *predecessor) {
    StabilizeResponse *stres = new StabilizeResponse();
    stres->type = MTYPE_STABILIZE_RESPONSE;
    stres->size = 12 + strlen(predecessor) + 1;
    stres->appPort = appPort;
    stres->predecessor = new char[stres->size - 12];
    strcpy(stres->predecessor, predecessor);
    
    return stres;
}

/**
 * Returns the size of the received byte array
 */
unsigned int MessageHandler::getSize(unsigned char *byteStream) {
    return ntohl(bctoi(byteStream + 4));
}

/**
 * Returns the size of the specified message
 */
unsigned int MessageHandler::getSize(void *msg) {
    return ((BaseMessage *) msg)->size;
}

/**
 * Returns the message type of the received byte array
 */
unsigned int MessageHandler::getType(unsigned char *byteStream) {
    return ntohl(bctoi(byteStream));
}

/**
 * Returns the type of the specified message
 */
unsigned int MessageHandler::getType(void *msg) {
    return ((BaseMessage *) msg)->type;
}
