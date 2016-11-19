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
#include <dirent.h>

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
#define NUMTAGS 6
std::string tags[NUMTAGS];// = {"id","pw","flag", "file","segment", "msg"}

pthread_mutex_t q_lock;
pthread_mutex_t client_sock_lock;
pthread_mutex_t dir_lock;
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

//Credit to "Peter Parker" on Stack Overflow
int handleSegment(std::string id, std::string msg, std::string filename, std::string seg){
  FILE* file;
  /*logStream = fopen(logName.c_str(), "a");
  fprintf(logStream, "In handle_segment, id = %s, msg = %s, seg = %d\n", id.c_str(), msg.c_str(), seg);
  fclose(logStream);*/
  DIR *dir;
  struct dirent *ent;
  std::string dirName = homeDir+"/"+id;
  pthread_mutex_lock(&dir_lock);
  if ((dir = opendir (dirName.c_str())) != NULL) {
    /* print all the files and directories within directory
      while ((ent = readdir (dir)) != NULL) {
      printf ("%s\n", ent->d_name);
    }*/
    //cout<<"BLAAAAAH"<<endl;
    closedir (dir);
  } else {
    //perror("WTF happened???");
    std::string mkdir = "mkdir "+dirName;
    sleep(1);
    if(system(mkdir.c_str()) < 0) {
      logStream = fopen(logName.c_str(), "a");
      fprintf(logStream, "HORRIBLE MKDIR ERRORS RUN FOR YOUR LIFE\n");
      fclose(logStream);
    }
    //mkdir(homeDir+"/"+id)
  }
  std::string newName = dirName+"/."+filename+"."+seg;
  file = fopen(newName.c_str(), "wb");
  if(file == NULL) {
    logStream = fopen(logName.c_str(), "a");
    fprintf(logStream, "ERROR: Cannot create/open file segment %s\n", newName.c_str());
    fclose(logStream);
    return -1;
  }
  if(fwrite(msg.c_str(), sizeof(char), msg.length(), file) != msg.length()) {
    logStream = fopen(logName.c_str(), "a");
    fprintf(logStream, "ERROR: Didn't write the correct number of bytes to file segment %s\n", newName.c_str());
    fclose(logStream);
  }
  fclose(file);
  pthread_mutex_unlock(&dir_lock);
  return 0;
}

std::string buildMsg(std::string flag, std::string msg){
  return "<all><flag>"+flag+"</flag>\n<msg>"+msg+"</msg></all>\n";
}

//For when msg is complicated
std::string buildMsg(std::string id, std::string pw, std::string flag, std::string msg, std::string segment, int msgLen, std::string filename){
  std::string msgTrunc = msg.substr(0, msgLen);
  return "<all><id>"+id+"</id>\n<pw>"+pw+"</pw>\n<flag>"+flag+"</flag>\n<file>"+filename+"</file>\n<segment>"+segment+"</segment>\n<msg>"+msgTrunc+"</msg></all>\n";
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


int sendFile(int client_fd, std::string fullPath, int segNum) {
    FILE* response;
    struct sockaddr_in remoteSock;
    int fileSize = 0;
    int n;
    int i;
    string header_str;
    bool foundIdx = false;

    response = fopen(fullPath.c_str(), "rb");
    if(response == NULL) {
        logStream = fopen(logName.c_str(), "a");
        fprintf(logStream, "Error opening file. Full path: %s\n", fullPath.c_str());
        fclose(logStream);
        return -1;
    }

    fseek(response, 0, SEEK_END);
    fileSize = ftell(response);
    rewind(response);

    char buffer[fileSize];
    bzero(buffer, fileSize);
    if((n = fread(buffer, sizeof(char), fileSize, response)) == fileSize) {
        string strBuf(buffer, n);
        //std::string id, std::string pw, std::string flag, std::string msg, std::string segment, int msgLen, std::string filename
        string payload = buildMsg("n", "n", "G", strBuf, to_string(segNum), strBuf.length(), fullPath);
        logStream = fopen(logName.c_str(), "a");
        fprintf(logStream, "\tReading file successful, calling send\n");
        fclose(logStream);
        pthread_mutex_lock(&client_sock_lock);
        int sent = send(client_fd, payload.c_str(), payload.length(), 0);
        pthread_mutex_unlock(&client_sock_lock);
        if(sent != payload.length()) {
          logStream = fopen(logName.c_str(), "a");
          fprintf(logStream, "\tSending file unsuccessful!\n");
          fclose(logStream);
        } else {
          logStream = fopen(logName.c_str(), "a");
          fprintf(logStream, "\tSending file %s successful.\n", fullPath.c_str());
          fclose(logStream);
        }
        bzero(buffer, sizeof(buffer));
    } else {
      logStream = fopen(logName.c_str(), "a");
      fprintf(logStream, "Error reading file in sendFile. Full path: %s\n", fullPath.c_str());
      fclose(logStream);
    }
    fclose(response);
    return 0;
}

int handleGet(int client_fd, std::string id, std::string filename){
  DIR *dir;
  struct dirent *ent;
  std::string dirName = homeDir+"/"+id;
  pthread_mutex_lock(&dir_lock);
  std::string findName = "."+filename+".";
  std::string foundFiles[2];
  int segs[2];
  int filesFound = 0;
  std::string resp;
  FILE* f1;
  FILE* f2;
  if ((dir = opendir (dirName.c_str())) != NULL) {
    /* print all the files and directories within directory*/

    while ((ent = readdir (dir)) != NULL) {
      //printf ("%s\n", ent->d_name);
      std::string curFile(ent->d_name);
      int found = curFile.find(findName);
      logStream = fopen(logName.c_str(), "a");
      fprintf(logStream, "\tReading file name %s\n", ent->d_name);
      fclose(logStream);
      //handle if found or not
      if(found == std::string::npos) continue;
      else {
        foundFiles[filesFound] = curFile;
        segs[filesFound] = atoi(curFile.substr(found+findName.length()).c_str());
        logStream = fopen(logName.c_str(), "a");
        fprintf(logStream, "\t\tAdding %s to foundFiles\n\t\tSegment is %d, seg string is %s\n", ent->d_name, segs[filesFound], curFile.substr(found).c_str());
        fclose(logStream);
        filesFound++;
      }
    }
    closedir(dir);

    if(filesFound < 2) {
      resp = buildMsg("E", "File incomplete");
      respond(client_fd, resp);
    } else {
      std::string fp1 = dirName+"/"+foundFiles[0];
      std::string fp2 = dirName+"/"+foundFiles[1];
      logStream = fopen(logName.c_str(), "a");
      fprintf(logStream, "\tCalling sendFile\n");
      fclose(logStream);
      sendFile(client_fd, fp1, segs[0]);
      sendFile(client_fd, fp2, segs[1]);
    }
  } else {
    logStream = fopen(logName.c_str(), "a");
    fprintf(logStream, "ERROR: Cannot open directory.\n");
    fclose(logStream);
  }
  //open dir, read off files, see which ones contain substring "."+filename

  pthread_mutex_unlock(&dir_lock);
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

void handle_msg(int client_fd, std::string full_msg){
  //Tags are id, pw, flag, file, segment, msg
  if(full_msg == "boop") return;
  int seg;
  std::string tagVals[NUMTAGS];
  std::string msg;
  for(int i = 0; i < NUMTAGS; i++) {
    tagVals[i] = searchXML(tags[i], full_msg);
    /*logStream = fopen(logName.c_str(), "a");
    fprintf(logStream, "TagVals[i]: %s tags[i]: %s\n", tagVals[i].c_str(), tags[i].c_str());
    fclose(logStream);*/
  }
  if(tagVals[0] == "NOTFOUND" || tagVals[1] == "NOTFOUND") {
    logStream = fopen(logName.c_str(), "a");
    fprintf(logStream, "User authentication failed: Invalid username/password format\n");
    fclose(logStream);
    msg = buildMsg("A", "INVALID");
    respond(client_fd, msg);
  }
  if(!authenticate(tagVals[0], tagVals[1])) {
    msg = buildMsg("A", "INVALID");
    respond(client_fd, msg);
    return;
  }
  msg = buildMsg("A", "VALID");
  respond(client_fd, msg);
  char flag = tagVals[2][0];


  switch(flag){
    case 'A':
      //Authentication already done
      break;
    case 'L':
      break;
    case 'G':
      handleGet(client_fd, tagVals[0], tagVals[3]);
      break;
    case 'P':
      seg = atoi(tagVals[4].c_str());
      handleSegment(tagVals[0], tagVals[5], tagVals[3], tagVals[4]);
      break;
    default:
    logStream = fopen(logName.c_str(), "a");
    fprintf(logStream, "Received unknown message.\n");
    fclose(logStream);
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
    pthread_mutex_destroy(&dir_lock);
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
    tags[0] = "id";
    tags[1] = "pw";
    tags[2] = "flag";
    tags[3] = "file";
    tags[4] = "segment";
    tags[5] = "msg";
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
    init();

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
    if(pthread_mutex_init(&dir_lock, NULL) != 0) {
        fprintf(stderr, "ERROR: Mutex initialization failed on dir_lock. \n");
        exit(1);
    }

    //Initialize socket
    homeDir.assign(argv[2]);
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
