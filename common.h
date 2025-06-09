#ifndef COMMON_H
#define COMMON_H
// common header file-
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>  // Added for intptr_t
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

// Define INET_ADDRSTRLEN if it's not defined
#ifndef INET_ADDRSTRLEN
#define INET_ADDRSTRLEN 16
#endif

#define MAX_USERNAME 50
#define MAX_PASSWORD 50
#define MAX_MESSAGE 1000
#define BUFFER_SIZE 1200
#define MAX_CLIENTS 10
#define USERS_FILE "users.txt"
#define CHATLOG_FILE "chatlog.txt"
#define SERVER_PORT 8888

// Message types:-
#define MSG_REGISTER 1
#define MSG_LOGIN 2
#define MSG_CHAT 3
#define MSG_PRIVATE 4
#define MSG_HISTORY 5
#define MSG_LOGOUT 6
#define MSG_SUCCESS 7
#define MSG_ERROR 8

// Message structure - defined in common.h only
typedef struct {
    int type;
    char sender[MAX_USERNAME];
    char recipient[MAX_USERNAME]; // For private messages only 
    char timestamp[26];
    char content[MAX_MESSAGE];
} Message;

// Client structure - defined in common.h only
typedef struct {
    SOCKET socket;
    char username[MAX_USERNAME];
    int is_logged_in;
} client_t;

// Function to get current timestamp
void get_timestamp(char *timestamp, size_t size) {
    time_t now;
    struct tm *local_time;
    
    time(&now);
    local_time = localtime(&now);
    strftime(timestamp, size, "%Y-%m-%d %H:%M:%S", local_time);
}

// Simple Caesar cipher encryption (shift by 3)
void encrypt_message(char *message) {
    for (int i = 0; message[i] != '\0'; i++) {
        if (message[i] >= 'a' && message[i] <= 'z') {
            message[i] = ((message[i] - 'a' + 3) % 26) + 'a';
        } else if (message[i] >= 'A' && message[i] <= 'Z') {
            message[i] = ((message[i] - 'A' + 3) % 26) + 'A';
        }
    }
}

// Simple Caesar cipher decryption (shift by -3)
void decrypt_message(char *message) {
    for (int i = 0; message[i] != '\0'; i++) {
        if (message[i] >= 'a' && message[i] <= 'z') {
            message[i] = ((message[i] - 'a' + 23) % 26) + 'a'; // +23 is equivalent to -3 mod 26
        } else if (message[i] >= 'A' && message[i] <= 'Z') {
            message[i] = ((message[i] - 'A' + 23) % 26) + 'A';
        }
    }
}

#endif // COMMON_H
