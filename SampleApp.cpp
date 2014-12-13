#include <cerrno>

#include <iostream>
#include <fstream>
#include <string>

#include <pthread.h>

#include "include/Chord.hpp"
#include "include/Utils.hpp"

using namespace std;

Chord *crd;

// Used to lock user input while synchronizing
pthread_mutex_t userInputLock;

/**
 * Separate thread for non-blocking notifications
 * 
 * @param   arg     Pointer to a boolean for interrupts (stopping thread)
 * @return  Nothing
 */
void *notificationListener(void *arg) {
    bool *interrupt = (bool *) arg;
    while (!*interrupt) {
        if (crd->hasNotification()) {
            ChordNotification *cnotif = crd->popNotification();
            switch (cnotif->getType()) {
                case ChordNotification::NTYPE_SYNC_NOTIFICATION:
                {   
                    cout << endl;
                    cout << "[NOTICE] New host joined - "
                         << getHostname(cstr(cnotif->getIp()))
                         << ":" << cnotif->getPort()
                         << endl;
                    cout << "         Acquiring/waiting for user input lock for syncing..." << endl;
                    pthread_mutex_lock(&userInputLock);
                    cout << "         User input now locked." << endl;
                    cout << "         Synchronizing/Moving local files..." << endl;
                    pthread_mutex_unlock(&userInputLock);
                    cout << "         Sync completed. User input now unlocked." << endl;
                    break;
                }
                default:
                    cerr << endl;
                    cerr << "[NOTICE] New Chord notification; cannot understand type: " << cnotif->getType() << endl;
            }
            
            // Just so it looks good lol...
            cout << "cmd> " << flush;
        }
        
        // Avoid throttling
        usleep(100000);
    }
    
    return NULL;
}

/**
 * Command line loop, waiting for user input
 */
void commandLoop() {
    while (true) {
        pthread_mutex_unlock(&userInputLock);
        string cmd;
        cout << "cmd> " << flush;
        getline(cin, cmd);
        pthread_mutex_lock(&userInputLock);
        
        vector<string> tokens = split(cmd);

        // Skip if there is nothing to tokenize (empty string or space-filled strings)
        if (tokens.size() == 0) {
            continue;
        }

        // Disables prompt loop
        string command = tokens[0];

        if (command.compare("exit") == 0) {
            crd->stop();
            pthread_mutex_unlock(&userInputLock);
            break;
        } else if (command.compare("map") == 0) {
            cout << ">> Getting chord map..." << endl;
            
            char *mapstr = crd->getChordMap();
            if (mapstr == NULL) {
                cerr << "[ERROR] Cannot get map, reason: " << crd->getError() << endl;
            } else {
                cout << "Map: " <<  mapstr << endl;
            }
        } else if (command.compare("put") == 0) {
            if (tokens.size() == 2) {
                char *hostname = NULL;
                unsigned int port = 0;
                
                cout << ">> Querying location of key: " << tokens[1] << endl;
                crd->query(cstr(tokens[1]), &hostname, port);
                
                if (hostname == NULL || port == 0) {
                    cerr << "[ERROR] Cannot find key: " << tokens[1] << ", reason: " << crd->getError() << endl;
                } else {
                    cout << "Uploading to " << hostname << ":" << port << endl;
                }
            } else {
                cerr << "[ERROR] Invalid argument for command put. Usage: put [filename]" << endl;
            }
        } else if (command.compare("hash") == 0) {
            if (tokens.size() == 2) {
                cout << "Hashed: " << crd->getHashedKey(cstr(tokens[1])) << endl;
            } else {
                cerr << "[ERROR] Invalid argument for command hash. Usage: hash [key]" << endl;
            }
        } else {
            cerr << "[ERROR] Unrecognized command: " << cmd << endl;
        }
    } 
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
                cerr << "[ERROR] Invalid argument." << endl;
                return -1;
        }
    }
    
    // Check if parametres are set
    if (chordPort == 0 || appPort == 0) {
        cerr << "[ERROR] Insufficient argument: chord and app port are both needed." << endl;
        cout << "Usage: ./" << argv[0] << " -c CHORD_PORT -p APP_PORT [-j JOIN_IPADDR]" << endl;
        return -1;
    }
    
    cout << ">> Setting up chord" << endl;
    // New instance of Chord service
    crd = new Chord(appPort, chordPort);
    // Set join point; if pass in NULL (or not set), the service will be a new standalone network
    crd->setJoinPointIp(joinNode);
    
    cout << ">> Initing chord" << endl;
    // Attempts to initialize chord
    if (!crd->init()) {
        cerr << "[ERROR] Cannot initialize Chord service: " << crd->getError() << endl;
        cout << "        Aborting..." << endl;
        return -1;
    }
    
    cout << ">> Subscribing to ChordNotification..." << endl;
    bool interrupt = false;
    pthread_t notificationThread;
    pthread_create(&notificationThread, NULL, notificationListener, &interrupt);
    
    cout << ">> Starting chord service..." << endl;
    if (!crd->start()) {
        cerr << "[ERROR] Cannot start Chord service: " << crd->getError() << endl;
        cout << "        Aborting..." << endl;
        return -1;
    }
    
    cout << ">> Starting command line..." << endl;
    pthread_mutex_init(&userInputLock, NULL);
    commandLoop();
    
    cout << ">> Sending interrupt to unsubscribe from ChordNotification..." << endl;
    interrupt = true;
    cout << ">> Waiting for thread to finish..." << endl;
    int ret = pthread_join(notificationThread, NULL);
    
    if (ret == -1) {
        cerr << "[ERROR] Problem while ending thread: " << strerror(errno) << endl;
    }
    
    cout << ">> Terminated." << endl;
}
