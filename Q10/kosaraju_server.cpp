#include "kosaraju_server.hpp"
#include "proactor.hpp"
#include <iostream>
#include <vector>
#include <stack>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <map>


#define PORT "9034"

using namespace std;

vector<vector<int>> adjMat;
mutex adjMatMutex;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutexCondition = PTHREAD_MUTEX_INITIALIZER;
bool mostGraphConnected = false;

bool wasHalfInSCC = false;
bool notHalfInSCC = false;

void dfsUtil(int node, vector<bool>& visited, stack<int>& st) {
    visited[node] = true;  // Mark the current node as visited
    int n = adjMat.size();  // Get the number of nodes
    for (int neighbor = 0; neighbor < n; ++neighbor) {
        if (adjMat[node][neighbor] && !visited[neighbor]) {
            dfsUtil(neighbor, visited, st);  // Recursively visit neighbors
        }
    }
    st.push(node);  // Push the node to the stack after visiting all its neighbors
}

void dfsReverseUtil(int node, vector<bool>& visited, vector<int>& component) {
    visited[node] = true;  // Mark the current node as visited
    component.push_back(node);  // Add the node to the current component
    int n = adjMat.size();  // Get the number of nodes
    for (int neighbor = 0; neighbor < n; ++neighbor) {
        if (adjMat[neighbor][node] && !visited[neighbor]) {
            dfsReverseUtil(neighbor, visited, component);  // Recursively visit neighbors in the reverse graph
        }
    }
}
vector<vector<int>> receiveGraph(int n, int m, int client_fd) {
    vector<vector<int>> adj(n, vector<int>(n, 0));  // Initialize adjacency matrix
    for (int i = 0; i < m; ++i) {
        char buf[256];
        int bytes_received = recv(client_fd, buf, sizeof(buf), 0);  // Receive edge from client
        if (bytes_received <= 0) {
            cerr << "Error receiving the graph!" << endl;
            break;
        }
        buf[bytes_received] = '\0';  // Null-terminate the string
        int u, v;
        sscanf(buf, "%d %d", &u, &v);  // Parse edge
        adj[u-1][v-1] = 1;  // Adjust to 0-based indexing
    }
    send(client_fd, "Graph received\n", 21, 0);  // Send confirmation to client
    return adj;  // Return the adjacency matrix
}

// Function to perform Kosaraju's algorithm to find strongly connected components (SCCs)
vector<vector<int>> findSCCs(int n) {
    vector<bool> visited(n, false);  // Vector to keep track of visited nodes
    stack<int> st;  // Stack to store nodes in finishing order of DFS

    // Perform DFS and fill the stack with nodes in finishing order
    for (int i = 0; i < n; ++i) {
        if (!visited[i]) {
            dfsUtil(i, visited, st);  // Call DFS for unvisited nodes
        }
    }

    // Create a transposed (reverse) adjacency matrix
    vector<vector<int>> reverseAdjMat(n, vector<int>(n, 0));
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            reverseAdjMat[i][j] = adjMat[j][i];  // Reverse the edges of the graph
        }
    }

    fill(visited.begin(), visited.end(), false);  // Reset the visited vector for the second DFS
    vector<vector<int>> SCC;  // Vector to store all strongly connected components

    // Process all nodes in the order defined by the stack
    while (!st.empty()) {
        int node = st.top();
        st.pop();
        if (!visited[node]) {
            vector<int> component;  // Vector to store the current component
            dfsReverseUtil(node, visited, component);  // Perform DFS on the transposed graph
            SCC.push_back(component);  // Add the component to the list of SCCs
        }
    }

    return SCC;  // Return the list of strongly connected components
}
// Function to print strongly connected components (SCCs) and update the status of the graph
void printSCCs(const vector<vector<int>>& scc, int client_fd) {
    bool foundMajority = false;  // Flag to indicate if any SCC contains more than half of the nodes

    // Lock the mutex to ensure thread safety
    pthread_mutex_lock(&mutexCondition);

    // Iterate through each SCC to check if it contains more than half of the nodes
    for (int i = 0; i < scc.size(); ++i) {
        if (scc[i].size() > adjMat.size() / 2) {
            mostGraphConnected = true;  // Set the flag indicating most of the graph is connected in one SCC
            wasHalfInSCC = true;  // Set the flag indicating we found an SCC with more than half nodes
            foundMajority = true;  // Mark that we found such an SCC
            pthread_cond_signal(&cond);  // Signal the condition variable to wake up any waiting threads
            break;  // Exit the loop as we found the desired SCC
        }
    }

    // Print the status of the foundMajority and wasHalfInSCC flags
    std::cout << "foundMajority: " << foundMajority << std::endl;
    std::cout << "wasMajority: " << wasHalfInSCC << std::endl;

    // If no majority SCC was found but previously it was, update the notHalfInSCC flag
    if (foundMajority == false && wasHalfInSCC == true) {
        notHalfInSCC = true;  // Set the flag indicating that the graph is no longer half in one SCC
        pthread_cond_signal(&cond);  // Signal the condition variable to wake up any waiting threads
    }

    // Unlock the mutex after the updates
    pthread_mutex_unlock(&mutexCondition);

    // Iterate through each SCC to send its components to the client
    for (const auto& component : scc) {
        string result;

        // Build a string of node numbers for the current SCC
        for (int node : component) {
            result += to_string(node + 1) + " ";  // Convert node index to 1-based and append to result string
        }
        result += "\n";  // Add a newline character at the end of the SCC

        // Send the SCC string to the client
        send(client_fd, result.c_str(), result.size(), 0);
    }
}

// Function to handle client commands
void handleClient(const string& command, int client_fd) {
    // Print the received command to the console
    cout << "Received command: " << command << endl;

    try {
        // Check if the command starts with "Newgraph"
        if (command.substr(0, 8) == "Newgraph") {
            cout << "Processing Newgraph command" << endl;
            size_t pos = 9;
            size_t next_space = command.find(" ", pos);
            int n = stoi(command.substr(pos, next_space - pos));
            pos = next_space + 1;
            int m = stoi(command.substr(pos));
            // Lock the adjacency matrix while updating it
            lock_guard<mutex> lock(adjMatMutex);
            adjMat = receiveGraph(n, m, client_fd);

        // Check if the command starts with "Kosaraju"
        } else if (command.substr(0, 8) == "Kosaraju") {
            cout << "Processing Kosaraju command" << endl;
            // Lock the adjacency matrix while reading it
            lock_guard<mutex> lock(adjMatMutex);
            vector<vector<int>> scc = findSCCs(adjMat.size());
            printSCCs(scc, client_fd);

        // Check if the command starts with "Newedge"
        } else if (command.substr(0, 7) == "Newedge") {
            cout << "Processing Newedge command" << endl;
            size_t pos = 8;
            size_t next_space = command.find(" ", pos);
            int u = stoi(command.substr(pos, next_space - pos));
            pos = next_space + 1;
            int v = stoi(command.substr(pos));
            // Lock the adjacency matrix while updating it
            {
                lock_guard<mutex> lock(adjMatMutex);
                adjMat[u-1][v-1] = 1;
            }
            send(client_fd, "Edge added\n", 11, 0);

        // Check if the command starts with "Removeedge"
        } else if (command.substr(0, 10) == "Removeedge") {
            cout << "Processing Removeedge command" << endl;
            size_t pos = 11;
            size_t next_space = command.find(" ", pos);
            int u = stoi(command.substr(pos, next_space - pos));
            pos = next_space + 1;
            int v = stoi(command.substr(pos));
            // Lock the adjacency matrix while updating it
            {
                lock_guard<mutex> lock(adjMatMutex);
                adjMat[u-1][v-1] = 0;
            }
            send(client_fd, "Edge removed\n", 13, 0);

        // Handle invalid commands
        } else {
            cout << "Invalid command: " << command << endl;
            send(client_fd, "Invalid command\n", 16, 0);
        }
    } catch (const exception& e) {
        // Handle exceptions and send error message to the client
        cerr << "Error handling command: " << e.what() << endl;
        send(client_fd, "Error processing command\n", 25, 0);
    }
}

// Function to create a listening socket
int createLisrSocket() {
    int listener;   // Listening socket descriptor
    int yes = 1;    // Flag for setsockopt() SO_REUSEADDR, to reuse the socket
    int rv;

    struct addrinfo hints, *ai, *p;

    // Initialize the hints structure to zero
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;     // Use IPv4 or IPv6, whichever
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
    hints.ai_flags = AI_PASSIVE;     // Fill in my IP for me

    // Get a list of potential server addresses
    if ((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0) {
        fprintf(stderr, "selectserver: %s\n", gai_strerror(rv)); // Print error message if getaddrinfo fails
        exit(1); // Exit the program if we can't get server addresses
    }

    // Loop through all the results and bind to the first we can
    for (p = ai; p != NULL; p = p->ai_next) {
        // Create a socket
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0) {
            continue; // If socket creation fails, try the next address
        }

        // Set socket options to reuse the address
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        // Bind the socket to the address
        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener); // If binding fails, close the socket and try the next address
            continue;
        }

        break; // Successfully bound, exit the loop
    }

    // Free the address info structure
    freeaddrinfo(ai);

    // If p is NULL, it means we couldn't bind to any address
    if (p == NULL) {
        return -1; // Return an error code
    }

    // Listen for incoming connections
    if (listen(listener, 10) == -1) {
        return -1; // Return an error code if listen fails
    }

    // Return the listening socket descriptor
    return listener;
}

void* proactorThread(int client_fd) {
    char buf[256];  // Buffer to store data received from the client
    while (true) {
        // Receive data from the client
        int nbytes = recv(client_fd, buf, sizeof(buf), 0);
        if (nbytes <= 0) {  // Check if the receive function encountered an error or the client disconnected
            if (nbytes == 0) {
                // If nbytes is 0, the client has closed the connection
                cout << "Client " << client_fd << " disconnected" << endl;
            } else {
                // If nbytes is less than 0, an error occurred
                perror("recv");
            }
            close(client_fd);  // Close the socket connection with the client
            break;  // Exit the loop and end the thread
        } else {
            // If data is received successfully, process the command
            string command(buf, nbytes);  // Create a string from the received data
            handleClient(command, client_fd);  // Handle the client command
        }
    }
    return nullptr;  // Return nullptr when the thread finishes execution
}

void* monitorSCC(void* arg) {
    while (true) {
        pthread_mutex_lock(&mutexCondition);  // Lock the mutex to safely access shared variables
        
        // Wait for the condition variable to be signaled and check the conditions
        while (!mostGraphConnected && !notHalfInSCC) {
            pthread_cond_wait(&cond, &mutexCondition);  // Wait for the condition variable to be signaled
        }

        // Store the current states of the conditions locally
        bool localMostGraphConnected = mostGraphConnected;
        bool localNotLongerInSCC = notHalfInSCC;

        // Update the global conditions based on the current state
        if (mostGraphConnected) {
            mostGraphConnected = false;  // Reset the global condition
            wasHalfInSCC = true;  // Update the state to indicate more than half is in one SCC
        }

        if (notHalfInSCC) {
            notHalfInSCC = false;  // Reset the global condition
            wasHalfInSCC = false;  // Update the state to indicate less than half is in one SCC
        }

        pthread_mutex_unlock(&mutexCondition);  // Unlock the mutex after updating shared variables
        
        // Print the appropriate message based on the local state
        if (localMostGraphConnected) {
            cout << "At least 50% of the graph belongs to the same SCC\n";
        }

        if (localNotLongerInSCC) {
            cout << "At least 50% of the graph NO LONGER belongs to the same SCC\n";
        }
    }
    
    return nullptr;  // Return nullptr when the thread finishes execution
}

int main() {
    int listener = createLisrSocket();  // Create listener socket
    if (listener == -1) {
        cerr << "Error: Unable to create listener socket" << endl;
        return 1;
    }

    fd_set master;
    fd_set read_fds;
    int fdmax;

    std::map<int, pthread_t> clients;  // Map to store client threads

    FD_ZERO(&master);  // Clear the master set
    FD_ZERO(&read_fds);  // Clear the read set
    FD_SET(listener, &master);  // Add listener to master set
    fdmax = listener;  // Initialize the maximum file descriptor

    pthread_t t;
    if (pthread_create(&t, nullptr, monitorSCC, nullptr) != 0) {  // Create monitoring thread
        cerr << "Error: Failed to create monitoring thread" << endl;
        return 1;
    }

    cout << "Waiting for connections..." << endl;

    while (true) {
        read_fds = master;  // Copy master set to read set
        if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1) {
            cerr << "Error: Select function failed" << endl;
            return 1;
        }

        for (int i = 0; i <= fdmax; ++i) {
            if (FD_ISSET(i, &read_fds)) {
                if (i == listener) {
                    struct sockaddr_storage remoteaddr;
                    socklen_t addrlen = sizeof remoteaddr;
                    int newfd = accept(listener, (struct sockaddr *)&remoteaddr, &addrlen);
                    if (newfd == -1) {
                        cerr << "Error: Accept function failed" << endl;
                    } else {
                        FD_SET(newfd, &master);  // Add new client to master set
                        if (newfd > fdmax) {
                            fdmax = newfd;  // Update max file descriptor
                        }
                        cout << "New connection from " << inet_ntoa(((struct sockaddr_in *)&remoteaddr)->sin_addr) << endl;

                        pthread_t tid = startProactor(newfd, proactorThread);  // Start proactor thread
                        if (tid == 0) {
                            cerr << "Error: Failed to start proactor thread" << endl;
                        } else {
                            clients[newfd] = tid;  // Store client thread
                        }
                    }
                } else {
                    char buf[256];
                    int bytes_received = recv(i, buf, sizeof buf, 0);  // Receive data from client
                    if (bytes_received <= 0) {
                        if (bytes_received == 0) {
                            cout << "Notice: Socket " << i << " hung up" << endl;
                        } else {
                            cerr << "Error: Receive error on socket " << i << endl;
                        }
                        close(i);  // Close client connection
                        FD_CLR(i, &master);  // Remove from master set

                        auto it = clients.find(i);
                        if (it != clients.end()) {
                            stopProactor(it->second);  // Stop proactor thread
                            clients.erase(it);  // Remove from client map
                        }
                    } else {
                        buf[bytes_received] = '\0';  // Null-terminate string
                        string command(buf);  // Parse command
                        handleClient(command, i);  // Handle command
                    }
                }
            }
        }
    }

    for (const auto& client : clients) {
        stopProactor(client.second);  // Stop all client threads
    }

    pthread_join(t, NULL);  // Join monitoring thread

    return 0;
}
