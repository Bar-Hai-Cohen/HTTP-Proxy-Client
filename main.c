#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>

// Define a structure for a node in the linked list
typedef struct Node {
    char *value;
    struct Node *next;
} Node;

// Declare global variable for the linked list
Node *pathList;

// Declare global variables for storing URL components
char *protocol, *hostname, *port, *filepath;

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
//    strcat(currentPath, "/");
    strcat(currentPath, hostname);

    // Check if the top-level directory exists
    if (access(currentPath, F_OK) == -1) {
        printf("Directory does not exist: %s\n", currentPath);
        return;
    }

    int flag_First_in_list = 0;  // check if is the first in the liked-list , if yes ,do not add another /

    // Iterate through the linked list and check subdirectories
    Node *current = pathList;
    while (current != NULL) {
        if (flag_First_in_list != 0) {
            strcat(currentPath, "/");
        }
        flag_First_in_list++;
        strcat(currentPath, current->value);

        // Check if the subdirectory exists
        if (access(currentPath, F_OK) == -1) {
            printf("Directory does not exist: %s\n", currentPath);
            return;
        }

        current = current->next;
    }

    // The last segment in the path list is assumed to be a file
    // Append the last segment to the current path
    if (current != NULL) {
        strcat(currentPath, current->value);
    }

    // Check if the file exists
    if (access(currentPath, F_OK) == -1) {
        printf("File does not exist: %s\n", currentPath);
    } else {
        printf("Directory structure and file exist: %s\n", currentPath);
    }
}

int main() {
    const char *url = "http://www.yoyo.com:1234/pub/files/foo.html";

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
