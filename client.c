#include "common.h"

// Global variables
SOCKET server_socket;
char username[MAX_USERNAME];
int logged_in = 0;
HANDLE recv_thread;
int running = 1;

// Function prototypes
DWORD WINAPI receive_messages(LPVOID arg);
void display_menu();
void register_user();
void login_user();
void send_chat_message();
void send_private_message();
void request_chat_history();
void logout_user();
void cleanup();
void enter_chat_mode();

int main() {
    struct sockaddr_in server_addr;
    WSADATA wsa_data;
    
    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        printf("Failed to initialize Winsock. Error Code: %d\n", WSAGetLastError());
        return 1;
    }
    
    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == INVALID_SOCKET) {
        printf("Socket creation failed. Error Code: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }
    
    // Prepare server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    
    // Convert IP address from text to binary - using inet_addr instead of inet_pton for better compatibility
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (server_addr.sin_addr.s_addr == INADDR_NONE) {
        printf("Invalid address or address not supported\n");
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }
    
    // Connection attempt with retry logic
    int max_retries = 3;
    int retry_count = 0;
    int connected = 0;
    
    printf("Attempting to connect to server at 127.0.0.1:%d...\n", SERVER_PORT);
    
    while (retry_count < max_retries && !connected) {
        if (connect(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (error == WSAECONNREFUSED) {
                printf("Connection attempt %d failed: Connection refused (Error 10061)\n", retry_count + 1);
                printf("Possible causes:\n");
                printf("1. The server is not running\n");
                printf("2. The server is not listening on port %d\n", SERVER_PORT);
                printf("3. A firewall is blocking the connection\n");
                
                if (retry_count < max_retries - 1) {
                    printf("Retrying in 2 seconds...\n");
                    Sleep(2000); // Wait 2 seconds before retrying
                }
            } else {
                printf("Connection failed. Error Code: %d\n", error);
                closesocket(server_socket);
                WSACleanup();
                return 1;
            }
            retry_count++;
        } else {
            connected = 1;
        }
    }
    
    if (!connected) {
        printf("Failed to connect after %d attempts. Please:\n", max_retries);
        printf("1. Ensure the server is running (run server.exe first)\n");
        printf("2. Check if a firewall is blocking the connection\n");
        printf("3. Verify the server is configured to use port %d\n", SERVER_PORT);
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }
    
    printf("Connected to chat server.\n");
    
    // Main menu loop
    while (running) {
        display_menu();
        
        int choice;
        printf("Enter your choice: ");
        scanf("%d", &choice);
        getchar(); // Consume newline
        
        switch (choice) {
            case 1:
                register_user();
                break;
            case 2:
                login_user();
                break;
            case 3:
                if (logged_in) {
                    send_chat_message();
                } else {
                    printf("You must be logged in to send messages.\n");
                }
                break;
            case 4:
                if (logged_in) {
                    send_private_message();
                } else {
                    printf("You must be logged in to send private messages.\n");
                }
                break;
            case 5:
                if (logged_in) {
                    request_chat_history();
                } else {
                    printf("You must be logged in to view chat history.\n");
                }
                break;
            case 6:
                if (logged_in) {
                    logout_user();
                } else {
                    printf("You are not logged in.\n");
                }
                break;
            case 7:
                running = 0;
                break;
            case 8:
                if (logged_in) {
                    enter_chat_mode();
                } else {
                    printf("You must be logged in to enter chat mode.\n");
                }
                break;
            default:
                printf("Invalid choice. Please try again.\n");
        }
    }
    
    // Clean up
    cleanup();
    return 0;
}

void display_menu() {
    printf("\n===== Chat Client Menu =====\n");
    printf("Status: %s as %s\n", logged_in ? "Logged in" : "Not logged in", 
            logged_in ? username : "Guest");
    printf("1. Register new account\n");
    printf("2. Login\n");
    printf("3. Send public message\n");
    printf("4. Send private message\n");
    printf("5. View chat history\n");
    printf("6. Logout\n");
    printf("7. Exit\n");
    
    // Add chat mode option for logged-in users
    if (logged_in) {
        printf("8. Enter chat mode (continuous messaging)\n");
    }
}

void register_user() {
    char password[MAX_PASSWORD];
    
    printf("\n===== Register New Account =====\n");
    printf("Enter username: ");
    fgets(username, sizeof(username), stdin);
    username[strcspn(username, "\n")] = 0; // Remove newline
    
    printf("Enter password: ");
    fgets(password, sizeof(password), stdin);
    password[strcspn(password, "\n")] = 0; // Remove newline
    
    // Prepare registration message
    Message msg;
    msg.type = MSG_REGISTER;
    strcpy(msg.sender, username);
    strcpy(msg.content, password);
    
    // Send registration request
    if (send(server_socket, (const char*)&msg, sizeof(Message), 0) == SOCKET_ERROR) {
        printf("Send failed. Error Code: %d\n", WSAGetLastError());
        return;
    }
    
    // Receive response
    if (recv(server_socket, (char*)&msg, sizeof(Message), 0) == SOCKET_ERROR) {
        printf("Recv failed. Error Code: %d\n", WSAGetLastError());
        return;
    }
    
    if (msg.type == MSG_SUCCESS) {
        printf("Registration successful! You can now login.\n");
    } else {
        printf("Registration failed: %s\n", msg.content);
    }
}

void login_user() {
    char password[MAX_PASSWORD];
    
    printf("\n===== Login =====\n");
    printf("Enter username: ");
    fgets(username, sizeof(username), stdin);
    username[strcspn(username, "\n")] = 0; // Remove newline
    
    printf("Enter password: ");
    fgets(password, sizeof(password), stdin);
    password[strcspn(password, "\n")] = 0; // Remove newline
    
    // Prepare login message
    Message msg;
    memset(&msg, 0, sizeof(Message)); // Clear the message structure
    msg.type = MSG_LOGIN;
    strcpy(msg.sender, username);
    strcpy(msg.content, password);
    
    printf("Sending login request...\n");
    
    // Send login request
    if (send(server_socket, (const char*)&msg, sizeof(Message), 0) == SOCKET_ERROR) {
        printf("Send failed. Error Code: %d\n", WSAGetLastError());
        return;
    }
    
    // Receive response with better handling of message types
    printf("Waiting for server response...\n");
    
    // If the server sends a chat message announcing the join,
    // it means the login was successful even if we didn't receive
    // the explicit success message
    int login_success = 0;
    int max_attempts = 3;
    int attempts = 0;
    
    while (!login_success && attempts < max_attempts) {
        fd_set readSet;
        struct timeval timeout;
        timeout.tv_sec = 3;  // 3 second timeout
        timeout.tv_usec = 0;
        
        FD_ZERO(&readSet);
        FD_SET(server_socket, &readSet);
        
        if (select(0, &readSet, NULL, NULL, &timeout) <= 0) {
            printf("No response from server. Attempt %d of %d.\n", attempts + 1, max_attempts);
            attempts++;
            continue;
        }
        
        if (recv(server_socket, (char*)&msg, sizeof(Message), 0) == SOCKET_ERROR) {
            printf("Recv failed. Error Code: %d\n", WSAGetLastError());
            return;
        }
        
        // Process the received message
        switch (msg.type) {
            case MSG_SUCCESS:
                printf("Login successful! Welcome to the chat.\n");
                login_success = 1;
                break;
                
            case MSG_ERROR:
                printf("Login failed: %s\n", msg.content);
                return;
                
            case MSG_CHAT:
                // Check if this is a join announcement from the server
                if (strcmp(msg.sender, "SERVER") == 0 && 
                    strstr(msg.content, username) != NULL && 
                    strstr(msg.content, "joined the chat") != NULL) {
                    printf("Login successful! Welcome to the chat.\n");
                    login_success = 1;
                } else {
                    // Store this message for later display
                    printf("Received chat message during login: [%s] %s: %s\n", 
                           msg.timestamp, msg.sender, msg.content);
                }
                break;
                
            default:
                printf("Received unexpected message type: %d\n", msg.type);
                // Continue trying - don't return
                break;
        }
        
        attempts++;
    }
    
    if (login_success) {
        logged_in = 1;
        
        // Start message receiver thread
        printf("Starting message receiver...\n");
        recv_thread = CreateThread(NULL, 0, receive_messages, NULL, 0, NULL);
        if (recv_thread == NULL) {
            printf("Thread creation failed. Error Code: %d\n", GetLastError());
            logged_in = 0;
            return;
        }
        
        // Make thread detached so it cleans up automatically
        CloseHandle(recv_thread);
        
        printf("You are now logged in and can send messages.\n");
    } else {
        printf("Failed to receive proper login confirmation from server.\n");
    }
}

void send_chat_message() {
    char message[MAX_MESSAGE];
    
    printf("\n===== Send Public Message =====\n");
    printf("Enter message (press Enter to send):\n");
    fgets(message, sizeof(message), stdin);
    message[strcspn(message, "\n")] = 0; // Remove newline
    
    // Check if message is empty
    if (strlen(message) == 0) {
        printf("Message cannot be empty.\n");
        return;
    }
    
    // Prepare message
    Message msg;
    memset(&msg, 0, sizeof(Message)); // Clear the message structure
    msg.type = MSG_CHAT;
    strcpy(msg.sender, username);
    
    // Fix: Add a special marker character at the beginning that won't be affected by encryption
    char marker_message[MAX_MESSAGE];
    sprintf(marker_message, "#%s", message); // Add # as a marker
    strcpy(msg.content, marker_message);
    
    printf("Sending message...\n");
    
    // Send message
    if (send(server_socket, (const char*)&msg, sizeof(Message), 0) == SOCKET_ERROR) {
        printf("Send failed. Error Code: %d\n", WSAGetLastError());
        printf("You may have been disconnected. Please try logging in again.\n");
        logged_in = 0;
    } else {
        printf("Message sent successfully.\n");
    }
}

void send_private_message() {
    char recipient[MAX_USERNAME];
    char message[MAX_MESSAGE];
    
    printf("\n===== Send Private Message =====\n");
    printf("Enter recipient username: ");
    fgets(recipient, sizeof(recipient), stdin);
    recipient[strcspn(recipient, "\n")] = 0; // Remove newline
    
    printf("Enter message (press Enter to send):\n");
    fgets(message, sizeof(message), stdin);
    message[strcspn(message, "\n")] = 0; // Remove newline
    
    // Check if message is empty
    if (strlen(message) == 0) {
        printf("Message cannot be empty.\n");
        return;
    }
    
    // Prepare message
    Message msg;
    msg.type = MSG_PRIVATE;
    strcpy(msg.sender, username);
    strcpy(msg.recipient, recipient);
    
    // Fix: Add a special marker character at the beginning
    char marker_message[MAX_MESSAGE];
    sprintf(marker_message, "#%s", message); // Add # as a marker
    strcpy(msg.content, marker_message);
    
    // Send message
    if (send(server_socket, (const char*)&msg, sizeof(Message), 0) == SOCKET_ERROR) {
        printf("Send failed. Error Code: %d\n", WSAGetLastError());
    }
}

void request_chat_history() {
    // Prepare request
    Message msg;
    msg.type = MSG_HISTORY;
    strcpy(msg.sender, username);
    
    // Send request
    if (send(server_socket, (const char*)&msg, sizeof(Message), 0) == SOCKET_ERROR) {
        printf("Send failed. Error Code: %d\n", WSAGetLastError());
    }
    
    printf("\nRequesting chat history...\n");
}

void logout_user() {
    // Prepare logout message
    Message msg;
    msg.type = MSG_LOGOUT;
    strcpy(msg.sender, username);
    
    // Send logout request
    if (send(server_socket, (const char*)&msg, sizeof(Message), 0) == SOCKET_ERROR) {
        printf("Send failed. Error Code: %d\n", WSAGetLastError());
    }
    
    logged_in = 0;
    printf("You have been logged out.\n");
    
    // Wait for receiver thread to terminate
    if (recv_thread != NULL) {
        WaitForSingleObject(recv_thread, 1000); // Wait up to 1 second
        CloseHandle(recv_thread);
        recv_thread = NULL;
    }
}

void enter_chat_mode() {
    char message[MAX_MESSAGE];
    Message msg;
    
    printf("\n===== Chat Mode =====\n");
    printf("Type your messages and press Enter to send.\n");
    printf("Type '/exit' to return to the main menu.\n\n");
    
    while (logged_in) {
        printf("Message: ");
        fgets(message, sizeof(message), stdin);
        message[strcspn(message, "\n")] = 0; // Remove newline
        
        // Check if user wants to exit chat mode
        if (strcmp(message, "/exit") == 0) {
            printf("Exiting chat mode.\n");
            break;
        }
        
        // Check if message is empty
        if (strlen(message) == 0) {
            continue;
        }
        
        // Prepare message
        memset(&msg, 0, sizeof(Message)); // Clear the message structure
        msg.type = MSG_CHAT;
        strcpy(msg.sender, username);
        
        // Fix: Add a special marker character at the beginning
        char marker_message[MAX_MESSAGE];
        sprintf(marker_message, "#%s", message); // Add # as a marker
        strcpy(msg.content, marker_message);
        
        // Send message
        if (send(server_socket, (const char*)&msg, sizeof(Message), 0) == SOCKET_ERROR) {
            printf("Send failed. Error Code: %d\n", WSAGetLastError());
            printf("You may have been disconnected. Please try logging in again.\n");
            logged_in = 0;
        }
    }
}

DWORD WINAPI receive_messages(LPVOID arg) {
    Message msg;
    int read_size;
    
    printf("Message receiver started. Listening for incoming messages...\n");
    
    while (logged_in) {
        // Clear the message buffer before receiving
        memset(&msg, 0, sizeof(Message));
        
        // Receive message with timeout to allow checking logged_in flag
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(server_socket, &readSet);
        
        struct timeval timeout;
        timeout.tv_sec = 1;  // 1 second timeout
        timeout.tv_usec = 0;
        
        int selectResult = select(0, &readSet, NULL, NULL, &timeout);
        
        if (selectResult == SOCKET_ERROR) {
            printf("\nSelect failed. Error Code: %d\n", WSAGetLastError());
            break;
        }
        
        if (selectResult > 0 && FD_ISSET(server_socket, &readSet)) {
            read_size = recv(server_socket, (char*)&msg, sizeof(Message), 0);
            
            if (read_size > 0) {
                // Process message based on type
                switch (msg.type) {
                    case MSG_CHAT:
                        // Only decrypt messages from non-SERVER senders
                        char decrypted_content[MAX_MESSAGE];
                        strcpy(decrypted_content, msg.content);
                        
                        // Only decrypt messages from regular users, not SERVER messages
                        if (strcmp(msg.sender, "SERVER") != 0) {
                            decrypt_message(decrypted_content);
                            
                            // Remove the marker character if present
                            if (decrypted_content[0] == '#') {
                                memmove(decrypted_content, decrypted_content + 1, strlen(decrypted_content));
                            }
                        }
                        printf("\n[%s] %s: %s\n", msg.timestamp, msg.sender, decrypted_content);
                        break;
                    
                    case MSG_PRIVATE:
                        char private_content[MAX_MESSAGE];
                        strcpy(private_content, msg.content);
                        decrypt_message(private_content);
                        
                        // Remove the marker character if present
                        if (private_content[0] == '#') {
                            memmove(private_content, private_content + 1, strlen(private_content));
                        }
                        
                        printf("\n[PRIVATE] [%s] %s: %s\n", msg.timestamp, msg.sender, private_content);
                        break;
                    
                    case MSG_HISTORY:
                        printf("\n%s\n", msg.content);
                        break;
                    
                    case MSG_SUCCESS:
                        printf("\n[SERVER] %s\n", msg.content);
                        break;
                    
                    case MSG_ERROR:
                        printf("\n[ERROR] %s\n", msg.content);
                        break;
                    
                    default:
                        printf("\n[UNKNOWN] Received unknown message type: %d\n", msg.type);
                        break;
                }
                
                // Check if we're in the main thread or chat mode before showing prompt
                DWORD current_thread_id = GetCurrentThreadId();
                DWORD main_thread_id = GetCurrentThreadId(); // This will be different in the receiver thread
                
                if (current_thread_id != main_thread_id) {
                    printf("\nMessage: "); // Better prompt for chat mode
                } else {
                    printf("\nEnter your choice: "); // Original prompt for menu
                }
                fflush(stdout);
            } else if (read_size == 0) {
                printf("\nServer disconnected.\n");
                logged_in = 0;
                break;
            } else {
                printf("\nRecv failed. Error Code: %d\n", WSAGetLastError());
                logged_in = 0;
                break;
            }
        }
    }
    
    printf("Message receiver stopped.\n");
    return 0;
}

void cleanup() {
    // If logged in, send logout message
    if (logged_in) {
        logout_user();
    }
    
    // Close socket
    closesocket(server_socket);
    WSACleanup();
    printf("Disconnected from server.\n");
}
