#include "common.h"

typedef struct {
    SOCKET socket;
    char username[MAX_USERNAME];
    int is_logged_in;
} client_t;

// Global variables
client_t clients[MAX_CLIENTS];
HANDLE clients_mutex;

// Function prototypes
DWORD WINAPI handle_client(LPVOID arg);
int authenticate_user(const char *username, const char *password);
int register_user(const char *username, const char *password);
void broadcast_message(Message *msg, SOCKET sender_socket);
void send_private_message(Message *msg, SOCKET sender_socket);
void send_chat_history(SOCKET client_socket);
void add_to_chat_log(Message *msg);
void initialize_server();
void cleanup_winsock();

int main() {
    SOCKET server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    HANDLE thread_handle;
    int client_len = sizeof(client_addr);
    WSADATA wsa_data;
    
    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        printf("Failed to initialize Winsock. Error Code: %d\n", WSAGetLastError());
        return 1;
    }
    
    // Create mutex for thread synchronization
    clients_mutex = CreateMutex(NULL, FALSE, NULL);
    if (clients_mutex == NULL) {
        printf("CreateMutex error: %d\n", GetLastError());
        WSACleanup();
        return 1;
    }
    
    // Initialize client list
    initialize_server();
    
    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == INVALID_SOCKET) {
        printf("Socket creation failed. Error Code: %d\n", WSAGetLastError());
        CloseHandle(clients_mutex);
        WSACleanup();
        return 1;
    }
    
    // Set socket options for reuse
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt)) == SOCKET_ERROR) {
        printf("Setsockopt failed. Error Code: %d\n", WSAGetLastError());
        closesocket(server_socket);
        CloseHandle(clients_mutex);
        WSACleanup();
        return 1;
    }
    
    // Prepare server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);
    
    // Bind socket
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        printf("Bind failed. Error Code: %d\n", WSAGetLastError());
        closesocket(server_socket);
        CloseHandle(clients_mutex);
        WSACleanup();
        return 1;
    }
    
    // Listen for connections
    if (listen(server_socket, 5) == SOCKET_ERROR) {
        printf("Listen failed. Error Code: %d\n", WSAGetLastError());
        closesocket(server_socket);
        CloseHandle(clients_mutex);
        WSACleanup();
        return 1;
    }
    
    // Print the server's local and public IP for clients to connect
    char hostname[256];
    gethostname(hostname, sizeof(hostname));
    struct hostent *he = gethostbyname(hostname);
    char *local_ip = inet_ntoa(*(struct in_addr*)(he->h_addr_list[0]));
    
    printf("Server started. Listening on:\n");
    printf("- Local IP (for same network): %s:%d\n", local_ip, SERVER_PORT);
    printf("- For connections from other networks, you need to set up port forwarding\n");
    printf("  in your router for port %d\n", SERVER_PORT);
    
    // Accept and handle client connections
    while (1) {
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket == INVALID_SOCKET) {
            printf("Accept failed. Error Code: %d\n", WSAGetLastError());
            continue;
        }
        
        char client_ip[INET_ADDRSTRLEN];
        // Use inet_ntoa instead of inet_ntop for better compatibility
        strcpy(client_ip, inet_ntoa(client_addr.sin_addr));
        printf("New connection from %s:%d\n", client_ip, ntohs(client_addr.sin_port));
        
        // Find free slot for client
        int slot = -1;
        WaitForSingleObject(clients_mutex, INFINITE);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].socket == INVALID_SOCKET) {
                clients[i].socket = client_socket;
                clients[i].is_logged_in = 0;
                slot = i;
                break;
            }
        }
        ReleaseMutex(clients_mutex);
        
        if (slot == -1) {
            printf("Server full, rejecting client\n");
            closesocket(client_socket);
            continue;
        }
        
        // Create thread to handle client - fixed parameter passing
        DWORD slot_id = (DWORD)slot;
        thread_handle = CreateThread(NULL, 0, handle_client, (LPVOID)slot_id, 0, NULL);
        if (thread_handle == NULL) {
            printf("Thread creation failed. Error Code: %d\n", GetLastError());
            closesocket(client_socket);
            
            WaitForSingleObject(clients_mutex, INFINITE);
            clients[slot].socket = INVALID_SOCKET;
            ReleaseMutex(clients_mutex);
        } else {
            CloseHandle(thread_handle); // Detach thread
        }
    }
    
    // Clean up
    closesocket(server_socket);
    CloseHandle(clients_mutex);
    WSACleanup();
    return 0;
}

void initialize_server() {
    // Initialize client array
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].socket = INVALID_SOCKET;
        clients[i].is_logged_in = 0;
    }
    
    // Create users file if it doesn't exist
    FILE *file = fopen(USERS_FILE, "a");
    if (file != NULL) {
        fclose(file);
    } else {
        perror("Failed to create users file");
        exit(EXIT_FAILURE);
    }
    
    // Create chat log file if it doesn't exist
    file = fopen(CHATLOG_FILE, "a");
    if (file != NULL) {
        fclose(file);
    } else {
        perror("Failed to create chat log file");
        exit(EXIT_FAILURE);
    }
}

DWORD WINAPI handle_client(LPVOID arg) {
    // Fixed typecasting for thread argument
    int index = (int)(DWORD_PTR)arg;
    SOCKET client_socket = clients[index].socket;
    Message msg;
    int read_size;
    
    while ((read_size = recv(client_socket, (char*)&msg, sizeof(Message), 0)) > 0) {
        // Process message based on type
        switch (msg.type) {
            case MSG_REGISTER: {
                // Extract password from message content
                char password[MAX_PASSWORD];
                strcpy(password, msg.content);
                
                // Try to register
                int result = register_user(msg.sender, password);
                
                // Send response
                Message response;
                response.type = result ? MSG_SUCCESS : MSG_ERROR;
                strcpy(response.sender, "SERVER");
                get_timestamp(response.timestamp, sizeof(response.timestamp));
                
                if (result) {
                    strcpy(response.content, "Registration successful");
                } else {
                    strcpy(response.content, "Username already exists");
                }
                
                send(client_socket, (const char*)&response, sizeof(Message), 0);
                break;
            }
            
            case MSG_LOGIN: {
                // Extract password from message content
                char password[MAX_PASSWORD];
                strcpy(password, msg.content);
                
                // Authenticate user
                int result = authenticate_user(msg.sender, password);
                
                // Send response
                Message response;
                response.type = result ? MSG_SUCCESS : MSG_ERROR;
                strcpy(response.sender, "SERVER");
                get_timestamp(response.timestamp, sizeof(response.timestamp));
                
                if (result) {
                    strcpy(response.content, "Login successful");
                    
                    // Update client info
                    WaitForSingleObject(clients_mutex, INFINITE);
                    strcpy(clients[index].username, msg.sender);
                    clients[index].is_logged_in = 1;
                    ReleaseMutex(clients_mutex);
                    
                    // Announce new user
                    Message announce;
                    announce.type = MSG_CHAT;
                    strcpy(announce.sender, "SERVER");
                    get_timestamp(announce.timestamp, sizeof(announce.timestamp));
                    sprintf(announce.content, "%s has joined the chat", msg.sender);
                    broadcast_message(&announce, INVALID_SOCKET);
                    add_to_chat_log(&announce);
                } else {
                    strcpy(response.content, "Invalid username or password");
                }
                
                send(client_socket, (const char*)&response, sizeof(Message), 0);
                break;
            }
            
            case MSG_CHAT:
                // Check if user is logged in
                if (!clients[index].is_logged_in) {
                    Message error;
                    error.type = MSG_ERROR;
                    strcpy(error.sender, "SERVER");
                    get_timestamp(error.timestamp, sizeof(error.timestamp));
                    strcpy(error.content, "You must be logged in to send messages");
                    send(client_socket, (const char*)&error, sizeof(Message), 0);
                    continue;
                }
                
                // Process message
                get_timestamp(msg.timestamp, sizeof(msg.timestamp));
                
                // Fix: Create a copy of the original message for broadcasting
                Message broadcast_copy = msg;
                
                // Only encrypt messages from regular users, not from SERVER
                if (strcmp(broadcast_copy.sender, "SERVER") != 0) {
                    encrypt_message(broadcast_copy.content);
                }
                broadcast_message(&broadcast_copy, client_socket);
                
                // Add original message to log (no need to decrypt since we never encrypted it)
                add_to_chat_log(&msg);
                break;
                
            case MSG_PRIVATE:
                // Check if user is logged in
                if (!clients[index].is_logged_in) {
                    Message error;
                    error.type = MSG_ERROR;
                    strcpy(error.sender, "SERVER");
                    get_timestamp(error.timestamp, sizeof(error.timestamp));
                    strcpy(error.content, "You must be logged in to send messages");
                    send(client_socket, (const char*)&error, sizeof(Message), 0);
                    continue;
                }
                
                // Process private message
                get_timestamp(msg.timestamp, sizeof(msg.timestamp));
                
                // Fix: Create a copy of the original message for sending
                Message private_copy = msg;
                
                encrypt_message(private_copy.content);
                send_private_message(&private_copy, client_socket);
                
                // Add original message to log
                add_to_chat_log(&msg);
                break;
                
            case MSG_HISTORY:
                // Check if user is logged in
                if (!clients[index].is_logged_in) {
                    Message error;
                    error.type = MSG_ERROR;
                    strcpy(error.sender, "SERVER");
                    get_timestamp(error.timestamp, sizeof(error.timestamp));
                    strcpy(error.content, "You must be logged in to view history");
                    send(client_socket, (const char*)&error, sizeof(Message), 0);
                    continue;
                }
                
                send_chat_history(client_socket);
                break;
                
            case MSG_LOGOUT:
                WaitForSingleObject(clients_mutex, INFINITE);
                if (clients[index].is_logged_in) {
                    // Announce user logout
                    Message announce;
                    announce.type = MSG_CHAT;
                    strcpy(announce.sender, "SERVER");
                    get_timestamp(announce.timestamp, sizeof(announce.timestamp));
                    sprintf(announce.content, "%s has left the chat", clients[index].username);
                    
                    clients[index].is_logged_in = 0;
                    ReleaseMutex(clients_mutex);
                    
                    broadcast_message(&announce, INVALID_SOCKET);
                    add_to_chat_log(&announce);
                } else {
                    ReleaseMutex(clients_mutex);
                }
                break;
        }
    }
    
    // Client disconnected
    if (read_size == 0) {
        printf("Client disconnected\n");
        
        WaitForSingleObject(clients_mutex, INFINITE);
        if (clients[index].is_logged_in) {
            // Announce disconnect
            Message announce;
            announce.type = MSG_CHAT;
            strcpy(announce.sender, "SERVER");
            get_timestamp(announce.timestamp, sizeof(announce.timestamp));
            sprintf(announce.content, "%s has disconnected", clients[index].username);
            
            clients[index].is_logged_in = 0;
            ReleaseMutex(clients_mutex);
            
            broadcast_message(&announce, INVALID_SOCKET);
            add_to_chat_log(&announce);
        } else {
            ReleaseMutex(clients_mutex);
        }
    } else if (read_size == SOCKET_ERROR) {
        printf("recv failed. Error Code: %d\n", WSAGetLastError());
    }
    
    // Clean up client slot
    WaitForSingleObject(clients_mutex, INFINITE);
    closesocket(clients[index].socket);
    clients[index].socket = INVALID_SOCKET;
    clients[index].is_logged_in = 0;
    ReleaseMutex(clients_mutex);
    
    return 0;
}

int authenticate_user(const char *username, const char *password) {
    FILE *file = fopen(USERS_FILE, "r");
    if (file == NULL) {
        return 0;
    }
    
    char line[MAX_USERNAME + MAX_PASSWORD + 2];
    char stored_username[MAX_USERNAME];
    char stored_password[MAX_PASSWORD];
    
    while (fgets(line, sizeof(line), file)) {
        // Remove newline
        line[strcspn(line, "\n")] = 0;
        
        // Parse username and password (format: username:password)
        char *token = strtok(line, ":");
        if (token != NULL) {
            strcpy(stored_username, token);
            token = strtok(NULL, ":");
            if (token != NULL) {
                strcpy(stored_password, token);
                
                // Compare credentials
                if (strcmp(username, stored_username) == 0 && 
                    strcmp(password, stored_password) == 0) {
                    fclose(file);
                    return 1; // Authentication successful
                }
            }
        }
    }
    
    fclose(file);
    return 0; // Authentication failed
}

int register_user(const char *username, const char *password) {
    // Check if username already exists
    FILE *file = fopen(USERS_FILE, "r");
    if (file != NULL) {
        char line[MAX_USERNAME + MAX_PASSWORD + 2];
        char stored_username[MAX_USERNAME];
        
        while (fgets(line, sizeof(line), file)) {
            // Remove newline
            line[strcspn(line, "\n")] = 0;
            
            // Parse username
            char *token = strtok(line, ":");
            if (token != NULL) {
                strcpy(stored_username, token);
                
                // Check if username exists
                if (strcmp(username, stored_username) == 0) {
                    fclose(file);
                    return 0; // Username already exists
                }
            }
        }
        
        fclose(file);
    }
    
    // Add new user
    file = fopen(USERS_FILE, "a");
    if (file == NULL) {
        return 0;
    }
    
    fprintf(file, "%s:%s\n", username, password);
    fclose(file);
    
    return 1; // Registration successful
}

void broadcast_message(Message *msg, SOCKET sender_socket) {
    WaitForSingleObject(clients_mutex, INFINITE);
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket != INVALID_SOCKET && clients[i].is_logged_in && 
            clients[i].socket != sender_socket) {
            // Fix: Create a copy of the message for each recipient to avoid any issues
            Message recipient_copy = *msg;
            
            if (send(clients[i].socket, (const char*)&recipient_copy, sizeof(Message), 0) == SOCKET_ERROR) {
                printf("Send failed. Error Code: %d\n", WSAGetLastError());
            }
        }
    }
    
    ReleaseMutex(clients_mutex);
}

void send_private_message(Message *msg, SOCKET sender_socket) {
    WaitForSingleObject(clients_mutex, INFINITE);
    
    // Send to recipient
    int found = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket != INVALID_SOCKET && clients[i].is_logged_in && 
            strcmp(clients[i].username, msg->recipient) == 0) {
            // Fix: Create a copy of the message to avoid any potential issues
            Message recipient_copy = *msg;
            
            if (send(clients[i].socket, (const char*)&recipient_copy, sizeof(Message), 0) == SOCKET_ERROR) {
                printf("Send failed. Error Code: %d\n", WSAGetLastError());
            }
            found = 1;
            break;
        }
    }
    
    // Send confirmation to sender
    Message response;
    response.type = found ? MSG_SUCCESS : MSG_ERROR;
    strcpy(response.sender, "SERVER");
    get_timestamp(response.timestamp, sizeof(response.timestamp));
    
    if (found) {
        sprintf(response.content, "Private message sent to %s", msg->recipient);
    } else {
        sprintf(response.content, "User %s not found or offline", msg->recipient);
    }
    
    // Find sender
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket == sender_socket) {
            send(clients[i].socket, (const char*)&response, sizeof(Message), 0);
            break;
        }
    }
    
    ReleaseMutex(clients_mutex);
}

void send_chat_history(SOCKET client_socket) {
    FILE *file = fopen(CHATLOG_FILE, "r");
    if (file == NULL) {
        Message error;
        error.type = MSG_ERROR;
        strcpy(error.sender, "SERVER");
        get_timestamp(error.timestamp, sizeof(error.timestamp));
        strcpy(error.content, "Chat history not available");
        send(client_socket, (const char*)&error, sizeof(Message), 0);
        return;
    }
    
    char line[MAX_USERNAME + MAX_MESSAGE + 50];
    Message history;
    history.type = MSG_HISTORY;
    strcpy(history.sender, "SERVER");
    
    // Send start message
    strcpy(history.content, "--- Chat History ---");
    get_timestamp(history.timestamp, sizeof(history.timestamp));
    send(client_socket, (const char*)&history, sizeof(Message), 0);
    
    // Send each line of history
    while (fgets(line, sizeof(line), file)) {
        // Remove newline
        line[strcspn(line, "\n")] = 0;
        
        strcpy(history.content, line);
        send(client_socket, (const char*)&history, sizeof(Message), 0);
        
        // Add small delay to prevent flooding
        Sleep(10); // Windows equivalent of usleep(10000)
    }
    
    // Send end message
    strcpy(history.content, "--- End of History ---");
    get_timestamp(history.timestamp, sizeof(history.timestamp));
    send(client_socket, (const char*)&history, sizeof(Message), 0);
    
    fclose(file);
}

void add_to_chat_log(Message *msg) {
    FILE *file = fopen(CHATLOG_FILE, "a");
    if (file == NULL) {
        perror("Failed to open chat log");
        return;
    }
    
    if (msg->type == MSG_PRIVATE) {
        fprintf(file, "[%s] %s -> %s: %s\n", 
                msg->timestamp, msg->sender, msg->recipient, msg->content);
    } else {
        fprintf(file, "[%s] %s: %s\n", 
                msg->timestamp, msg->sender, msg->content);
    }
    
    fclose(file);
}

void cleanup_winsock() {
    // Close all client sockets first
    WaitForSingleObject(clients_mutex, INFINITE);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket != INVALID_SOCKET) {
            closesocket(clients[i].socket);
            clients[i].socket = INVALID_SOCKET;
        }
    }
    ReleaseMutex(clients_mutex);
    
    // Clean up Winsock
    WSACleanup();
    printf("Winsock cleanup complete\n");
}
