//#include <string.h>

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
    http://www.cplusplus.com/reference/cstring/strtok/
*/
std::vector<struct sockaddr_in*> remoteSocks;
std::vector<int> clientFDvec;

typedef struct Server {
  std::string homeDir;
  std::string addr;
  int port;
}Server;

std::vector<Server> servers;

typedef struct ConfLine {
  std::string lineType;
  std::vector<std::string> lineContents;
}ConfLine;

typedef struct User{
  std::string id;
  std::string pw;
}User;

User user;

int parseConfig(char* conf){
  std::ifstream confFile;
  std::string line;

  confFile.open(conf);
  if (confFile == NULL) {
    std::cout<<"FATAL: No such conf file"<<std::endl;
    exit(1);
  }
  do{
    ConfLine lineStruct = {};
    if(getline(confFile, line) == NULL) break;
    else {
      //std::cout<<"Line is "<<line<<std::endl;
      char* pch;
      int idx = 0;
      char* lineArr = strdup(line.c_str());
      pch = strtok (lineArr," :");
      do {
        //pch = strtok (NULL, " :");
        std::string tokStr = pch;
        //std::cout<<"Pch is: "<<pch<<" ?"<<std::endl;
        if(!idx) {
          lineStruct.lineType = tokStr;
        } else {
          lineStruct.lineContents.push_back(tokStr);
        }
        idx++;
        //sleep(1);
        //std::cout<<"wat?"<<std::endl;
      }while ((pch = strtok(NULL, " :"))!= NULL);
      //std::cout<<"Line type = "<<lineStruct.lineType<<std::endl;
      //std::cout<<"Line contents: "<<std::endl;
      //for(int i = 0; i < lineStruct.lineContents.size(); i++) std::cout<<"\t"<<lineStruct.lineContents[i]<<std::endl;
      if(lineStruct.lineType == "Server") {
        Server s = {};
        s.homeDir = lineStruct.lineContents[0];
        s.addr = lineStruct.lineContents[1];
        s.port = atoi(strdup(lineStruct.lineContents[2].c_str()));
        servers.push_back(s);
      } else if (lineStruct.lineType == "Username"){
        user.id = lineStruct.lineContents[0];
      } else if (lineStruct.lineType == "Password") {
        user.pw = lineStruct.lineContents[0];
      }

    }
  }while(!confFile.eof());
  confFile.close();
  return 0;
}


int testSend() {
    for(int i = 0; i < 4; i++) {
        std::string tester = "Testing " + i; //does pointer math not concatenation, this isn't Javascript you numpty
        std::cout<<tester<<std::endl;
        //Connect client socket to remote port. This means client sends to that port, but it listens on whichever one the kernel assigns it.
        struct sockaddr_in * curSockAddr = remoteSocks[i];
        if(connect(clientFDvec[i], (struct sockaddr *)curSockAddr, sizeof(*curSockAddr)) < 0) {
           std::cout<<"Connecting to "<<curSockAddr->sin_port<<" at "<<curSockAddr->sin_addr.s_addr<<" 1: "<<htons(10001)<<" 2: "<<htons(10002)<<" 3: "<<htons(10003)<<" 4: "<<htons(10004)<<std::endl;
           perror("connection failed error");
           return 1;
        }
        if((sendto(clientFDvec[i], tester.c_str(), tester.length(), 0, (struct sockaddr*)remoteSocks[i], sizeof(*(remoteSocks[i])))) < 0) {
            printf("Error in sendto on socket %d", i);
            perror("");
        }
    }
    return 0;
}

int doList() {
  for(int i = 0; i < 4; i++) {
      std::string msgType = "L";
      char buffer[1024];
      int recSize;
      unsigned int sockLen = sizeof(remoteSocks[0]);
      //Connect client socket to remote port. This means client sends to that port, but it listens on whichever one the kernel assigns it.
      struct sockaddr_in * curSockAddr = remoteSocks[i];
      if(connect(clientFDvec[i], (struct sockaddr *)curSockAddr, sizeof(*curSockAddr)) < 0) {
         std::cout<<"Connecting to "<<curSockAddr->sin_port<<" at "<<curSockAddr->sin_addr.s_addr<<" 1: "<<htons(10001)<<" 2: "<<htons(10002)<<" 3: "<<htons(10003)<<" 4: "<<htons(10004)<<std::endl;
         perror("connection failed error");
         return 1;
      }
      if((sendto(clientFDvec[i], msgType.c_str(), msgType.length(), 0, (struct sockaddr*)remoteSocks[i], sizeof(*(remoteSocks[i])))) < 0) {
          printf("Error in sendto on socket %d", i);
          perror("");
      }
      while((recSize = recvfrom(clientFDvec[i], buffer, 1024, 0, (struct sockaddr*)remoteSocks[i], (socklen_t*)&sockLen)) > 0) {
        std::cout<<"Buffer is: "<<buffer<<std::endl;
      }
      if(recSize < 0) perror("Recvfrom error"); //will be 0 if no error
  }
  return 0;
}

int doGet() {
    return 0;
}

int doPut() {
    return 0;
}

//Pre-condition: User struct user is initialized
int authenticate() {
  std::cout<<"Checking credentials..."<<std::endl;
  for(int i = 0; i < 4; i++) {
      std::string credentials = "A" + user.id + " " + user.pw;
      char buffer[1024];
      int recSize;
      unsigned int sockLen = sizeof(remoteSocks[0]);
      //Connect client socket to remote port. This means client sends to that port, but it listens on whichever one the kernel assigns it.
      struct sockaddr_in * curSockAddr = remoteSocks[i];
      if(connect(clientFDvec[i], (struct sockaddr *)curSockAddr, sizeof(*curSockAddr)) < 0) {
         std::cout<<"Connecting to "<<curSockAddr->sin_port<<" at "<<curSockAddr->sin_addr.s_addr<<" 1: "<<htons(10001)<<" 2: "<<htons(10002)<<" 3: "<<htons(10003)<<" 4: "<<htons(10004)<<std::endl;
         perror("connection failed error");
         return 1;
      }
      if((sendto(clientFDvec[i], credentials.c_str(), credentials.length(), 0, (struct sockaddr*)remoteSocks[i], sizeof(*(remoteSocks[i])))) < 0) {
          printf("Error in sendto on socket %d", i);
          perror("");
      }
      while((recSize = recvfrom(clientFDvec[i], buffer, 1024, 0, (struct sockaddr*)remoteSocks[i], (socklen_t*)&sockLen)) > 0) {
        std::string response(buffer);
        if(response != "VALID") {
          std::cout<<"Fatal error: User credentials are not valid on server #"<<i<<std::endl;
          exit(1);
        }
        else std::cout<<"Credentials accepted by server #"<<i<<endl;
      }
      if(recSize < 0) perror("Recvfrom error"); //will be 0 if no error
  }
  return 0;
}

int main(int argc, char* argv[]){

    int sockFD1=0;
    int sockFD2=0;
    int sockFD3=0;
    int sockFD4=0;
    int clientFD1 = 0;
    int clientFD2 = 0;
    int clientFD3 = 0;
    int clientFD4 = 0;
    int n = 0;
    struct sockaddr_in servsock1, servsock2, servsock3, servsock4, clientsock;
    char buffer[BUFFER_SIZE];
    std::string locHost = "127.0.0.1";
    std::string conf = "dfc.conf";
    char* confArr = strdup(conf.c_str());
    //for(int i = 0; i < locHost.length(); i++) std::cout<<locHost[i]<<std::endl;

    //Initialize everything
    memset(buffer, 0, sizeof(buffer));

    //Parse config file
    parseConfig(argv[1]);

    //Init client socket
    if((clientFD1 = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Error initializing client socket 1\n");
        return 1;
    }
    if((clientFD2 = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Error initializing client socket 2\n");
        return 1;
    }
    if((clientFD3 = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Error initializing client socket 3\n");
        return 1;
    }
    if((clientFD4 = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Error initializing client socket 4\n");
        return 1;
    }
    clientFDvec.push_back(clientFD1);
    clientFDvec.push_back(clientFD2);
    clientFDvec.push_back(clientFD3);
    clientFDvec.push_back(clientFD4);


    //Init all the server sockets' info structs
    servsock1.sin_family = AF_INET;
    servsock1.sin_port= htons(servers[0].port);
    if(inet_pton(AF_INET, locHost.c_str(), &servsock1.sin_addr)<=0) {
        printf("inet_pton error\n");
        return 1;
    }
    servsock2.sin_family = AF_INET;
    servsock2.sin_port= htons(servers[1].port);
    if(inet_pton(AF_INET, locHost.c_str(), &servsock2.sin_addr)<=0) {
        printf("inet_pton error\n");
        return 1;
    }
    servsock3.sin_family = AF_INET;
    servsock3.sin_port= htons(servers[2].port);
    if(inet_pton(AF_INET, locHost.c_str(), &servsock3.sin_addr)<=0) {
        printf("inet_pton error\n");
        return 1;
    }
    servsock4.sin_family = AF_INET;
    servsock4.sin_port= htons(servers[3].port);
    if(inet_pton(AF_INET, locHost.c_str(), &servsock4.sin_addr)<=0) {
        printf("inet_pton error\n");
        return 1;
    }

    remoteSocks.push_back(&servsock1);
    remoteSocks.push_back(&servsock2);
    remoteSocks.push_back(&servsock3);
    remoteSocks.push_back(&servsock4);

    authenticate();

    while(1) {
        printf("Please select from the following options: \n\tLIST\n\tGET\n\tPUT\n\tquit\n");
        //fgets(fullCommand, 103, stdin);
        //sscanf(fullCommand, "%s %s", command, filename);

        std::string command;
        int idx = 0;
        std::string line;
        std::vector<std::string> args;
        getline(std::cin, line);
        std::istringstream iss(line);
        while(iss) {
          std::string in;
          iss >> in;
          std::cout << "Substring: " << in << std::endl;
          if(!idx) command = in;
          else {
              args.push_back(in);
          }
          idx++;
        }

        std::cout<<"command is "<<command<<std::endl;
        if(command == "LIST") {
            doList();
        } else if (command == "GET") {
            doGet();
        } else if(command == "PUT") {
            doPut();
        } else if (command == "TEST") {
            std::cout<<"Test send"<<std::endl;
            testSend();
        }else if (command == "quit"){
            exit(0);
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
