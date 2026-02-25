#ifndef SERVER_H
#define SERVER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <print>
#include <cstring>
using namespace std;

int server_socket()
{
    int sockfd;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        println(stderr, "Error opening socket");
        return 1;
    }
    println("Socket created successfully");

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // Bind to any available network interface
    server_addr.sin_port = htons(8080);       // Convert port number to network byte order

    if (bind(sockfd, (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0)
    {
        perror("Bind failed");
        close(sockfd);
        return 1;
    }
    println("Socket successfully bound to port 8080");

    if (listen(sockfd, SOMAXCONN) < 0)
    {
        perror("Listen failed");
        close(sockfd);
        return 1;
    }

    close(sockfd);
    return 0;
}

#endif // SERVER_H