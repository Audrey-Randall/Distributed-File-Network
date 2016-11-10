#include <string.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <stdlib.h>
#include <memory.h>
#include <errno.h>

//C++
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
//using namespace std;

#define BUFFER_SIZE 1024


/* Citations:
    http://www.thegeekstuff.com/2011/12/c-socket-programming/?utm_source=feedburner
    http://stackoverflow.com/questions/27014955/socket-connect-vs-bind
    Linux man pages
*/
std::vector<struct sockaddr_in*> remoteSocks;

int testSend(int clientFD) {
    for(int i = 0; i < 4; i++) {
        std::string tester = "Testing " + i;
        //Connect client socket to remote port. This means client sends to that port, but it listens on whichever one the kernel assigns it.
        struct sockaddr_in * curSockAddr = remoteSocks[i];
        if(connect(clientFD, (struct sockaddr *)curSockAddr, sizeof(*curSockAddr)) < 0) {
           printf("connection failed error\n");
           return 1;
        }
        if((sendto(clientFD, tester.c_str(), tester.length(), 0, (struct sockaddr*)remoteSocks[i], sizeof(*(remoteSocks[i])))) < 0) {
            printf("Error in sendto on socket %d", i);
            perror("");
        }
    }
    return 0;
}

int doList() {
    return 0;
}

int doGet() {
    return 0;
}

int doPut() {
    return 0;
}


int main(int argc, char* argv[]){

    int sockFD1=0;
    int sockFD2=0;
    int sockFD3=0;
    int sockFD4=0;
    int clientFD;
    int n = 0;
    struct sockaddr_in servsock1, servsock2, servsock3, servsock4, clientsock;
    char buffer[BUFFER_SIZE];

    //Initialize everything
    memset(buffer, 0, sizeof(buffer));

    //Init client socket
    if((clientFD = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Error initializing client socket \n");
        return 1;
    }

    //Init all the server sockets' info structs
    servsock1.sin_family = AF_INET;
    servsock1.sin_port= htons(10001);
    if(inet_pton(AF_INET, argv[1], &servsock1.sin_addr)<=0) {
        printf("inet_pton error\n");
        return 1;
    }
    servsock2.sin_family = AF_INET;
    servsock2.sin_port= htons(10001);
    if(inet_pton(AF_INET, argv[1], &servsock2.sin_addr)<=0) {
        printf("inet_pton error\n");
        return 1;
    }
    servsock3.sin_family = AF_INET;
    servsock3.sin_port= htons(10001);
    if(inet_pton(AF_INET, argv[1], &servsock3.sin_addr)<=0) {
        printf("inet_pton error\n");
        return 1;
    }
    servsock4.sin_family = AF_INET;
    servsock4.sin_port= htons(10001);
    if(inet_pton(AF_INET, argv[1], &servsock4.sin_addr)<=0) {
        printf("inet_pton error\n");
        return 1;
    }

    remoteSocks.push_back(&servsock1);
    remoteSocks.push_back(&servsock2);
    remoteSocks.push_back(&servsock3);
    remoteSocks.push_back(&servsock4);

    while(1) {
        printf("Please select from the following options: \n\tLIST\n\tGET\n\tPUT\n");
        //fgets(fullCommand, 103, stdin);
        //sscanf(fullCommand, "%s %s", command, filename);
        std::string in;
        std::string command;
        int idx = 0;
        std::vector<std::string> args;
        while(getline(std::cin, in)) {
            if(!idx) command = in;
            else {
                args.push_back(in);
            }
        }
        if(command == "LIST") {
            doList();
        } else if (command == "GET") {
            doGet();
        } else if(command == "PUT") {
            doPut();
        } else if (command == "TEST") {
            testSend(clientFD);
        }else {
            std::cout<<"Unknown command "<<command<<std::endl;
            continue;
        }



        /* This code will get the server's response once it's been sent by reading from the socket file, and print it to stdout.
        int buflen;
        while((buflen = recvfrom(socketFD, buffer, sizeof(buffer), 0, (struct sockaddr *)&remoteSock, &remoteSockLen)) > 0) {
            //Handle server response
        }*/
    }
    return 0;
}
