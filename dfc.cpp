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
#include <openssl/md5.h>

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
#define NUMTAGS 6
std::string tags[NUMTAGS];// = {"id","pw","flag", "file","segment", "msg"}

typedef struct Tuple{
  int p1;
  int p2;
}Tuple;

typedef struct ServerMap{
   Tuple maps[4][4];
}ServerMap;

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
Tuple serverMaps[4][4];

void initServerMaps(){
  Tuple t1 = {0,1};
  Tuple t2 = {1,2};
  Tuple t3 = {2,3};
  Tuple t4 = {3,0};
  serverMaps[0][0] = t1;
  serverMaps[0][1] = t2;
  serverMaps[0][2] = t3;
  serverMaps[0][3] = t4;
  serverMaps[1][0] = t4;
  serverMaps[1][1] = t1;
  serverMaps[1][2] = t2;
  serverMaps[1][3] = t3;
  serverMaps[2][0] = t3;
  serverMaps[2][1] = t4;
  serverMaps[2][2] = t2;
  serverMaps[2][3] = t1;
  serverMaps[3][0] = t2;
  serverMaps[3][1] = t3;
  serverMaps[3][2] = t4;
  serverMaps[3][3] = t1;
}

//returns element enclosed within tag if it exists, "NOTFOUND" if it does not
std::string searchXML(std::string tag, std::string msg){
  std::string s = "<"+tag+">";
  std::string e = "</"+tag+">";
  int start = msg.find(s);
  int end = msg.find(e);
  if(start == std::string::npos || end == std::string::npos) return "NOTFOUND";
  else return msg.substr(start+s.length(), end - (start + s.length()));
}

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

std::string buildMsg(std::string flag, std::string msg){
  return "<all><id>"+user.id+"</id><pw>"+user.pw+"</pw><flag>"+flag+"</flag><msg>"+msg+"</msg></all>";
}

//For when msg is a file segment: flag must be "P"
std::string buildMsg(std::string flag, std::string msg, std::string segment, int msgLen, std::string filename){
  std::string msgTrunc = msg.substr(0, msgLen);
  return "<all><id>"+user.id+"</id><pw>"+user.pw+"</pw><flag>"+flag+"</flag><file>"+filename+"</file><segment>"+segment+"</segment><msg>"+msgTrunc+"</msg></all>";
}

int testSend() {
    for(int i = 0; i < 4; i++) {
        std::string tester = buildMsg("A", "testing");
        std::cout<<tester<<std::endl;
        if((sendto(clientFDvec[i], tester.c_str(), tester.length(), 0, (struct sockaddr*)remoteSocks[i], sizeof(*(remoteSocks[i])))) < 0) {
            printf("Error in sendto on socket %d", i);
            perror("");
        }
    }
    return 0;
}

int doList() {
  for(int i = 0; i < 4; i++) {
      std::string msg = buildMsg("L", "");
      char buffer[1024];
      int recSize;
      unsigned int sockLen = sizeof(remoteSocks[0]);

      if((sendto(clientFDvec[i], msg.c_str(), msg.length(), 0, (struct sockaddr*)remoteSocks[i], sizeof(*(remoteSocks[i])))) < 0) {
          printf("Error in sendto on socket %d", i);
          perror("");
      }
      while((recSize = recvfrom(clientFDvec[i], buffer, 1024, 0, (struct sockaddr*)remoteSocks[i], (socklen_t*)&sockLen)) > 0) {
        std::cout<<"Buffer is: "<<buffer<<std::endl;
      }
      if(recSize < 0) perror("Recvfrom error"); //recSize will be 0 if no error
  }
  return 0;
}

std::string findCorrectMsg(std::string fullMsg) {
  std::vector<std::string> msgs;
  int cont;
  while((cont = fullMsg.find("</all>")) != std::string::npos) {
    std::string msg = searchXML("all", fullMsg);
    msgs.push_back(msg);
    fullMsg = fullMsg.substr(cont+6);
  }
  //Message doesn't contain any flags
  if(msgs.size() == 0) {
    std::cout<<"ERROR: Server sent malformed response to GET request, cannot reconstruct file."<<std::endl;
    return "NOTFOUND";
  }
  for(int i = 0; i < msgs.size(); i++){
    //Look for the message within a message that's a response to GET.
    std::string response = msgs[i];
    std::string flag = searchXML("flag", response);
    //Found it!
    if(flag == "G") {

      std::cout<<"\t\tFound it!Seg is "<<searchXML("segment",msgs[i])<<" and msg is: "<<searchXML("msg", msgs[i])<<std::endl;
      return msgs[i];
      continue;
    } else {
      //Message doesn't contain any flag tags
      if(flag == "NOTFOUND") {
        //std::cout<<"\t\tERROR: Server sent malformed response."<<std::endl;
        continue;
      }
      //Message is an error in response to GET
      if(flag == "E") {
        std::cout<<"\t\tServer sent error: "<<std::endl;
        return msgs[i];
      }
      //std::cout<<"\t\tDiscarding message with incorrect flag: "<<searchXML("msg",msgs[i])<<std::endl;
    }
  }
  return "NOTFOUND";
}

int doGet(std::string filename) {
  std::string pieces[4];
  std::string names[4];
  std::string response = "";
  unsigned int sockLen = sizeof(remoteSocks[0]);
  FILE* local;
  for(int i = 0; i < 4; i++){
    names[i] = "."+filename+"."+std::to_string(i+1);
    pieces[i] = "empty";
  }
  //Iterate over pieces
  for(int i = 0; i < 4; i++) {
    std::cout<<"\tSending and receiving to and from server "<<i<<std::endl;
    //std::string flag, std::string msg, std::string segment, int msgLen, std::string filename
    std::string msg = buildMsg("G", "none", "-1", 4, filename);
    char buffer[1024];
    int recSize;
    std::string tagVals[NUMTAGS];

    if((send(clientFDvec[i], msg.c_str(), msg.length(), 0)) < 0) {
        printf("Error in sendto in doGet");
        perror("");
    }
    bool gotResp = false;
    std::string getResp;
    for(int j = 0; j < 2; j++){
    while(!gotResp){
      //Get the full message in the socket, which might be many messages concatenated together.
      response = "";
      do {
        recSize = recvfrom(clientFDvec[i], buffer, 1024, 0, (struct sockaddr*)remoteSocks[i], (socklen_t*)&sockLen);
        std::string curResp(buffer);
        response = response + curResp;
      }while(recSize == 1024);
      //std::cout<<"\t\tFull message is: "<<response<<std::endl;
      getResp = findCorrectMsg(response);
      if(getResp != "NOTFOUND") {
        gotResp = true;
        if((send(clientFDvec[i], "boop", 4, 0)) < 0) {
            printf("Error in sendto in doGet on boop");
            perror("");
        }
      }
      std::cout<<"\tLooping through calling recv until a GET response from "<<i<<" arrives"<<std::endl;
    }

    //Tags are id, pw, flag, file, segment, msg
    int segIdx;
    for(int i = 0; i < NUMTAGS; i++) {
      tagVals[i] = searchXML(tags[i], getResp);
    }
    segIdx = atoi(tagVals[4].c_str())-1;
    if(pieces[segIdx] == "empty") {
      pieces[segIdx] = tagVals[5];
    }
  }
}
  //Check that all pieces were found
  for(int i = 0; i < 4; i++) {
    if(pieces[i] == "empty") {
      std::cout<<"Cannot reconstruct file: segment "<<i+1<<" missing."<<std::endl;
      return -1;
    }
  }
  std::string fullMsg = pieces[0]+pieces[1]+pieces[2]+pieces[3]+ "Blahh";
  local = fopen(filename.c_str(), "wb");
  if(fwrite(fullMsg.c_str(), sizeof(char), fullMsg.length(), local) != fullMsg.length()) {
    std::cout<<"ERROR: fwrite in doGet()"<<std::endl;
  }
  return 0;
}

int doPut(std::string filename) {
  std::cout<<"Put"<<std::endl;
  FILE* file = fopen(filename.c_str(), "rb");
  if(file == NULL){
    std::cout<<"Cannot open file "<<filename<<", please check that it exists."<<std::endl;
    return -1;
  }
  int fileSize;
  int pieceSize; //last piece will be bigger by up to 3 bytes
  int n1, n2, n3, n4, nFull;
  char* buf1;
  char* buf2;
  char* buf3;
  char* buf4;
  std::string msgs[4];
  unsigned char digest[32];
  char* buf;
  long long int md5hash;
  int hash;

  //Split file into four pieces
  fseek(file, 0, SEEK_END);
  fileSize = ftell(file);
  rewind(file);
  pieceSize = fileSize/4;
  //Determine which servers to upload what to
  //Construct message with appropriate flags
  //Send messages to servers

  buf1 = new char[pieceSize+1];
  buf2 = new char[pieceSize+1];
  buf3 = new char[pieceSize+1];
  buf4 = new char[(fileSize - pieceSize*3)+1];
  bzero(buf1, pieceSize+1);
  bzero(buf2, pieceSize+1);
  bzero(buf3, pieceSize+1);
  bzero(buf4, (fileSize - pieceSize*3)+1);

  if((n1 = fread(buf1, sizeof(char), pieceSize, file))<= 0) {
    std::cout<<"Read error in doPut segment 1"<<std::endl;
    return -1;
  }
  if((n2 = fread(buf2, sizeof(char), pieceSize, file))<= 0) {
    std::cout<<"Read error in doPut segment 2"<<std::endl;
    return -1;
  }
  if((n3 = fread(buf3, sizeof(char), pieceSize, file))<= 0) {
    std::cout<<"Read error in doPut segment 3"<<std::endl;
    return -1;
  }
  if((n4 = fread(buf4, sizeof(char), (fileSize - pieceSize*3), file))<= 0) {
    std::cout<<"Read error in doPut segment 4"<<std::endl;
    return -1;
  }
  buf = new char[fileSize+1];
  bzero(buf, fileSize+1);
  rewind(file);
  if((nFull = fread(buf, sizeof(char), fileSize, file))<= 0) {
    std::cout<<"Read error in doPut full file"<<std::endl;
    return -1;
  }

  MD5((unsigned char*)buf, fileSize, digest);
  const char* newDigest = (const char*)digest;
  md5hash = strtoll(newDigest, NULL, 16);
  hash = md5hash % 4;

  std::string seg1(buf1);
  std::string seg2(buf2);
  std::string seg3(buf3);
  std::string seg4(buf4);

  msgs[0] = buildMsg("P", seg1, "1", pieceSize, filename);
  msgs[1] = buildMsg("P", seg2, "2", pieceSize, filename);
  msgs[2] = buildMsg("P", seg3, "3", pieceSize, filename);
  msgs[3] = buildMsg("P", seg4, "4", fileSize - pieceSize*3, filename);

  for(int i = 0; i < 4; i++) {
    //pthread_mutex_lock(&client_sock_lock);
    //serverMaps[hash][i] contains tuple that says which segments to send to server i
    int firstSeg = serverMaps[hash][i].p1;
    int secSeg = serverMaps[hash][i].p2;
    int sent = send(clientFDvec[i], msgs[firstSeg].c_str(), msgs[firstSeg].length(), 0);
    sent = send(clientFDvec[i], msgs[secSeg].c_str(), msgs[secSeg].length(), 0);
    //pthread_mutex_unlock(&client_sock_lock);
  }

  fclose(file);
  return 0;
}

//Pre-condition: User struct user is initialized
int authenticate() {
  std::cout<<"Checking credentials..."<<std::endl;
  for(int i = 0; i < 4; i++) {
      std::string credentials = buildMsg("A", "blargh");
      char buffer[1024];
      int recSize;
      unsigned int sockLen = sizeof(remoteSocks[0]);

      if((sendto(clientFDvec[i], credentials.c_str(), credentials.length(), 0, (struct sockaddr*)remoteSocks[i], sizeof(*(remoteSocks[i])))) < 0) {
          printf("Error in sendto on socket %d", i);
          perror("");
      }
      do {
        recSize = recvfrom(clientFDvec[i], buffer, 1024, 0, (struct sockaddr*)remoteSocks[i], (socklen_t*)&sockLen);
        std::string response(buffer);
        std::string msg = searchXML("msg", response);
        if(msg != "VALID") {
          std::cout<<"Fatal error: User credentials are not valid on server #"<<i+1<<" msg is "<<response<<std::endl;
          exit(1);
        }
        else std::cout<<"Credentials accepted by server #"<<i+1<<std::endl;
      }while(recSize == 1024);
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
    tags[0] = "id";
    tags[1] = "pw";
    tags[2] = "flag";
    tags[3] = "file";
    tags[4] = "segment";
    tags[5] = "msg";

    //Parse config file
    parseConfig(argv[1]);

    //Init client sockets, one for each server I need to talk to
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

    for(int i = 0; i < 4; i++) {
      //Connect client socket to remote port. This means client sends to that port, but it listens on whichever one the kernel assigns it.
      struct sockaddr_in * curSockAddr = remoteSocks[i];
      if(connect(clientFDvec[i], (struct sockaddr *)curSockAddr, sizeof(*curSockAddr)) < 0) {
         std::cout<<"Connecting to "<<curSockAddr->sin_port<<" at "<<curSockAddr->sin_addr.s_addr<<" 1: "<<htons(10001)<<" 2: "<<htons(10002)<<" 3: "<<htons(10003)<<" 4: "<<htons(10004)<<std::endl;
         perror("connection failed error");
         return 1;
      }
    }

    initServerMaps();
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
          if(in == "") break;
          //std::cout << "Substring: " << in << std::endl;
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
            doGet(args[0]);
        } else if(command == "PUT") {
            std::cout<<"File name is: "<<args[0]<<std::endl;
            doPut(args[0]);
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
