#ifndef __MESSAGE_HANDLER_HPP__
#define __MESSAGE_HANDLER_HPP__

#include "MessageTypes.hpp"

class MessageHandler {
public:
    static unsigned char *serialize(void *msg);
    static void *unserialize(unsigned char *byteStream);
    static unsigned int getType(unsigned char *byteStream);
    static unsigned int getType(void *msg);
    static unsigned int getSize(unsigned char *byteStream);
    static unsigned int getSize(void *msg);
    
    static SuccessorQuery *createSQ(uint32_t searchTerm, uint32_t appPort, char *sender);
    static SuccessorQueryResponse *createSQR(uint32_t searchTerm, uint32_t appPort, char *responder);
};

#endif
