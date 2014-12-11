#include <iostream>
#include <fstream>
#include <string>

#include <sys/stat.h>

#include "../include/Chord.hpp"
#include "../include/ChordError.hpp"
#include "../include/Utils.hpp"

using namespace std;

Chord *crd;

void setup() {
    string s = string(getComputerName()) + "_store";
    //mkdir(s.c_str(), 0776);
    
    crd = new Chord(7783, 9293);
}


int main() {
    char *joinNode = getIpAddr("xinu02.cs.purdue.edu");
    cout << "Setting up chord" << endl;
    setup();
    
    cout << "Getting hash" << endl;
    if (crd->getHashedKey(NULL) == 0) {
        cerr << "[ERROR] Cannot get hashed key: " << crd->getError() << endl;
    }
    
    cout << "Initing chord" << endl;
    if (!crd->init()) {
        cerr << "Cannot start Chord service. Aborting..." << endl;
        cout << "ID is: " << crd->getHashedId() << endl;
        return -1;
    }
    
    cout << "Joining chord" << endl;
    if (!crd->join(joinNode)) {
        cerr << "Cannot join " << joinNode << endl;
        return -1;
    }
}
