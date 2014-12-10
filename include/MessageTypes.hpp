#ifndef __MESSAGE_TYPES_HPP__
#define __MESSAGE_TYPES_HPP__

#include <stdint.h>

const uint32_t MTYPE_CHORD_QUERY = 1;
const uint32_t MTYPE_SUCCESSOR_QUERY = 2;

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
    unsigned char *fromId;
    unsigned char *toId;
    
    unsigned char *key;
    unsigned char *keyOwner;
} ChordQuery;


/**
 * Type = MTYPE_SUCCESSOR_QUERY
 * 
 * successor = NULL if a query; otherwise response
 */
typedef struct {
    uint32_t type;
    uint32_t size;
    unsigned char *fromId;
    unsigned char *toId;
    
    unsigned char *successor;
} SuccessorQuery;
#endif
