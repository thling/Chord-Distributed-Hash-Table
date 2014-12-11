#include <cstdlib>

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
    
    switch (this->getType(msg)) {
        case MTYPE_KEY_QUERY:
        {
            KeyQuery *cquery = (KeyQuery *) msg;
            ret = new unsigned char[cquery->size];
            memcpy(ret, itobc(htonl(cquery->type)), 4);
            memcpy(ret + 4, itobc(htonl(cquery->size)), 4);
            memcpy(ret + 8, itobc(htonl(cquery->key)), 4);
            memcpy(ret + 12, cquery->sender, cquery->size - 12);
            break;
        }
        case MTYPE_KEY_QUERY_RESPONSE:
        {
            KeyQueryResponse *cqr = (KeyQueryResponse *) msg;
            ret = new unsigned char[cqr->size];
            memcpy(ret, itobc(htonl(cqr->type)), 4);
            memcpy(ret + 4, itobc(htonl(cqr->size)), 4);
            memcpy(ret + 8, itobc(htonl(cqr->port)), 4);
            memcpy(ret + 12, cqr->owner, cqr->size - 12);
            break;
        }
        case MTYPE_SUCCESSOR_QUERY:
        {
            SuccessorQuery *squery = (SuccessorQuery *) msg;
            ret = new unsigned char[squery->size];
            memcpy(ret, itobc(htonl(squery->type)), 4);
            memcpy(ret + 4, itobc(htonl(squery->size)), 4);
            memcpy(ret + 8, squery->sender, squery->size - 8);
            break;
        }
        case MTYPE_SUCCESSOR_QUERY_RESPONSE:
        {
            SuccessorQueryResponse *sqr = (SuccessorQueryResponse *) msg;
            ret = new unsigned char[sqr->size];
            memcpy(ret, itobc(htonl(sqr->type)), 4);
            memcpy(ret + 4, itobc(htonl(sqr->size)), 4);
            memcpy(ret + 8, sqr->responder, sqr->size - 8);
            break;
        }
        default:
            cerr << "Cannot identify message type: " << this->getType(msg) << endl;
    }
    
    return ret;
}

void *MessageHandler::unserialize(unsigned char *byteStream) {
    switch (this->getType(byteStream)) {
        case MTYPE_KEY_QUERY:
        {
            KeyQuery *cquery = new KeyQuery();
            cquery->type = MTYPE_KEY_QUERY;
            cquery->size = ntohl(bctoi(byteStream + 4));
            cquery->key = ntohl(bctoi(byteStream + 8));
            cquery->sender = new char[cquery->size - 12];
            memcpy(cquery->sender, byteStream + 12, cquery->size - 12);
            return cquery;
        }
        case MTYPE_KEY_QUERY_RESPONSE:
        {
            KeyQueryResponse *cqr = new KeyQueryResponse();
            cqr->type = MTYPE_KEY_QUERY_RESPONSE;
            cqr->size = ntohl(bctoi(byteStream + 4));
            cqr->port = ntohl(bctoi(byteStream + 8));
            cqr->owner = new char[cqr->size - 12];
            memcpy(cqr->owner, byteStream + 12, cqr->size - 12);
            return cqr;
        }
        case MTYPE_SUCCESSOR_QUERY:
        {
            SuccessorQuery *squery = new SuccessorQuery();
            squery->type = MTYPE_KEY_QUERY;
            squery->size = ntohl(bctoi(byteStream + 4));
            squery->sender = new char[squery->size - 8];
            memcpy(squery->sender, byteStream + 8, squery->size - 8);
            return squery;
        }
        case MTYPE_SUCCESSOR_QUERY_RESPONSE:
        {
            SuccessorQueryResponse *sqr = new SuccessorQueryResponse();
            sqr->type = MTYPE_SUCCESSOR_QUERY_RESPONSE;
            sqr->size = ntohl(bctoi(byteStream + 4));
            sqr->responder = new char[sqr->size - 8];
            memcpy(sqr->responder, byteStream + 8, sqr->size - 8);
            return sqr;
        }
        default:
            dprt << "Cannot identify message type: " << this->getType(byteStream);
            return NULL;
    }
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
