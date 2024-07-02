#ifndef PROACTOR_HPP
#define PROACTOR_HPP

#include <pthread.h>

// Type definition for the proactor function callback
typedef void* (*proactorFunc)(int);

// Wrapper function to start the proactor thread
void* proactorThreadWrapper(void* arg);

// Function to start a new proactor and return the proactor thread ID
pthread_t startProactor(int client_fd, proactorFunc func);

// Function to stop the proactor by thread ID
int stopProactor(pthread_t tid);

#endif // PROACTOR_HPP
