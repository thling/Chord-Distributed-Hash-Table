#ifndef __UTILS_HPP__
#define __UTILS_HPP__
#define dprt Log(__FILE__, __FUNCTION__, __LINE__)

#include <cstdarg>
#include <cstdlib>
#include <cstring>

#include <iomanip>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

#include <pthread.h>
#include <stdint.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <sys/types.h>

// Avoid interleaving print on command line
static pthread_mutex_t dprtMutex;

// For easy conversion from int to byte char
typedef union byteInt {
    uint32_t val;
    unsigned char cval[4];
} byteInt;


// Whether to print debug messages
static bool __DEBUG__ = true;

// For debugging purpose
class Log {
public:
    int level;
    Log(const std::string &file, const std::string &func, const unsigned int &line) {
        if (__DEBUG__) {
            pthread_mutex_init(&dprtMutex, NULL);
            
            pthread_mutex_lock(&dprtMutex);
            std::string fshort = func.substr(0, 6);
            std::cout << "[" << std::setfill(' ') << std::setw(6) << fshort
                      << ":" << std::setfill('0') << std::setw(4) << line << "] ";
        }
        
        return;
    }

    template <class T>
    Log &operator<<(const T &v) {
        if (__DEBUG__) {
            std::cout << v;
        }
        
        return *this;
    }

    ~Log() {
        if (__DEBUG__) {
            std::cout << std::endl;
            pthread_mutex_unlock(&dprtMutex);
        }
        
        return;
    }
};

/**
 * Returns time in microseconds. Note the larger parts of seconds have been discarded
 * 
 * @return  The time in relative micoseconds;
 *          relative as in we don't care about the larger seconds part
 */
static unsigned long int getTimeInUSeconds() {
    struct timeval t;  
    gettimeofday(&t, NULL);
    unsigned long int sec = t.tv_sec, usec = t.tv_usec;
    
    // Trim down larger part, they are too far away
    sec = sec - (sec / 1000) * 1000;
    
    return sec * 1000000 + usec;
}

/**
 * Return the file size in bytes
 * 
 * @param   filename    The name of the file to get size
 * @return  Size of the file, in bytes
 */
static bool fileExists(char *filename) {
    std::ifstream ifs;
    ifs.open(filename);
    bool result = ifs.good();
    ifs.close();
    
    return result;
}

/**
 * Return the file size in bytes
 * 
 * @param   filename    The name of the file to get size
 * @return  Size of the file, in bytes
 */
static unsigned long int getFilesize(char *filename) {
    std::streampos begin,end;
    std::ifstream ifs;
    ifs.open(filename, std::ios::binary);
    begin = ifs.tellg();
    ifs.seekg (0, std::ios::end);
    end = ifs.tellg();
    ifs.close();
    
    return end - begin;
}

/**
 * Converts a string into standard char * (not const char *)
 *
 * @param   src String to be converted
 * @return  Converted char *
 */
static char *cstr(std::string src) {
    char *tmp = new char[src.length() + 1];
    strcpy(tmp, src.c_str());
    
    return tmp;
}

/**
 * Check if the port number falls within valid range
 * 
 * @param   p   Port number to check
 * @return  True if p is valud and false if p is not
 */
static bool isValidPort(unsigned int p) {
    if (p < 1024 || p > 65535) {
        return false;
    }
    
    return true;
}

/**
 * Converts an unsigned int into a 4-byte char array.
 * 
 * @param i Unsigned integer to be converted
 * @return The converted 4-byte char array
 */
static unsigned char *itobc(uint32_t i) {
    unsigned char *ret = new unsigned char[4];
    byteInt a;
    a.val = i;
    
    memcpy(ret, a.cval, 4);
    return ret;
}

/**
 * Converts a 4-byte char array to unsigned int.
 *
 * @param c 4-byte char array to be converted
 * @return The converted unsigned int
 */
static uint32_t bctoi(unsigned char *c) {
    byteInt a;
    memcpy(a.cval, c, 4);
    
    return a.val;
}

/**
 * Split string into vector of tokens
 * 
 * @param   str     The string to be splitted
 * @param   delim   The delimiter of the tokens, default is <space>
 * @param   max     The maximum number of tokens to split
 * @return  A vector of tokenized string
 */
static std::vector<std::string> split(std::string str, char delim = ' ', int max = -1) {
    // Vector for tokens
    std::vector<std::string> ret;

    // Nothing to split
    if (str.length() == 0) {
        return ret;
    }
    
    // Trim and reduce
    size_t pos = str.find_first_not_of(delim);
    str = str.substr(pos);
    pos = str.find_last_not_of(delim);
    str = str.substr(0, pos + 1);
    
    dprt << "Trimmed string [" << str << "]";
    
    // Start finding
    pos = str.find_first_of(delim);
    while (pos != std::string::npos && max != 0) {
        std::string s = str.substr(0, pos);
        str = str.substr(pos + 1);
        ret.push_back(s);
        max--;
        pos = str.find_first_of(delim);
    }

    ret.push_back(str);

    return ret;
}

/**
 * Get name of local host
 *
 * @return  The name of the local host
 */
static char *getHostname(char *ipaddr = NULL) {
    char *hostname = new char[1024];
    
    if (ipaddr == NULL) {
        gethostname(hostname, 1024);
    } else {
        struct sockaddr_in sa;
        sa.sin_family = AF_INET;
        inet_pton(sa.sin_family, ipaddr, &sa.sin_addr);
        
        int res = getnameinfo((struct sockaddr *) &sa, sizeof(sa), hostname, 1024, NULL, 0, 0);
        if (res) {
            std::cerr << "Unable to get the host name of " << ipaddr << ": "
                      << gai_strerror(res)
                      << std::endl;
            return NULL;
        }
    }
    
    return hostname;
}

/**
 * Get computer name of the local host
 *
 * @return  The name of the local host
 */
static char *getComputerName(char *hostname = NULL) {
    if (hostname == NULL) {
        hostname = getHostname();
    }
    
    std::string hname(hostname);
    size_t pos = hname.find_first_of(".");
    hname = hname.substr(0, pos);
    
    return cstr(hname);
}

static char *getIpAddr(char *hostname = NULL) {
    if (hostname == NULL) {
        hostname = getHostname();
    }
    
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    getaddrinfo(hostname, "80", &hints, &res);
    char *result = inet_ntoa(((struct sockaddr_in *) res->ai_addr)->sin_addr);
    char *ret = new char[strlen(result) + 1];
    strcpy(ret, result);
    
    return ret;
}

#endif
