#ifndef KOSARAJU_SERVER_HPP
#define KOSARAJU_SERVER_HPP

#include <vector>
#include <stack>
#include <mutex>
#include <condition_variable>
#include <map>

struct ClientInfo {
    int client_fd;
    pthread_t thread_id;
};

// Function to perform depth-first search
void dfsUtil(int node, std::vector<bool>& visited, std::stack<int>& st);

// Function to perform depth-first search on the reverse graph
void dfsReverseUtil(int node, std::vector<bool>& visited, std::vector<int>& component);

// Function to perform Kosaraju's algorithm to find strongly connected components
std::vector<std::vector<int>> findSCCs(int n);

// Function to receive the graph input from the client
std::vector<std::vector<int>> receiveGraph(int n, int m, int client_fd);

// Function to print strongly connected components
void printSCCs(const std::vector<std::vector<int>>& scc, int client_fd);

// Function to handle client commands
void handleClient(const std::string& command, int client_fd);

// Function to return a listening socket
int createLisrSocket();


// Function to handle client communication in the proactor pattern
void* proactorThread(int client_fd);

// Function to monitor the condition of the graph's connectivity
void* monitorSCC(void* arg);


#endif // KOSARAJU_SERVER_HPP
