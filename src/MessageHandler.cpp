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
    unsigned int size = 0;
    
    switch (this->getType(msg)) {
        case MTYPE_CHORD_QUERY:
        {
            ChordQuery *cquery = (ChordQuery *) msg;
            if (cquery->fromId == NULL) {
                dprt << "From ID cannot be empty";
                return NULL;
            } else if (cquery->toId == NULL) {
                dprt << "To ID cannot be empty";
                return NULL;
            } else if (cquery->key == NULL) {
                dprt << "Key cannot be empty";
                return NULL;
            }
            
            // (4B for type) + (4B for size) + (3 * 160B message digests)
            size = 488;
            if (cquery->keyOwner != NULL) size += 160;
            
            ret = new unsigned char[size];
            memcpy(ret, itobc(size), 4);
            memcpy(ret + 4, itobc(cquery->type), 4);
            memcpy(ret + 8, cquery->fromId, 160);
            memcpy(ret + 168, cquery->toId, 160);
            memcpy(ret + 328, cquery->key, 160);
            if (cquery->keyOwner != NULL) {
                memcpy(ret + 488, cquery->keyOwner, 160);
            }
            
            break;
        }
        case MTYPE_SUCCESSOR_QUERY:
        {
            SuccessorQuery *squery = (SuccessorQuery *) msg;
            if (squery->fromId == NULL) {
                dprt << "From ID cannot be empty";
                return NULL;
            } else if (squery->toId == NULL) {
                dprt << "To ID cannot be empty";
                return NULL;    
            } 
            
            // (4B for type) + (4B for size) + (2 * 160B message digests)
            size = 328;
            if (squery->successor != NULL) size += 160;
            
            ret = new unsigned char[size];
            memcpy(ret, itobc(size), 4);
            memcpy(ret + 4, itobc(squery->type), 4);
            memcpy(ret + 8, squery->fromId, 160);
            memcpy(ret + 168, squery->toId, 160);
            if (squery->successor != NULL) {
                memcpy(ret + 328, squery->successor, 160);
            }
            break;
        }
        default:
            cerr << "Cannot identify message type: " << this->getType(msg) << endl;
    }
    
    return ret;
}

void *MessageHandler::unserialize(unsigned char *byteStream) {
    switch (this->getType(byteStream)) {
        case MTYPE_CHORD_QUERY:
        {
            ChordQuery *cquery = new ChordQuery();
            cquery->type = MTYPE_CHORD_QUERY;
            cquery->size = bctoi(byteStream + 4);
            cquery->fromId = new unsigned char[160];
            cquery->toId = new unsigned char[160];
            cquery->key = new unsigned char[160];
            
            memcpy(cquery->fromId, byteStream + 8, 160);
            memcpy(cquery->toId, byteStream + 168, 160);
            memcpy(cquery->key, byteStream + 328, 160);

            if (cquery->size == 648) {
                cquery->keyOwner = new unsigned char[160];
                memcpy(cquery->keyOwner, byteStream + 488, 160);
            } else {
                cquery->keyOwner = NULL;
            }
            
            return cquery;
        }
        case MTYPE_SUCCESSOR_QUERY:
        {
            SuccessorQuery *squery = new SuccessorQuery();
            squery->type = MTYPE_CHORD_QUERY;
            squery->size = bctoi(byteStream + 4);
            squery->fromId = new unsigned char[160];
            squery->toId = new unsigned char[160];
            
            memcpy(squery->fromId, byteStream + 8, 160);
            memcpy(squery->toId, byteStream + 168, 160);

            if (squery->size == 488) {
                squery->successor = new unsigned char[160];
                memcpy(squery->successor, byteStream + 328, 160);
            } else {
                squery->successor = NULL;
            }
            
            return squery;
            break;
        }
        default:
            dprt << "Cannot identify message type: " << this->getType(byteStream);
            return NULL;
    }
}

unsigned int MessageHandler::getSize(unsigned char *byteStream) {
    return bctoi(byteStream + 4);
}

unsigned int MessageHandler::getSize(void *msg) {
    return ((BaseMessage *) msg)->size;
}

unsigned int MessageHandler::getType(unsigned char *byteStream) {
    return bctoi(byteStream);
}

unsigned int MessageHandler::getType(void *msg) {
    return ((BaseMessage *) msg)->type;
}
