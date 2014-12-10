#include <iostream>
#include <fstream>
#include <string>

#include <sys/stat.h>

#include "../include/Chord.hpp"
#include "../include/Utils.hpp"

using namespace std;

Chord *crd;

void setup() {
    string s = string(getComputerName()) + "_store";
    //mkdir(s.c_str(), 0776);
    crd = new Chord();
}


int main() {
    setup();
    
    if (!crd->start()) {
        cerr << "Cannot start Chord service. Aborting..." << endl;
        cout << "ID is: " << crd->getHashedId() << endl;
        return -1;
    }
}
