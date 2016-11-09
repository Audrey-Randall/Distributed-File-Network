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
using namespace std;

typedef struct Ele{
    int client_fd;
    string uri;
}Ele;

const int MAX_CHARS_PER_LINE = 512;
const int MAX_TOKENS_PER_LINE = 20;
const char* const DELIMITER = " ";
pthread_t senderThreads[10];
string homeDir;
bool caughtSigInt;
int sock_fd;

pthread_mutex_t q_lock;
queue<Ele*> q;
int threadsActive;

/* Notes: TCP sockets differ from UDP in that they need a call to listen() and they use recv(), not recvfrom().
    Why are sockaddr_in structs created like that then cast to sockaddr structs?
*/

typedef struct Header{
    string version;
    string resp_code;
    string resp_human;
    string header1;
    string val1;
    string header2;
    string val2;
    string header3;
    string val3;
}Header;

typedef struct Entry{
    string data;
    int len;
}Entry;

void catch_sigint(int s){
    cout<<"caught signal "<<s<<", exiting"<<endl;
    caughtSigInt = true;
    for(int i=0; i<10; i++)  {
        pthread_join(senderThreads[i], NULL);
        threadsActive--;
    }
    pthread_mutex_destroy(&q_lock);
    close(sock_fd);
    //exit(0);
}

void set_home_dir(){
    ifstream conf;
    bool foundExt = false;
    conf.open("ws.conf");
    if (!conf.good()) {
        perror("No configuration file found");
        printf("Fatal error: exiting.\n");
        exit(1);
    }

    while (!conf.eof()) {
        char buf[MAX_CHARS_PER_LINE];
        conf.getline(buf, MAX_CHARS_PER_LINE);
        int n = 0;
        const char* token[MAX_TOKENS_PER_LINE] = {}; // initialize to 0

        // parse the line
        //strtok returns 0 (NULL?) if it can't find a token
        token[0] = strtok(buf, DELIMITER);
        if (token[0]) {
            for (n = 1; n < MAX_TOKENS_PER_LINE; n++) {
                token[n] = strtok(0, DELIMITER);
                if (!token[n]) break; // no more tokens
            }
        }
        string word0(token[0]);
        if(word0 == "DocumentRoot"){
            int n;
            char* realDir = new char[strlen(token[1])-2];
            for(n = 1; n < strlen(token[1])-1; n++) {
                realDir[n-1]=token[1][n];
            }
            string realRealDir(realDir);
            homeDir = realRealDir;
        }
    }
}

//Credit for a lot of this parsing code goes to http://cs.dvc.edu/HowTo_Cparse.html
int getType(string ext, Entry* type) {
    ifstream conf;
    bool foundExt = false;
    conf.open("ws.conf");
    if (!conf.good()) {
        perror("No configuration file found");
        printf("Fatal error: exiting.\n");
        exit(1);
    }

    while (!conf.eof()) {
        char buf[MAX_CHARS_PER_LINE];
        conf.getline(buf, MAX_CHARS_PER_LINE);
        int n = 0;
        const char* token[MAX_TOKENS_PER_LINE] = {}; // initialize to 0

        // parse the line
        //strtok returns 0 (NULL?) if it can't find a token
        token[0] = strtok(buf, DELIMITER);
        if (token[0]) {
            for (n = 1; n < MAX_TOKENS_PER_LINE; n++) {
                token[n] = strtok(0, DELIMITER);
                if (!token[n]) break; // no more tokens
            }
        }
        if(token[0] && token[1]) {
            string tok0(token[0]);
            string tok1(token[1]);
            if(tok0 == ext) {
                //string tok1(token[1]);
                (type->data).assign(tok1);
                type->len = strlen(token[1]);
                return 0;
            }
        }
    }
    return -1;
}

void pack_header(Header* header, string ext) {
    cout<<"Entering pack_header"<<endl;
    string c_type = "Content-Type";
    string c_len = "Content-Length";
    string conn = "Connection";
    header->header1 = "Content-Type";
    header->header2 = "Content-Length";
    header->header3 = "Connection";
    Entry* type = new Entry;
    type->data = "error";
    getType(ext, type);
    if(type->data == "error") cout<<"Error: content type not found"<<endl;
    header->val1 = type->data;
    header->val2 = "2";
    header->val3 = "Close";
    header->resp_code = "200";
    header->resp_human = "OK";
    header->version = "HTTP/1.0";
}

int parse_request(char* client_req, char* method, char* uri, char* version) {
    sscanf(client_req, "%s %s %s", method, uri, version);
    if(strcmp(method, "GET")) {
        printf("Unimplemented HTTP method\n");
        return -501;
    } else {
        printf("In parse_request, command was \"%s,%s,%s\"\n", method, uri, version);
    }
    return 0;
}

int sendFile(int client_fd, string filepath) {
    FILE* response;
    struct sockaddr_in remoteSock;
    int fileSize = 0;
    int n;
    int i;
    int is404 = 0;
    string header_str;
    Header* header = new Header;
    string ext;
    string fullPath = homeDir + filepath;

    stringstream strstream;
    strstream.str(filepath);
    while(getline(strstream, ext, '.'));
    ext = "."+ext;
    pack_header(header, ext);

    response = fopen(fullPath.c_str(), "rb");
    if(response == NULL) {
        perror("Open file error in sendFile");
        printf("Full path: %s\n", fullPath.c_str());
        cout<<"homeDir is "<<homeDir<<endl;
        is404 = 1;
        header->resp_code = "404";
        header->resp_human = "File not found";
    }

    if(is404) {
        response = fopen("404.html", "rb");
        if(response == NULL) {
            printf("AAAAARGH THE ERROR MESSAGE HAS AN ERROR\n");
            return -1;
        }
        printf("response is %p\n", response);
    }

    fseek(response, 0, SEEK_END);
    fileSize = ftell(response);
    rewind(response);
    //MinGW doesn't recognize to_string, known bug
    ostringstream ss;
    ss << fileSize;
    header->val2 = ss.str();

    header_str = header->version + " " + header->resp_code + " " + header->resp_human + "\r\n" + header->header1 + ": "+header->val1+"\r\n"+header->header2+": "+header->val2+"\r\n"+ header->header3+": "+header->val3+"\r\n\r\n";

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
        int sent = send(client_fd, charLoad, payload.length(), 0);
        bzero(buffer, sizeof(buffer));
    }
    if (n < 0) printf("Read error\n");
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
            sendFile(ele->client_fd, ele->uri);
        } else{
            //if queue was empty wait and check again
            int sleep_time = rand()%101;
            usleep(sleep_time);
        }
        delete(ele);
    }
}

int main(int argc, char* argv[]) {
    struct sockaddr_in server, client;
    struct sigaction sigIntHandler;
    int client_fd, read_size;
    socklen_t sockaddr_len;
    char* client_req = new char[1024];
    char* method = new char[4];
    char* uri = new char[1086]; //2000-2-4-8
    char* version = new char[8];
    bzero(client_req, 1024);
    bzero(method, 4);
    bzero(uri, 1086);
    bzero(version, 8);
    caughtSigInt = false;
    threadsActive = 0;

    sigIntHandler.sa_handler = catch_sigint;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;

    sigaction(SIGINT, &sigIntHandler, NULL);

    //Initialize mutexes and queue
    if(pthread_mutex_init(&q_lock, NULL) != 0) {
        fprintf(stderr, "ERROR: Mutex initialization failed. \n");
        exit(1);
    }

    if((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return 1;
    }
    //Allows immediate reuse of socket: credit to stack overflow
    int yes = 1;
    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
        perror("setsockopt");
        exit(1);
    }

    server.sin_family = AF_INET;
    server.sin_port= htons(8080);
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    sockaddr_len = sizeof(server);

    if(bind(sock_fd,(struct sockaddr *)&server , sizeof(server)) < 0) {
        perror("Bind error");
        return 1;
    }

    if(listen(sock_fd, 10) < 0) {
        perror("Listen error");
        return 1;
    }
    set_home_dir();

    //Initialize thread pool
    for(int i = 0; i < 10; i++) {
        int retVal = pthread_create(&senderThreads[i], NULL, crawlQueue, NULL);
        if(retVal) {
            perror("Error in pthread_create");
            exit(1);
        } else {
            threadsActive++;
        }
    }

    while(1) {
        if((client_fd = accept(sock_fd, (struct sockaddr *)&client, &sockaddr_len)) < 0) {
            perror("Accept error");
            return 1;
        }
        if(caughtSigInt) return 0;
        while((read_size = recv(client_fd , client_req , 2000 , 0)) > 0 ) {
            //parse request. TODO: handle security issues here
            if(parse_request(client_req, method, uri, version) < 0) {
                printf("Parse error\n");
                //send error consistent with return value
            } else {
                //push client_fd and uri to queue?
                //so that threads can read from queue and send the file?
                Ele* ele = new Ele;
                ele->client_fd = client_fd;
                ele->uri = uri;
                pthread_mutex_lock(&q_lock);
                q.push(ele);
                pthread_mutex_unlock(&q_lock);

                //sendFile(client_fd, uri);
            }
            bzero(client_req, 2000);
            fflush(stdout);
        }

        if(read_size < 0) {
            perror("Recv failed");
            return 1;
        }
    }
    return 0;
}

