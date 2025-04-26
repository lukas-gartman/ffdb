#ifndef SYNCH_H
#define SYNCH_H

#include <pthread.h>
#include <stdexcept>
#include <iostream>

/* Wrapper class for pthread mutex for use in memory mapped files. Let addr
 * point to an address in a memory mapped file, we can access the mutex like
 * this:
 *
 *     Mutex *mutex = (Mutex *) addr;
 *
 * Note that when working with a new memory mapped file, the mutex must be in a
 * zero initialized region. This way initialized will naturally be set to false.
 */
class Mutex {
private:
    pthread_mutex_t mutex;
    bool initialized;

public:
    void init() {
        if (initialized) throw std::runtime_error("Mutex already initialized");

        pthread_mutexattr_t attr;

        if (pthread_mutexattr_init(&attr) != 0)
            throw std::runtime_error("Failed to initialize mutex attributes");

        if (pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED) != 0) {
            pthread_mutexattr_destroy(&attr);
            throw std::runtime_error("Failed to set pshared");
        }

        if (pthread_mutex_init(&mutex, &attr) != 0) {
            pthread_mutexattr_destroy(&attr);
            throw std::runtime_error("Failed to initialize mutex");
        }

        pthread_mutexattr_destroy(&attr);
        initialized = true;
    }

    void lock() {
        if (!initialized) throw std::runtime_error("Mutex not initialized");

        pthread_mutex_lock(&mutex);
    }

    void unlock() {
        if (!initialized) throw std::runtime_error("Mutex not initialized");

        pthread_mutex_unlock(&mutex);
    }
};

class RWLock {
private:
    pthread_rwlock_t rwlock;
    bool initialized;

public:
    void init() {
        if (initialized) throw std::runtime_error("RWLock already initialized");

        pthread_rwlockattr_t attr;

        if (pthread_rwlockattr_init(&attr) != 0)
            throw std::runtime_error("Failed to initialize mutex attributes");

        if (pthread_rwlockattr_setpshared(&attr, PTHREAD_PROCESS_SHARED) != 0) {
            pthread_rwlockattr_destroy(&attr);
            throw std::runtime_error("Failed to set pshared");
        }

        if (pthread_rwlock_init(&rwlock, &attr) != 0) {
            pthread_rwlockattr_destroy(&attr);
            throw std::runtime_error("Failed to initialize rwlock");
        }

        pthread_rwlockattr_destroy(&attr);
        initialized = true;
    }

    void read_lock() {
        if (!initialized) throw std::runtime_error("RWLock not initialized");

        pthread_rwlock_rdlock(&rwlock);
    }

    void write_lock() {
        if (!initialized) throw std::runtime_error("RWLock not initialized");

        pthread_rwlock_wrlock(&rwlock);
    }

    void unlock() {
        if (!initialized) throw std::runtime_error("RWLock not initialized");

        pthread_rwlock_unlock(&rwlock);
    }
};

#endif //SYNCH_H
