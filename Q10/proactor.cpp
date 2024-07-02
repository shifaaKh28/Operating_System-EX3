#include "proactor.hpp"
#include <unistd.h>
#include <iostream>
#include <cstring>
#include <cstdlib>

// Struct to pass arguments to the proactor thread
struct ProactorArgs {
    int client_fd;
    proactorFunc func;
};

// Wrapper function to run the proactor thread
void* proactorThreadWrapper(void* arg) {
    ProactorArgs* args = static_cast<ProactorArgs*>(arg);
    args->func(args->client_fd);
    delete args;
    return nullptr;
}

// Starts a new proactor and returns the proactor thread ID
pthread_t startProactor(int client_fd, proactorFunc func) {
    pthread_t tid;
    ProactorArgs* args = new ProactorArgs{client_fd, func};
    if (pthread_create(&tid, nullptr, proactorThreadWrapper, args) != 0) {
        std::cerr << "Error creating proactor thread: " << std::strerror(errno) << std::endl;
        std::exit(EXIT_FAILURE);
    }
    return tid;
}

// Stops the proactor by thread ID
int stopProactor(pthread_t tid) {
    return pthread_cancel(tid);
}
