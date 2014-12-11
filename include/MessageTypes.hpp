#ifndef __MESSAGE_TYPES_HPP__
#define __MESSAGE_TYPES_HPP__

#include <stdint.h>

const uint32_t MTYPE_KEY_QUERY = 1;
const uint32_t MTYPE_KEY_QUERY_RESPONSE = 2;
const uint32_t MTYPE_SUCCESSOR_QUERY = 3;
const uint32_t MTYPE_SUCCESSOR_QUERY_RESPONSE = 4;
const uint32_t MTYPE_JOIN_REQUEST = 5;

/**
 * Base message type (wrapper)
 */
typedef struct {
    uint32_t type;
    uint32_t size;
} BaseMessage;

/**
 * Type = MTYPE_CHORD_QUEYR
 * 
 * keyOwner = NULL if query; otherwise response
 * If keyOwner == NULL and fromId == Self, then cannot find key in the system
 */
typedef struct {
    uint32_t type;
    uint32_t size;
    
    uint32_t key;
    char *sender;   // IP addr of sender
} KeyQuery;

typedef struct {
    uint32_t type;
    uint32_t size;
    
    uint32_t port;  // Port of the node's application handling the data
    char *owner;   // IP addr of the node containing the thing data
} KeyQueryResponse;

/**
 * Type = MTYPE_SUCCESSOR_QUERY
 * 
 * successor = NULL if a query; otherwise response
 */
typedef struct {
    uint32_t type;
    uint32_t size;
    
    char *sender;   // IP addr of the sender
} SuccessorQuery;

typedef struct {
    uint32_t type;
    uint32_t size;
    
    char *responder;    // IP addr of the responder
} SuccessorQueryResponse;

typedef struct {
    uint32_t type;
    uint32_t size;
    
    char *ipaddr;   // IP addr of the node requesting to join
} JoinRequest;
#endif
