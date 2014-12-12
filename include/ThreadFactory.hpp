#ifndef __THREAD_FACTORY_HPP__
#define __THREAD_FACTORY_HPP__

#include <pthread.h>

class ThreadFactory {
public:
    ThreadFactory() {
       /* empty */
    }

    virtual ~ThreadFactory() {
        /* empty */
    }

    /**
     * Returns true if the thread was successfully started, false if there was an error starting the thread
     */
    bool startThread() {
        return (pthread_create(&_thread, NULL, startThreadWorker, this) == 0);
    }
    
    /**
     * Returns true if the thread was successfully started, false if there was an error starting the thread
     */
    bool detachThread() {
        return (pthread_detach(_thread) == 0);
    }

    /** Will not return until the internal thread has exited. */
    void waitExit() {
        pthread_join(_thread, NULL);
    }

protected:
    /**
     * Implement this function for threaded activity
     */
    virtual void threadWorker() = 0;

private:
    static void * startThreadWorker(void * thisObj) {
        ((ThreadFactory *) thisObj)->threadWorker();
        return NULL;
    }

    pthread_t _thread;
};

#endif
