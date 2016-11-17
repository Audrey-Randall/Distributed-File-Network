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
#include <math.h>
#include <queue>
#include <pthread.h>

//C++
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
using namespace std;

typedef struct Ele{
    int client_fd;
    string client_msg;
}Ele;

const int MAX_CHARS_PER_LINE = 512;
const int MAX_TOKENS_PER_LINE = 20;
const char* const DELIMITER = " ";
pthread_t senderThreads[10];
string homeDir;
bool caughtSigInt;
int sock_fd;
int port;
FILE* logStream;
std::string logName;
struct sockaddr_in servSock, client;

pthread_mutex_t q_lock;
pthread_mutex_t client_sock_lock;
queue<Ele*> q;
int threadsActive;
vector<string> indexes;

/* Notes: TCP sockets differ from UDP in that they need a call to listen() and they use recv(), not recvfrom().
    Why are sockaddr_in structs created like that then cast to sockaddr structs?
*/

typedef struct Entry{
    string data;
    int len;
}Entry;

typedef struct User{
  std::string id;
  std::string pw;
}User;

std::vector<User> users;

int parseConfig(char* conf){
  std::ifstream confFile;
  std::string line;

  confFile.open(conf);
  if (confFile == NULL) {
    std::cout<<"FATAL: No such conf file"<<std::endl;
    exit(1);
  }
  do{
    if(getline(confFile, line) == NULL) break;
    else {
      //std::cout<<"Line is "<<line<<std::endl;
      User u;
      char* pch;
      int idx = 0;
      char* lineArr = strdup(line.c_str());
      pch = strtok (lineArr," :");
      do {
        std::string tokStr = pch;
        if(!idx) {
          u.id = tokStr;
        } else {
          u.pw = tokStr;
          users.push_back(u);
        }
        idx++;
      }while ((pch = strtok(NULL, " :"))!= NULL);
    }
  }while(!confFile.eof());

  /*logStream = fopen(logName.c_str(), "a");
  for(int i = 0; i < users.size(); i++){
    fprintf(logStream, "%s: %s\n", users[i].id.c_str(), users[i].pw.c_str());
  }
  fclose(logStream);*/
  confFile.close();
  return 0;
}

bool authenticate(std::string id, std::string pw){
  logStream = fopen(logName.c_str(), "a");
  fprintf(logStream, "id is %s and pw is %s\n", id.c_str(), pw.c_str());
  fclose(logStream);
  for(int i = 0; i < users.size(); i++) {
    if(users[i].id == id) {
      if(users[i].pw == pw) {
        logStream = fopen(logName.c_str(), "a");
        fprintf(logStream, "Credentials verified.\n");
        fclose(logStream);
        return true;
      }
      else {
        logStream = fopen(logName.c_str(), "a");
        fprintf(logStream, "Incorrect password.\n");
        fclose(logStream);
        return true; //should be false
      }
    }
  }
  logStream = fopen(logName.c_str(), "a");
  fprintf(logStream, "User not found.\n");
  fclose(logStream);
  return true; //should be false
}

void respond(int client_fd, std::string msg) {
  //pthread_mutex_lock(&client_sock_lock);
  if((send(client_fd, msg.c_str(), msg.length(), 0)) < 0) {
    //pthread_mutex_unlock(&client_sock_lock);
    logStream = fopen(logName.c_str(), "a");
    fprintf(logStream, "respond() failed!\n");
    fclose(logStream);
    perror("");
  } else {
    logStream = fopen(logName.c_str(), "a");
    fprintf(logStream, "respond() succeeded\n");
    fclose(logStream);
    //pthread_mutex_unlock(&client_sock_lock);
  }
}

void handle_msg(int client_fd, std::string full_msg){
  int space = full_msg.find(' ');
  int credEnd = full_msg.find('#');
  std::string msg = full_msg.substr(credEnd+1);
  char flag = msg[0];
  if(space == std::string::npos || credEnd == std::string::npos) {
    logStream = fopen(logName.c_str(), "a");
    fprintf(logStream, "User authentication failed: Invalid username/password format \"%s\"\n", msg.c_str());
    fclose(logStream);
    respond(client_fd, "INVALID");
  } else {
    std::string id = full_msg.substr(0, space);
    std::string pw = full_msg.substr(space+1, credEnd-space-1);
    if(!authenticate(id, pw)) respond(client_fd, "INVALID");
    respond(client_fd, "VALID");
  }
  switch(flag){
    case 'A':
      break;
    case 'L':
      break;
    case 'G':
      break;
    case 'P':
      break;
    default:
    logStream = fopen(logName.c_str(), "a");
    fprintf(logStream, "Received unknown message.\n");
    fclose(logStream);
    sleep(1);
    exit(1);
  }
}

void catch_sigint(int s){
    cout<<"caught signal "<<s<<", exiting"<<endl;
    caughtSigInt = true;
    for(int i=0; i<10; i++)  {
        pthread_join(senderThreads[i], NULL);
        threadsActive--;
    }
    pthread_mutex_destroy(&q_lock);
    pthread_mutex_destroy(&client_sock_lock);
    close(sock_fd);
    while(!q.empty()){
        cout<<"Stuff left in queue?"<<endl;
        Ele* ele = q.front();
        q.pop();
        delete(ele);
    }
    //cout<<"afsfsafsaff"<<endl;
    //exit(0);
}

void catch_sigpipe(int s) {
  cout<<"Caught SIGPIPE"<<endl;
  sleep(10);
  exit(1);
}

int sendFile(int client_fd, string client_msg, string filepath) {
    FILE* response;
    struct sockaddr_in remoteSock;
    int fileSize = 0;
    int n;
    int i;
    int is404 = 0;
    string header_str;
    string ext;
    string fullPath;
    bool foundIdx = false;

    //Error flags
    int* errCode = new int;
    *errCode = 200;
    bool* invalidMethod = new bool;
    *invalidMethod = false;
    bool invalidURI;
    bool* invalidVersion = new bool;
    *invalidVersion = false;

    //string filepath = ???

    fullPath = homeDir + filepath;

    int idx = filepath.find('.');
    ext = filepath.substr(idx, filepath.length()-1);
    ext = "/"+ext;
    cout<<"ext is "<<endl;
    response = fopen(fullPath.c_str(), "rb");
    if(response == NULL) {
        logStream = fopen(logName.c_str(), "a");
        fprintf(logStream, "Error opening file. Full path: %s\n", fullPath.c_str());
        fclose(logStream);

    }

    fseek(response, 0, SEEK_END);
    fileSize = ftell(response);
    rewind(response);

    char buffer[fileSize];
    bzero(buffer, fileSize);
    while ((n = fread(buffer, sizeof(char), fileSize, response)) > 0) {
        string strBuf(buffer, n);
        string payload = header_str + strBuf;
        cout<<"header str: "<<header_str.length()<<" buffer: "<<strBuf.length()<<" payload length: "<<payload.length()<<endl;
        const char* charLoad = payload.c_str();
        cout<<"File opened, header_str is "<<header_str<<endl;
        if(ext== ".gif"||ext==".png"){
            cout<<"buffer length: "<<sizeof(buffer)<<" and n = "<<n<<endl;
            //int i;
            //for(i = 0; i < n; i++) printf("%c", buffer[i]);
        }
        pthread_mutex_lock(&client_sock_lock);
        int sent = send(client_fd, charLoad, payload.length(), 0);
        pthread_mutex_unlock(&client_sock_lock);
        bzero(buffer, sizeof(buffer));
    }
    if (n < 0) printf("Read error\n");
    fclose(response);
    return 0;
}

void *crawlQueue(void *payload){
    //pop Ele from queue if queue isn't empty
    while(!caughtSigInt) {
        pthread_mutex_lock(&q_lock);
        int success = 0;
        Ele* ele = new Ele;
        if(!q.empty()) {
            success = 1;
            ele = q.front();
            q.pop();
        }
        pthread_mutex_unlock(&q_lock);
        if(success) {
            //sendFile(ele->client_fd, ele->client_msg);
            //std::cout<<"Received message "<<ele->client_msg<<std::endl;
            logStream = fopen(logName.c_str(), "a");
            fprintf(logStream, "Received message %s\n", ele->client_msg.c_str());
            fclose(logStream);
            handle_msg(ele->client_fd, ele->client_msg);
        } else{
            //if queue was empty wait and check again
            int sleep_time = rand()%101;
            usleep(sleep_time);
        }
        delete(ele);
    }
}

void init(){
    caughtSigInt = false;
    threadsActive = 0;
}

int main(int argc, char* argv[]) {
    struct sockaddr_in client;
    struct sigaction sigIntHandler, sigPipeHandler;
    int client_fd, read_size;
    socklen_t sockaddr_len;
    char client_req[2000];
    std::string locLogName = "log.txt";
    std::string dir(argv[2]);
    std::string confName = "dfs.conf";

    //Log file name
    logName = dir + "/" + locLogName;

    //Clear log file
    logStream = fopen(logName.c_str(), "w");
    if(logStream == NULL) {
      printf("HORRIBLE ERRORS: log name is %s\n", logName.c_str());
      exit(1);
    }
    fclose(logStream);

    //parse configuration
    parseConfig(strdup(confName.c_str()));

    //Set up signal handler for SIGINT and SIGPIPE
    sigIntHandler.sa_handler = catch_sigint;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, NULL);

    sigPipeHandler.sa_handler = catch_sigpipe;
    sigemptyset(&sigPipeHandler.sa_mask);
    sigPipeHandler.sa_flags = 0;
    sigaction(SIGPIPE, &sigPipeHandler, NULL);

    //Initialize mutexes
    if(pthread_mutex_init(&q_lock, NULL) != 0) {
        fprintf(stderr, "ERROR: Mutex initialization failed on q_lock. \n");
        exit(1);
    }
    if(pthread_mutex_init(&client_sock_lock, NULL) != 0) {
        fprintf(stderr, "ERROR: Mutex initialization failed on client_sock_lock. \n");
        exit(1);
    }

    //Initialize socket
    homeDir.assign(argv[1]);
    if((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        logStream = fopen(logName.c_str(), "a");
        fprintf(logStream, "Socket creation error: %s\n", strerror(errno));
        fclose(logStream);
        //perror("Socket creation error");
        return 1;
    }
    //Allows immediate reuse of socket: credit to stack overflow
    int yes = 1;
    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
      logStream = fopen(logName.c_str(), "a");
      fprintf(logStream, "setsockopt: %s\n", strerror(errno));
      fclose(logStream);
      exit(1);
    }

    servSock.sin_family = AF_INET;
    servSock.sin_port= htons(atoi(argv[1]));
    servSock.sin_addr.s_addr = htonl(INADDR_ANY);
    sockaddr_len = sizeof(servSock);

    if(bind(sock_fd,(struct sockaddr *)&servSock , sizeof(servSock)) < 0) {
      logStream = fopen(logName.c_str(), "a");
      fprintf(logStream, "Bind error: %s\n", strerror(errno));
      fclose(logStream);
        return 1;
    }

    if(listen(sock_fd, 10) < 0) {
      logStream = fopen(logName.c_str(), "a");
      fprintf(logStream, "Listen error: %s\n", strerror(errno));
      fclose(logStream);
        return 1;
    }

    //Initialize thread pool
    for(int i = 0; i < 10; i++) {
        int retVal = pthread_create(&senderThreads[i], NULL, crawlQueue, NULL);
        if(retVal) {
          logStream = fopen(logName.c_str(), "a");
          fprintf(logStream, "pthread_create error: %s\n", strerror(errno));
          fclose(logStream);
          exit(1);
        } else {
          threadsActive++;
        }
    }

    while(!caughtSigInt && threadsActive > 0) {
        //pthread_mutex_lock(&client_sock_lock);
        if((client_fd = accept(sock_fd, (struct sockaddr *)&client, &sockaddr_len)) < 0) {
            logStream = fopen(logName.c_str(), "a");
            fprintf(logStream, "Accept error: %s\n", strerror(errno));
            fclose(logStream);
            while(threadsActive > 0);
            //cout<<"threadsActive after accept error: "<<threadsActive<<endl;
            return 1;
        }
        //pthread_mutex_unlock(&client_sock_lock);
        while((read_size = recv(client_fd , client_req , 2000 , 0)) > 0 ) {
            Ele* ele = new Ele;
            string msg(client_req);
            ele->client_fd = client_fd;
            ele->client_msg = msg;
            pthread_mutex_lock(&q_lock);
            q.push(ele);
            pthread_mutex_unlock(&q_lock);
            bzero(client_req, 2000);
        }

        if(read_size < 0) {
            logStream = fopen(logName.c_str(), "a");
            fprintf(logStream, "Recv error: %s\n", strerror(errno));
            fclose(logStream);
            while(threadsActive > 0);
            //cout<<"threadsActive after accept error: "<<threadsActive<<endl;
            return 1;
        }
    }
    return 0;
}
