#ifndef __CHORD_HPP__
#define __CHORD_HPP__
#define CHORD_LENGTH_BIT 3

typedef struct {
    char *ipaddr;
    char *hashedId;
} node;

class Chord {
public:
    Chord();
    
    bool start();
    char *query(char *key);
    char *getChordMap();
    
    char *getHashedId();
    char *getHashedKey(char *key);
    
    char *sha1Hash(unsigned char *, size_t len);
    
private:
    unsigned int chordBit;
    char *hashedId;
    
    char *ipaddr;
    node *successor;
    
    node *getSuccessor();
    
    bool join();
};

#endif
