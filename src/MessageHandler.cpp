#include <cstdlib>
#include <cstring>

#include "../include/MessageHandler.hpp"
#include "../include/MessageTypes.hpp"
#include "../include/Utils.hpp"

using namespace std;

unsigned char *MessageHandler::serialize(void *msg) {
    if (msg == NULL) {
        dprt << "Serialize target cannot be NULL.";
        return NULL;
    }
    
    unsigned char *ret = NULL;
    
    switch (MessageHandler::getType(msg)) {
        case MTYPE_KEY_QUERY:
        {
            KeyQuery *kquery = (KeyQuery *) msg;
            ret = new unsigned char[kquery->size];
            memcpy(ret, itobc(htonl(kquery->type)), 4);
            memcpy(ret + 4, itobc(htonl(kquery->size)), 4);
            memcpy(ret + 8, itobc(htonl(kquery->key)), 4);
            if (kquery->size - 12 > 0) {
                memcpy(ret + 12, kquery->sender, kquery->size - 12);
            }
            break;
        }
        case MTYPE_KEY_QUERY_RESPONSE:
        {
            KeyQueryResponse *kqr = (KeyQueryResponse *) msg;
            ret = new unsigned char[kqr->size];
            memcpy(ret, itobc(htonl(kqr->type)), 4);
            memcpy(ret + 4, itobc(htonl(kqr->size)), 4);
            memcpy(ret + 8, itobc(htonl(kqr->port)), 4);
            if (kqr->size - 12 > 0) {
                memcpy(ret + 12, kqr->owner, kqr->size - 12);
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
            if (squery->size - 12 > 0) {
                memcpy(ret + 12, squery->sender, squery->size - 12);
            }
            break;
        }
        case MTYPE_SUCCESSOR_QUERY_RESPONSE:
        {
            SuccessorQueryResponse *sqr = (SuccessorQueryResponse *) msg;
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

void *MessageHandler::unserialize(unsigned char *byteStream) {
    switch (MessageHandler::getType(byteStream)) {
        case MTYPE_KEY_QUERY:
        {
            KeyQuery *kquery = new KeyQuery();
            kquery->type = MTYPE_KEY_QUERY;
            kquery->size = ntohl(bctoi(byteStream + 4));
            kquery->key = ntohl(bctoi(byteStream + 8));
            if (kquery->size - 12 > 0) {
                kquery->sender = new char[kquery->size - 12];
                memcpy(kquery->sender, byteStream + 12, kquery->size - 12);
            } else {
                kquery->sender = NULL;
            }
            return kquery;
        }
        case MTYPE_KEY_QUERY_RESPONSE:
        {
            KeyQueryResponse *kqr = new KeyQueryResponse();
            kqr->type = MTYPE_KEY_QUERY_RESPONSE;
            kqr->size = ntohl(bctoi(byteStream + 4));
            kqr->port = ntohl(bctoi(byteStream + 8));
            if (kqr->size - 12 > 0) {
                kqr->owner = new char[kqr->size - 12];
                memcpy(kqr->owner, byteStream + 12, kqr->size - 12);
            } else {
                kqr->owner = NULL;
            }
            return kqr;
        }
        case MTYPE_SUCCESSOR_QUERY:
        {
            SuccessorQuery *squery = new SuccessorQuery();
            squery->type = MTYPE_SUCCESSOR_QUERY;
            squery->size = ntohl(bctoi(byteStream + 4));
            squery->searchTerm = ntohl(bctoi(byteStream + 8));
            if (squery->size - 12 > 0) {
                squery->sender = new char[squery->size - 12];
                memcpy(squery->sender, byteStream + 12, squery->size - 12);
            } else {
                squery->sender = NULL;
            }
            return squery;
        }
        case MTYPE_SUCCESSOR_QUERY_RESPONSE:
        {
            SuccessorQueryResponse *sqr = new SuccessorQueryResponse();
            sqr->type = MTYPE_SUCCESSOR_QUERY_RESPONSE;
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

SuccessorQuery *MessageHandler::createSQ(uint32_t searchTerm, uint32_t appPort, char *sender) {
    SuccessorQuery *sq = new SuccessorQuery();
    sq->type = MTYPE_SUCCESSOR_QUERY;
    sq->size = 16 + strlen(sender) + 1;
    sq->searchTerm = searchTerm;
    sq->appPort = appPort;
    sq->sender = new char[sq->size - 16];
    strcpy(sq->sender, sender);
    
    return sq;
}

SuccessorQueryResponse *MessageHandler::createSQR(uint32_t searchTerm, uint32_t appPort, char *responder) {
    SuccessorQueryResponse *sqr = new SuccessorQueryResponse();
    sqr->type = MTYPE_SUCCESSOR_QUERY_RESPONSE;
    sqr->size = 16 + strlen(responder) + 1;
    sqr->searchTerm = searchTerm;
    sqr->appPort = appPort;
    sqr->responder = new char[sqr->size - 16];
    strcpy(sqr->responder, responder);
    
    return sqr;
}

unsigned int MessageHandler::getSize(unsigned char *byteStream) {
    return ntohl(bctoi(byteStream + 4));
}

unsigned int MessageHandler::getSize(void *msg) {
    return ((BaseMessage *) msg)->size;
}

unsigned int MessageHandler::getType(unsigned char *byteStream) {
    return ntohl(bctoi(byteStream));
}

unsigned int MessageHandler::getType(void *msg) {
    return ((BaseMessage *) msg)->type;
}
