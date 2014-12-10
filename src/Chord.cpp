#include <cstdlib>
#include <iostream>
#include <sstream>

#include <openssl/sha.h>

#include "../include/Chord.hpp"
#include "../include/MessageHandler.hpp"
#include "../include/MessageTypes.hpp"
#include "../include/Utils.hpp"

using namespace std;

Chord::Chord() {
    // Get hash
    this->ipaddr = getIpAddr();
    this->hashedId = this->sha1Hash((unsigned char *)this->ipaddr, strlen(this->ipaddr));
}


bool Chord::start() {
    return false;
}

char *Chord::query(char *key) {
    if (key == NULL) {
    }
    
    return NULL;
}

char *Chord::getHashedId() {
    return this->hashedId;
}

char *Chord::getHashedKey(char *key){
    return NULL;
}

node *Chord::getSuccessor() {
    return this->successor;
}

bool join() {
    return false;
}

char *Chord::getChordMap() {
    
    return NULL;
}

char *Chord::sha1Hash(unsigned char *tohash, size_t len) {
    unsigned char *hash = new unsigned char[160];
    SHA1(tohash, len, hash);
    
    stringstream ss;
    for(int i = 0; i < 20; ++i) {
        ss << hex << setw(2) << setfill('0') << (int)hash[i];
    }
    
    return cstr(ss.str());
}
