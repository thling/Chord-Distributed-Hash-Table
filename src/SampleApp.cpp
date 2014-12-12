#include <iostream>
#include <fstream>
#include <string>

#include <sys/stat.h>

#include "../include/Chord.hpp"
#include "../include/ChordError.hpp"
#include "../include/Utils.hpp"

using namespace std;

Chord *crd;

void setup(unsigned int appPort, unsigned int chordPort, char *joinNode = NULL) {
    string s = string(getComputerName()) + "_store";
    //mkdir(s.c_str(), 0776);
    crd = new Chord(appPort, chordPort);
    
    // Sets which node to join
    crd->setJoinPointIp(joinNode);
}


int main(int argc, char **argv) {
    unsigned int chordPort = 0, appPort = 0;
    char *joinNode = NULL;
    
    int optflag;
    
    // Get command line arguments
    while ((optflag = getopt(argc, argv, "p:c:j:")) != -1) {
        switch (optflag) {
            case 'p':
                appPort = atoi(optarg);
                if (!isValidPort(appPort)) {
                    cerr << "[ERROR] Invalid port number: " << appPort << endl;
                    return -1;
                }
                
                dprt << "  App Port: " << appPort;
                break;
            case 'c':
                chordPort = atoi(optarg);
                if (!isValidPort(chordPort)) {
                    cerr << "[ERROR] Invalid port number: " << chordPort << endl;
                    return -1;
                }
                
                dprt << "Chord Port: " << chordPort;
                break;
            case 'j':
                joinNode = optarg;
                dprt << "   Join IP: " << joinNode;
                break;
            default:
                cerr << "Invalid argument." << endl;
                return -1;
        }
    }
    
    if (chordPort == 0 || appPort == 0) {
        cerr << "Insufficient argument: chord and app port are both needed." << endl;
        cout << "Usage: ./" << argv[0] << " -c CHORD_PORT -p APP_PORT [-j JOIN_IPADDR]" << endl;
        return -1;
    }
    
    cout << "Setting up chord" << endl;
    setup(appPort, chordPort, joinNode);
    
    cout << "Initing chord" << endl;
    if (!crd->init()) {
        cerr << "Cannot init Chord service. Aborting..." << endl;
        return -1;
    }

    if (!crd->start()) {
        cerr << "Cannot start Chord service. Aborting..." << endl;
        cerr << crd->getError() << endl;
    }
    
    while (true) {
        usleep(100000);
    }
}
