#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>


// Define a structure for a node in the linked list
typedef struct Node {
    char *value;
    struct Node *next;
} Node;

struct hostent* server_info=NULL;

// Declare global variable for the linked list
Node *pathList;

// Declare global variables for storing URL components
char *protocol, *hostname, *port, *filepath;

/**
 * @brief Constructs an HTTP request based on the options specified in the command line.
 *
 * @param hostname The hostname of the server.
 * @param port The port number for the server.
 * @param filepath The filepath of the requested resource.
 * @return The constructed HTTP request string.
 */
char *constructHTTPRequest(const char *hostname, int port, const char *filepath) {
    // Allocate memory for the HTTP request string
    char *request = (char *) malloc(512);

    // Construct the HTTP request string
    snprintf(request, 512,
             "GET %s HTTP/1.0\r\n"
             "Host: %s:%d\r\n"
             "\r\n", filepath, hostname, port);

    return request;
}

/**
 * @brief Sends an HTTP request to the server and receives the response.
 *
 * @param hostname The hostname of the server.
 * @param port The port number for the server.
 * @param filepath The filepath of the requested resource.
 * @param localFilePath The path where the received file should be saved locally.
 */
// Function to send an HTTP request to the server and receive the response
void sendHTTPRequestAndReceiveResponse(const char *hostname, const char *port, const char *filepath) {
    // Construct HTTP request
    char request[1024];  // Adjust the size as needed
    sprintf(request, "GET %s HTTP/1.0\r\nHost: %s\r\n\r\n", filepath, hostname);

    // Print the HTTP request
    printf("HTTP request =\n%s\nLEN = %zu\n", request, strlen(request));

    // Connect to the server
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_info = gethostbyname(hostname);
    if (!server_info) {
        fprintf(stderr, "gethostbyname failed: %s\n", hstrerror(h_errno));
        close(sockfd);
        exit(EXIT_FAILURE);
    }


    struct sockaddr_in serverAddr;
    memset(&serverAddr,0,sizeof (struct sockaddr_in));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    // Set the server address using the first IP address from the hostent structure
    serverAddr.sin_addr.s_addr = ((struct in_addr *)server_info->h_addr)->s_addr;

    // Connect to the server
    if (connect(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1) {
        perror("Connection to server failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }


    // Send the HTTP request to the server
    if (send(sockfd, request, strlen(request), 0) == -1) {
        perror("Failed to send HTTP request");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Receive an HTTP response
    char response[1024];  // Adjust the size as needed
    ssize_t bytesRead = recv(sockfd, response, sizeof(response) - 1, 0);
    if (bytesRead == -1) {
        perror("Failed to receive HTTP response");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    response[bytesRead] = '\0';

    // Save the file locally
    FILE *file = fopen(filepath, "w");
    if (file == NULL) {
        perror("Error opening file for writing");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    fprintf(file, "%s", response);
    fclose(file);

    // Close the socket
    close(sockfd);

    printf("File saved locally: %s\n", filepath);
}

/**
 * @brief Splits a path and stores each segment in a linked list.
 *
 * Given a path, this function splits it at each "/" and stores each segment
 * in a linked list.
 *
 * @param path The input path to be split and stored.
 */
void splitAndStorePath(const char *path) {
    // Find the position of the first "/"
    const char *pathStart = path;
    const char *pathEnd = strchr(pathStart, '/');

    // Iterate through the path and split at each "/"
    while (pathEnd != NULL) {
        // Extract the segment between pathStart and pathEnd
        size_t segmentLength = pathEnd - pathStart;
        char *segment = malloc(segmentLength + 1);
        strncpy(segment, pathStart, segmentLength);
        segment[segmentLength] = '\0';

        // Create a new node for the linked list
        Node *newNode = malloc(sizeof(Node));
        newNode->value = segment;
        newNode->next = NULL;

        // Add the new node to the linked list
        if (pathList == NULL) {
            // If the list is empty, set the new node as the head
            pathList = newNode;
        } else {
            // Otherwise, find the end of the list and append the new node
            Node *current = pathList;
            while (current->next != NULL) {
                current = current->next;
            }
            current->next = newNode;
        }

        // Move to the next segment
        pathStart = pathEnd + 1;
        pathEnd = strchr(pathStart, '/');
    }

    // Handle the last segment after the last "/"
    size_t lastSegmentLength = strlen(pathStart);
    if (lastSegmentLength > 0) {
        char *lastSegment = malloc(lastSegmentLength + 1);
        strcpy(lastSegment, pathStart);

        Node *newNode = malloc(sizeof(Node));
        newNode->value = lastSegment;
        newNode->next = NULL;

        if (pathList == NULL) {
            pathList = newNode;
        } else {
            Node *current = pathList;
            while (current->next != NULL) {
                current = current->next;
            }
            current->next = newNode;
        }
    }
}

/**
 * @brief Splits a URL into its components.
 *
 * Given a URL, this function extracts the protocol, hostname, port, and filepath.
 * The components are stored in global variables. It also calls splitAndStorePath
 * to store the path segments in a linked list.
 *
 * @param url The input URL to be split.
 */
void splitURL(const char *url) {
    // Find the position of "://"
    const char *protocolEnd = strstr(url, "://");
    if (protocolEnd == NULL) {
        fprintf(stderr, "Invalid URL format: %s\n", url);
        exit(EXIT_FAILURE);
    }

    // Extract protocol
    protocol = malloc(protocolEnd - url + 1);
    strncpy(protocol, url, protocolEnd - url);
    protocol[protocolEnd - url] = '\0';

    // Move to the hostname part
    const char *hostnameStart = protocolEnd + 3;

    // Find the position of ":"
    const char *portStart = strchr(hostnameStart, ':');
    const char *pathStart = strchr(hostnameStart, '/');

    if (portStart != NULL && (pathStart == NULL || portStart < pathStart)) {
        // Extract hostname up to the port
        hostname = malloc(portStart - hostnameStart + 1);
        strncpy(hostname, hostnameStart, portStart - hostnameStart);
        hostname[portStart - hostnameStart] = '\0';

        // Move to the port part
        const char *portEnd = strchr(portStart, '/');

        // Check if the port contains only digits
        if (portEnd != NULL && portEnd > portStart + 1) {
            int isDigit = 1;
            for (const char *digitCheck = portStart + 1; digitCheck < portEnd; ++digitCheck) {
                if (!isdigit(*digitCheck)) {
                    isDigit = 0;
                    break;
                }
            }

            // If the port contains non-digit characters, set it to "80"
            if (!isDigit) {
                port = malloc(3);
                strcpy(port, "80");
                // Extract filepath
                filepath = malloc(strlen(portEnd) + 1);
                strcpy(filepath, portEnd);
                // Call splitAndStorePath to store path segments in the linked list
                splitAndStorePath(filepath);
                return;
            }
        }

        if (portEnd != NULL) {
            // Extract port
            port = malloc(portEnd - portStart);
            strncpy(port, portStart + 1, portEnd - portStart - 1);
            port[portEnd - portStart - 1] = '\0';

            // Extract filepath
            filepath = malloc(strlen(portEnd) + 1);
            strcpy(filepath, portEnd);
        } else {
            // No slash after port, extract the rest as the port
            port = malloc(strlen(portStart + 1) + 1);
            strcpy(port, portStart + 1);

            // Set filepath to "/"
            filepath = malloc(2);
            strcpy(filepath, "/");
        }
    } else {
        // No port specified or port appears after a slash
        if (pathStart != NULL) {
            // Extract hostname up to the first "/"
            hostname = malloc(pathStart - hostnameStart + 1);
            strncpy(hostname, hostnameStart, pathStart - hostnameStart);
            hostname[pathStart - hostnameStart] = '\0';

            // Extract filepath
            port = malloc(3);
            strcpy(port, "80");
            filepath = malloc(strlen(pathStart) + 1);
            strcpy(filepath, pathStart);
        } else {
            // No port and no path specified
            hostname = malloc(strlen(hostnameStart) + 1);
            strcpy(hostname, hostnameStart);

            // Check for colon (:) without a number until the first "/"
            const char *colonStart = strchr(hostnameStart, ':');
            if (colonStart != NULL && (pathStart == NULL || colonStart < pathStart)) {
                port = malloc(3);
                strcpy(port, "80");
            } else {
                port = malloc(1);
                port[0] = '\0';
            }

            // Set filepath to "/"
            filepath = malloc(2);
            strcpy(filepath, "/");
        }
    }

    // Check if the port length is zero, then set it to "80"
    if (strlen(port) == 0) {
        free(port);
        port = malloc(3);
        strcpy(port, "80");
    }

    // Call splitAndStorePath to store path segments in the linked list
    splitAndStorePath(filepath);
}

/**
 * @brief Prints and displays the values in the linked list.
 */
void printPathList() {
    printf("Path List:");
    Node *current = pathList;
    while (current != NULL) {
        printf("%s\n", current->value);
        current = current->next;
    }
}

/**
 * @brief Frees the memory allocated for the linked list.
 */
void freePathList() {
    Node *current = pathList;
    while (current != NULL) {
        Node *temp = current;
        current = current->next;
        free(temp->value);
        free(temp);
    }
    pathList = NULL;
}

/**
 * @brief Generates an HTTP response for a file.
 *
 * Given a file path, this function generates an HTTP response
 * with the following format:
 * HTTP/1.0 200 OK\r\n
 * Content-Length: N\r\n\r\n
 * Where N is the file size in bytes.
 *
 * @param filePath The path to the file.
 */
void generateHTTPResponse(const char *filePath) {
    // Open the file
    FILE *file = fopen(filePath, "rb");
    if (file == NULL) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Generate HTTP response
    printf("HTTP/1.0 200 OK\r\n");
    printf("Content-Length: %ld\r\n\r\n", fileSize);

    // Print file content to stdout (you might want to send it to the client in a real proxy)
    char buffer[1024];
    size_t bytesRead;
    size_t totalBytes = 0;  // Variable to track total response bytes

    while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        fwrite(buffer, 1, bytesRead, stdout);
        totalBytes += bytesRead;  // Update total response bytes
    }

    // Close the file
    fclose(file);

    // Print total response bytes
    printf("\nTotal response bytes: %zu\n", totalBytes);
}


/**
 * @brief Checks if the specified directory structure exists.
 *
 * Given a hostname and a linked list representing the path, this function
 * checks if the specified directory structure exists. The last segment in
 * the path list is assumed to be a file.
 *
 * @param hostname The hostname for the top-level directory.
 * @param pathList Linked list representing the internal folders.
 */
void checkDirectoryExistence(const char *hostname, Node *pathList) {
    char currentPath[256];  // Start with the current directory
    memset(currentPath, 0, 256);
    strcat(currentPath, hostname);

//    // Check if the top-level directory exists
//    if (access(currentPath, F_OK) == -1) {
//        printf("Directory does not exist: %s\n", currentPath);
//        return;
//    }

    int flag_First_in_list = 0;  // check if is the first in the liked-list , if yes ,do not add another /

    // Iterate through the linked list and check subdirectories
    Node *current = pathList;
    while (current != NULL) {
        if (flag_First_in_list != 0) {
            strcat(currentPath, "/");
        }
        flag_First_in_list++;
        strcat(currentPath, current->value);

//        // Check if the subdirectory exists
//        if (access(currentPath, F_OK) == -1) {
//            printf("Directory does not exist: %s\n", currentPath);
//            return;
//        }

        current = current->next;
    }

    // The last segment in the path list is assumed to be a file
    // Append the last segment to the current path
    if (current != NULL) {
        strcat(currentPath, current->value);
    }

    // Check if the file exists locally
    if (access(currentPath, F_OK) == -1) {
        printf("File does not exist locally: %s\n", currentPath);

        // Use the new function to send HTTP request and receive response
        sendHTTPRequestAndReceiveResponse(hostname, port, filepath);
    } else {
        generateHTTPResponse(currentPath);
        printf("Directory structure and file exist locally: %s\n", currentPath);
        printf("File is given from the local filesystem\n");
    }
}


int main() {
//    const char *url = "http://www.yoyo.com:1234/pub/files/fo1.html";

    const char *url = "http://www.josephwcarrillo.com";



    // Split the URL
    splitURL(url);

    // Print original components
    printf("Original Components:\n");
    printf("Protocol: %s\n", protocol);
    printf("Hostname: %s\n", hostname);
    printf("Port: %s (Length: %zu)\n", port, strlen(port));
    printf("Filepath: %s\n", filepath);

    // Display stored path segments
    printPathList();

    // Check directory existence
    checkDirectoryExistence(hostname, pathList);

    // Free allocated memory
    free(protocol);
    free(hostname);
    free(port);
    free(filepath);

    // Free memory allocated for the linked list
    freePathList();

    return 0;
}
