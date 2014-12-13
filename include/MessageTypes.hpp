#ifndef __MESSAGE_TYPES_HPP__
#define __MESSAGE_TYPES_HPP__

#include <stdint.h>

const uint32_t MTYPE_SUCCESSOR_QUERY = 1;
const uint32_t MTYPE_SUCCESSOR_RESPONSE = 2;
const uint32_t MTYPE_CHORD_MAP_QUERY = 3;
const uint32_t MTYPE_CHORD_MAP_RESPONSE = 4;
const uint32_t MTYPE_UPDATE_PREDECESSOR = 5;
const uint32_t MTYPE_UPDATE_PREDECESSOR_ACK = 6;
const uint32_t MTYPE_STABILIZE_REQUEST = 7;
const uint32_t MTYPE_STABILIZE_RESPONSE = 8;

/**
 * Base message type (wrapper)
 */
typedef struct {
    uint32_t type;
    uint32_t size;
} BaseMessage;

typedef struct {
    uint32_t type;
    uint32_t size;
    
    uint32_t appPort;
    char *predecessor;
} UpdatePredcessor;

typedef struct {
    uint32_t type;
    uint32_t size;
    uint32_t hashedId;
} UpdatePredcessorAck;

typedef struct {
    uint32_t type;
    uint32_t size;
    
    uint32_t appPort;
    char *sender;
} StabilizeRequest;

typedef struct {
    uint32_t type;
    uint32_t size;
    
    uint32_t appPort;
    char *predecessor;
} StabilizeResponse;

/**
 * Type = MTYPE_SUCCESSOR_QUERY
 * 
 * successor = NULL if a query; otherwise response
 */
typedef struct {
    uint32_t type;
    uint32_t size;
    uint32_t searchTerm;
    
    uint32_t appPort;
    char *sender;   // IP addr of the sender
} SuccessorQuery;

typedef struct {
    uint32_t type;
    uint32_t size;
    uint32_t searchTerm;

    uint32_t appPort;
    char *responder;    // IP addr of the responder
} SuccessorResponse;

typedef struct {
    uint32_t type;
    uint32_t size;
    uint32_t seq;
    
    char *sender;
} ChordMapQuery;

typedef struct {
    uint32_t type;
    uint32_t size;
    uint32_t seq;
    
    char *responder;
} ChordMapResponse;

#endif
