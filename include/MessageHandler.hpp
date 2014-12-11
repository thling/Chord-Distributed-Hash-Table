#ifndef __MESSAGE_HANDLER_HPP__
#define __MESSAGE_HANDLER_HPP__

#include "MessageTypes.hpp"

class MessageHandler {
public:
    unsigned char *serialize(void *msg);
    void *unserialize(unsigned char *byteStream);
    unsigned int getType(unsigned char *byteStream);
    unsigned int getType(void *msg);
    unsigned int getSize(unsigned char *byteStream);
    unsigned int getSize(void *msg);
};

#endif
