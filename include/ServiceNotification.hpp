#ifndef __SERVICE_NOTIFICATION_HPP__
#define __SERVICE_NOTIFICATION_HPP__

#include <vector>

#include <pthread.h>

using namespace std;

/**
 * Basic notification object abstraction
 */
class Notification { /* empty, everything should be implemented by derived class */ };

/**
 * Basic notification service abstraction
 */
class ServiceNotification {
public:
    ServiceNotification() {
        pthread_mutex_init(&(this->nlistLock), NULL);
        pthread_mutex_init(&(this->notifierLock), NULL);
        this->serviceName = "";
        this->hasNotif = false;
    };
    
    /**
     * Use this to obtain next notification; removes the first item in the queue
     */
    Notification *popNotification() {
        Notification *ret = NULL;
        
        pthread_mutex_lock(&(this->nlistLock));
        if (!this->notifications.empty()) {
            vector<Notification *>::iterator it = this->notifications.begin();
            if (it != this->notifications.end()) {
                ret = *it;
                this->notifications.erase(it);
            }
        }
        
        if (this->notifications.empty()) {
            pthread_mutex_lock(&notifierLock);
            this->hasNotif = false;
            pthread_mutex_unlock(&notifierLock);
        }
        pthread_mutex_unlock(&(this->nlistLock));
        
        return ret;
    }
    
    const char *getServiceName() { return this->serviceName; }
    bool hasNotification() { return this->hasNotif; }
    
protected:
    void setServiceName(const char *name) { this->serviceName = name; }
    
    /**
     * Services use this to push notification into the queue
     * Subscriber can use this popNotification() method to get the notification
     */
    void pushNotification(Notification *n) {
        pthread_mutex_lock(&(this->nlistLock));
        this->notifications.push_back(n);
        pthread_mutex_unlock(&(this->nlistLock));
        
        pthread_mutex_lock(&(this->notifierLock));
        this->hasNotif = true;
        pthread_mutex_unlock(&(this->notifierLock));
    }

private:
    pthread_mutex_t nlistLock, notifierLock;
    
    const char *serviceName;
    vector<Notification *> notifications;
    bool hasNotif;
};

#endif
