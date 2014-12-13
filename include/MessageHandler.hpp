#ifndef __MESSAGE_HANDLER_HPP__
#define __MESSAGE_HANDLER_HPP__

#include "MessageTypes.hpp"

/**
 * Processes and handles the messages specified in MessageTypes.hpp
 * 
 * This class groups message handling related methods together
 */
class MessageHandler {
public:
    static unsigned char *serialize(void *msg);
    static void *unserialize(unsigned char *byteStream);
    
    static unsigned int getType(unsigned char *byteStream);
    static unsigned int getType(void *msg);
    
    static unsigned int getSize(unsigned char *byteStream);
    static unsigned int getSize(void *msg);

    static UpdatePredcessor *createUpdatePredecessor(uint32_t appPort, char *predecessor);
    static UpdatePredcessorAck *createUpdatePredecessorAck(uint32_t hashedId);
    
    static StabilizeRequest *createStabilizeRequest(uint32_t appPort, char *sender);
    static StabilizeResponse *createStabilizeResponse(uint32_t appPort, char *predecessor);
    
    static SuccessorQuery *createSuccessorQuery(uint32_t searchTerm, uint32_t appPort, char *sender);
    static SuccessorResponse *createSuccessorResponse(uint32_t searchTerm, uint32_t appPort, char *responder);
    
    static ChordMapQuery *createChordMapQuery(uint32_t seq, char *sender);
    static ChordMapResponse *createChordMapResponse(uint32_t seq, char *responder);
};

#endif
