#include "kosaraju_server.hpp"
#include "proactor.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <algorithm>
#include <chrono>
#include <thread>

using namespace std;

// Global variables to store the graph and protect it with a mutex
vector<list<int>> adj; // Adjacency list for the graph
vector<list<int>> transposedAdj; // Transposed adjacency list for the graph
int n, m; // Number of vertices and edges in the graph
mutex graph_mutex; // Mutex to protect the graph data structure
condition_variable scc_cond; // Condition variable for SCC changes
bool scc_condition_met = false; // Flag indicating if SCC condition is met
bool notify = false; // Flag to indicate when to notify the condition variable

// Function to perform DFS and fill the stack
void fillOrder(int v, vector<bool>& visited, stack<int>& Stack) {
    visited[v] = true;
    for (int neighbor : adj[v]) {
        if (!visited[neighbor]) {
            fillOrder(neighbor, visited, Stack);
        }
    }
    Stack.push(v);
}

// Function to perform DFS on the transposed graph
void DFSUtil(int v, vector<bool>& visited, vector<int>& component) {
    visited[v] = true;
    component.push_back(v);
    for (int neighbor : transposedAdj[v]) {
        if (!visited[neighbor]) {
            DFSUtil(neighbor, visited, component);
        }
    }
}

// Function to get the transposed graph
void getTranspose() {
    transposedAdj = vector<list<int>>(n);
    for (int v = 0; v < n; ++v) {
        for (int neighbor : adj[v]) {
            transposedAdj[neighbor].push_back(v);
        }
    }
}

// Function to find and return all strongly connected components (SCCs)
string findSCCs() {
    stack<int> Stack;
    vector<bool> visited(n, false);

    for (int i = 0; i < n; ++i) {
        if (!visited[i]) {
            fillOrder(i, visited, Stack);
        }
    }

    getTranspose();

    fill(visited.begin(), visited.end(), false);

    stringstream ss;
    int scc_count = 0;
    int max_scc_size = 0; // Track the size of the largest SCC

    while (!Stack.empty()) {
        int v = Stack.top();
        Stack.pop();

        if (!visited[v]) {
            vector<int> component;
            DFSUtil(v, visited, component);
            ss << "SCC " << ++scc_count << " is: ";
            for (int vertex : component) {
                ss << (vertex + 1) << " ";
            }
            ss << endl;
            max_scc_size = max(max_scc_size, (int)component.size());
        }
    }

    // Debugging output
    cout << "SCCs Calculated: \n" << ss.str();

    // Check if at least 50% of the graph vertices are in one SCC
    bool condition_met = (max_scc_size >= (n + 1) / 2);  // Ensure rounding up for odd n
    {
        lock_guard<mutex> lock(graph_mutex);
        if (condition_met != scc_condition_met) {
            scc_condition_met = condition_met;
            notify = true;
            cout << "SCC condition changed: " << (condition_met ? "met" : "not met") << endl;
        }
    }
    scc_cond.notify_one();

    return ss.str();
}

// Function to handle the "Newgraph" command
void handleNewGraph(int vertices, int edges, int client_fd) {
    n = vertices;
    m = edges;
    adj = vector<list<int>>(n);
    char buf[256];
    int u, v;

    for (int i = 0; i < m; ++i) {
        memset(buf, 0, sizeof(buf));
        int nbytes = recv(client_fd, buf, sizeof(buf) - 1, 0);
        if (nbytes <= 0) {
            if (nbytes == 0) {
                cout << "Socket " << client_fd << " hung up" << endl;
            } else {
                perror("recv");
            }
            close(client_fd);
            return;
        }
        buf[nbytes] = '\0';
        stringstream ss(buf);
        ss >> u >> v;
        if (u < 1 || u > n || v < 1 || v > n) {
            cerr << "Invalid edge: " << u << " " << v << endl;
            --i;
            continue;
        }
        adj[u - 1].push_back(v - 1);
    }
    cout << "Graph with " << n << " vertices and " << m << " edges created." << endl;
}

// Function to handle the "Newedge" command
void handleNewEdge(int u, int v) {
    if (u < 1 || u > n || v < 1 || v > n) {
        cerr << "Invalid edge: " << u << " " << v << endl;
        return;
    }
    adj[u - 1].push_back(v - 1);
    cout << "Edge added: " << u << " -> " << v << endl;
}

// Function to handle the "Removeedge" command
void handleRemoveEdge(int u, int v) {
    if (u < 1 || u > n || v < 1 || v > n) {
        cerr << "Invalid edge: " << u << " " << v << endl;
        return;
    }
    adj[u - 1].remove(v - 1);
    cout << "Edge removed: " << u << " -> " << v << endl;
}

// Function to convert a string to lowercase
string toLowerCase(const string& str) {
    string result = str;
    transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

// Function to handle client commands
void handleClient(int client_fd) {
    char buf[1024];
    while (true) {
        int nbytes = recv(client_fd, buf, sizeof(buf) - 1, 0);
        if (nbytes <= 0) {
            if (nbytes == 0) {
                cout << "Socket " << client_fd << " hung up" << endl;
            } else {
                perror("recv");
            }
            close(client_fd);
            return;
        }
        buf[nbytes] = '\0';
        string command(buf);
        stringstream ss(command);
        string cmd;
        ss >> cmd;
        cmd = toLowerCase(cmd);
        string response;
        {
            lock_guard<mutex> lock(graph_mutex);
            if (cmd == "newgraph") {
                int vertices, edges;
                ss >> vertices >> edges;
                response = "Send the edges.\n";
                send(client_fd, response.c_str(), response.length(), 0);
                handleNewGraph(vertices, edges, client_fd);
                response = "New graph created.\n";
                send(client_fd, response.c_str(), response.length(), 0);
            } else if (cmd == "kosaraju") {
                response = findSCCs();
                send(client_fd, response.c_str(), response.length(), 0);
            } else if (cmd == "newedge") {
                int u, v;
                ss >> u >> v;
                handleNewEdge(u, v);
                response = "Edge added.\n";
                send(client_fd, response.c_str(), response.length(), 0);
            } else if (cmd == "removeedge") {
                int u, v;
                ss >> u >> v;
                handleRemoveEdge(u, v);
                response = "Edge removed.\n";
                send(client_fd, response.c_str(), response.length(), 0);
            } else {
                response = "Invalid command.\n";
                send(client_fd, response.c_str(), response.length(), 0);
            }
        }
    }
}

// Monitoring thread function
void monitoringThread() {
    while (true) {
        unique_lock<mutex> lock(graph_mutex);
        scc_cond.wait(lock, [] { return notify; });
        notify = false;
        if (scc_condition_met) {
            cout << "At least 50% of the graph belongs to the same SCC\n";
        } else {
            cout << "At least 50% of the graph no longer belongs to the same SCC\n";
        }
    }
}

int main() {
    int listener;
    struct sockaddr_in myaddr;
    int yes = 1;
    int port = 9034;

    if ((listener = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        perror("setsockopt");
        exit(1);
    }

    myaddr.sin_family = AF_INET;
    myaddr.sin_addr.s_addr = INADDR_ANY;
    myaddr.sin_port = htons(port);
    memset(&(myaddr.sin_zero), '\0', 8);

    if (bind(listener, (struct sockaddr *)&myaddr, sizeof(myaddr)) == -1) {
        perror("bind");
        exit(1);
    }

    if (listen(listener, 10) == -1) {
        perror("listen");
        exit(1);
    }

    cout << "Server running, press Ctrl+C to exit..." << endl;

    Proactor proactor;

    // Start the monitoring thread
    thread monitor(monitoringThread);

    while (true) {
        struct sockaddr_in remoteaddr;
        socklen_t addrlen = sizeof(remoteaddr);
        int newfd = accept(listener, (struct sockaddr*)&remoteaddr, &addrlen);
        if (newfd == -1) {
            perror("accept");
        } else {
            cout << "New connection from " << inet_ntoa(remoteaddr.sin_addr) << " on socket " << newfd << endl;
            proactor.startProactor(newfd, handleClient);
        }
    }

    monitor.join();
    close(listener);
    return 0;
}
