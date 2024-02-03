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
#include <errno.h>

// Define a structure for a node in the linked list
typedef struct Node {
    char *value;
    struct Node *next;
} Node;

typedef uint16_t in_port_t;
struct hostent *server_info = NULL;
int lenUrl;
int saveLocally=1;

// Declare global variable for the linked list
Node *pathList;

// Declare global variables for storing URL components
char *protocol, *hostname, *port, *filepath,*currentPath ;

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

// Function to build a path from a hostname and linked list of path segments
char *buildPath(const char *hostname, Node *pathList) {
    currentPath = malloc(lenUrl);
    if (currentPath == NULL) {
        // Free allocated memory
        free(currentPath);
        free(hostname);
        free(port);
        free(filepath);
        // Free memory allocated for the linked list
        freePathList();
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }

    memset(currentPath, 0, lenUrl);
    strcat(currentPath, hostname);

    int flag_First_in_list = 0;

    // Iterate through the linked list and check subdirectories
    Node *current = pathList;
    while (current != NULL) {
        if (flag_First_in_list != 0) {
            // Use platform-independent path separator
            strcat(currentPath, "/");
        }
        flag_First_in_list++;

        // Check the combined length before concatenating
        if (strlen(currentPath) + strlen(current->value) < lenUrl) {
            strcat(currentPath, current->value);
        } else {
            // Free allocated memory
            free(hostname);
            free(port);
            free(filepath);
            // Free memory allocated for the linked list
            freePathList();
            fprintf(stderr, "Path length exceeds maximum limit.\n");
            exit(EXIT_FAILURE);
        }

        current = current->next;
    }

    // The last segment in the path list is assumed to be a file
    // Append the last segment to the current path
    if (current != NULL) {
        // Check the combined length before concatenating
        if (strlen(currentPath) + strlen(current->value) < lenUrl) {
            strcat(currentPath, current->value);
        } else {
            fprintf(stderr, "Path length exceeds maximum limit.\n");
            exit(EXIT_FAILURE);
        }
    }

    return currentPath;
}

/**
 * @brief Opens the URL in the default web browser.
 *
 * @param url The URL to be opened in the browser.
 */
void openInBrowser(const char *url) {
    // Use a system command to open the default web browser
    char command[256];
    snprintf(command, sizeof(command), "xdg-open http://%s", url);

    if (system(command) == -1) {
        perror("Error opening in the browser");
        exit(EXIT_FAILURE);
    }
}


/**
 * @brief Sends an HTTP request to the server and receives the response.
 *
 * @param hostname The hostname of the server.
 * @param port The port number for the server.
 * @param filepath The filepath of the requested resource.
 */
void sendHTTPRequestAndReceiveResponse(const char *hostname, const char *port, const char *filepath) {
    // Construct HTTP request
    char request[1024];  // Adjust the size as needed
    sprintf(request, "GET %s HTTP/1.0\r\nHost: %s\r\n\r\n", filepath, hostname);

    in_port_t port1 = atoi(port);

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
    memset(&serverAddr, 0, sizeof(struct sockaddr_in));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port1);
    // Set the server address using the first IP address from the hostent structure
    serverAddr.sin_addr.s_addr = ((struct in_addr *) server_info->h_addr)->s_addr;

    // Connect to the server
    if (connect(sockfd, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) == -1) {
        perror("Connection to server failed");
        fprintf(stderr, "Error code: %d\n", errno);  // Add this line to print the error code
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Send the HTTP request to the server
    if (send(sockfd, request, strlen(request), 0) == -1) {
        perror("Failed to send HTTP request");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    int headerRead = 0;  // Flag to indicate whether the header has been fully read
    int bytesRead = 0;
    int totalBytesRead = 0;
    unsigned char response[8192];
    int contentLength = 0;
    // Open the file
    FILE *file = NULL;
    char *currentPath = NULL;

    // Loop to receive the response
    while ((bytesRead = recv(sockfd, response, sizeof(response) - 1, 0)) > 0) {
        response[bytesRead] = '\0';



        // If the header has not been fully read, check for the end of the header
        if (!headerRead) {
            char *headerEnd = strstr(response, "\r\n\r\n");
            if (headerEnd != NULL) {
                // The header is fully read
                headerRead = 1;

                // Extract the status and content length from the header
                char *statusStart = response + 9;  // Assuming "HTTP/1.x " is at the beginning
                int statusCode = atoi(statusStart);

                if (statusCode == 404) {
                    printf("File does not exist (HTTP 404 Not Found)\n");
                    close(sockfd);
                    // Free allocated memory
                    free(hostname);
                    free(port);
                    free(filepath);
                    // Free memory allocated for the linked list
                    freePathList();
                    exit(EXIT_FAILURE);
                } else if (statusCode == 200) {
                    char *contentLengthStart = strstr(response, "Content-Length: ");
                    if (contentLengthStart != NULL) {
                        contentLengthStart += 16;  // Move to the beginning of the content length value
                        contentLength = atoi(contentLengthStart);
                        printf("Content Length: %zu\n", contentLength);

                        // Open the file
                        file = NULL;
                        if (strcmp(filepath, "/") == 0) {
                            file = fopen(hostname, "w");
                            printf("File saved locally: %s\n", hostname);
                            currentPath = hostname;
                        } else {
                            currentPath = buildPath(hostname, pathList);
                            file = fopen(currentPath, "w");
                            printf("File saved locally: %s\n", currentPath);
                        }

                        if (file == NULL) {
                            perror("Error opening file for writing");
                            close(sockfd);
                            exit(EXIT_FAILURE);
                        }
                    }
                }
            }
        }

        // Write everything received to the file
        fwrite(response, 1, bytesRead, file);

        // Update total response bytes
        totalBytesRead += bytesRead;

        // If the content length is known and reached, break the loop
        if (contentLength > 0 && totalBytesRead >= contentLength) {
            break;
        }

        // Print everything received to the screen
        printf("%s", response);
    }

    char responseHeader[1024];  // Adjust the size as needed
    sprintf(responseHeader, "HTTP/1.0 200 OK\r\nContent-Length: %ld\r\n\r\n", contentLength);

    // Print total response bytes
    printf("\nTotal response bytes: %zu\n", contentLength + strlen(responseHeader));

    printf("File saved locally: %s\n", currentPath);


    fclose(file);
    close(sockfd);
    if (saveLocally == 1) {
        openInBrowser(currentPath);
    }
    if (strcmp(filepath, "/") == 0) {
        //free(currentPath);
        return;
    }

    //free(currentPath);
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
    const char *protocol = "http://";
    if(strcmp(protocol,"http://")!=0){
        fprintf(stderr, "Invalid URL format: %s\n", url);
        exit(EXIT_FAILURE);
    }
    if (protocolEnd == NULL) {
        fprintf(stderr, "Invalid URL format: %s\n", url);
        exit(EXIT_FAILURE);
    }

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

            // If the port contains non-digit characters, throw an error
            if (!isDigit) {
                fprintf(stderr, "Invalid port format: %.*s\n", (int) (portEnd - portStart), portStart);
                exit(EXIT_FAILURE);
            }

            // Extract port
            port = malloc(portEnd - portStart);
            strncpy(port, portStart + 1, portEnd - portStart - 1);
            port[portEnd - portStart - 1] = '\0';

            // Extract filepath
            filepath = malloc(strlen(portEnd) + 1);
            strcpy(filepath, portEnd);
        } else {
            // No slash after port, throw an error
            fprintf(stderr, "Invalid port format: %s\n", portStart);
            exit(EXIT_FAILURE);
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
                // Colon without a number after it, throw an error
                fprintf(stderr, "Invalid port format: %s\n", colonStart);
                exit(EXIT_FAILURE);
            } else {
                // No port, set to "80"
                port = malloc(3);
                strcpy(port, "80");
            }

            // Set filepath to "/"
            filepath = malloc(2);
            strcpy(filepath, "/");
        }
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

    //TODO:CHEECK IF ALSO PRINT AND ALSO OPEN
    if (saveLocally == 1) {
        openInBrowser(filePath);
    }


    // Close the file
    fclose(file);

    char responseHeader[1024];  // Adjust the size as needed
    sprintf(responseHeader, "HTTP/1.0 200 OK\r\nContent-Length: %ld\r\n\r\n", fileSize);

    // Print total response bytes
    printf("\nTotal response bytes: %zu\n", totalBytes + strlen(responseHeader));
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

    char *currentPath = buildPath(hostname, pathList);

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

//int main(int argc, char *argv[]) {
//    // Check if the correct number of arguments is provided
//    if (argc < 2 || argc > 3) {
//        fprintf(stderr, "Usage: %s <url> [-s]\n", argv[0]);
//        exit(EXIT_FAILURE);
//    }
//
//    // Extract arguments
//    const char *url = argv[1];
//    saveLocally = (argc == 3 && strcmp(argv[2], "-s") == 0); // If saveLocally is 1, it means the "-s" flag is present in the command line, and the program should save the file locally.
//
//    lenUrl = strlen(url);
//
//    // Split the URL
//    splitURL(url);
//
//    // Print original components
//    printf("Original Components:\n");
//    printf("Protocol: %s\n", protocol);
//    printf("Hostname: %s\n", hostname);
//    printf("Port: %s (Length: %zu)\n", port, strlen(port));
//    printf("Filepath: %s\n", filepath);
//
//    // Display stored path segments
//    printPathList();
//
//    // Check directory existence or send HTTP request based on the -s flag
//    if (saveLocally) {
//        checkDirectoryExistence(hostname, pathList);
//    } else {
//        sendHTTPRequestAndReceiveResponse(hostname, port, filepath);
//    }
//
//    // Free allocated memory
//    free(hostname);
//    free(port);
//    free(filepath);
//
//    // Free memory allocated for the linked list
//    freePathList();
//
//    return 0;
//}

int main() {
//    const char *url = "http://www.yoyo.com:1234/pub/files/fo1.html";

    const char *url = "http://www.josephwcarrillo.com";

    lenUrl = strlen(url);

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
    free(hostname);
    free(port);
    free(filepath);
    free(currentPath);

    // Free memory allocated for the linked list
    freePathList();

    return 0;
}