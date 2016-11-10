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

pthread_mutex_t q_lock;
queue<Ele*> q;
int threadsActive;
vector<string> indexes;

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
    while(!q.empty()){
        cout<<"Stuff left in queue?"<<endl;
        Ele* ele = q.front();
        q.pop();
        delete(ele);
    }
    //cout<<"afsfsafsaff"<<endl;
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
        int numTokens;
        if (token[0]) {
            for (n = 1; n < MAX_TOKENS_PER_LINE; n++) {
                token[n] = strtok(0, DELIMITER);
                if (!token[n]) {
                    numTokens = n-1;
                    break; // no more tokens
                }
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
        if(word0 == "Listen") {
            stringstream strVal;
            strVal<<token[1];
            strVal>>port;
            cout<<"Port is: "<<port<<endl;
        }
        if(word0 == "DirectoryIndex") {
            for(int i = 1; i < numTokens; i++) {
                string idx(token[i]);
                indexes.push_back(idx);
            }
            for(int i = 0; i < indexes.size(); i++) {
                cout<<"Indexes["<<i<<"] is "<<indexes[i]<<endl;
            }
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

void pack_header(Header* header, string ext, int* errCode) {
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
    header->val2 = "2"; //gets overwritten
    header->val3 = "Close";
    header->resp_code = "200";
    switch(*errCode) {
        case 200:
            header->resp_human = "OK";
            break;
        case 400:
            header->resp_human = "Bad Request";
            break;
        case 404:
            header->resp_human = "Not Found";
            break;
        case 500:
            header->resp_human = "Internal Server Error: cannot allocate memory";
            break;
        case 501:
            header->resp_human = "Not Implemented";
            break;
        default:
            cout<<"Error assigning human-readable error code in pack_header"<<endl;
            header->resp_human = "undefined error";
    }
    header->resp_human = "OK";
    header->version = "HTTP/1.0";
}

int parse_request(const char* client_req, string* uri, int* errCode, bool* invalidMethod, bool*invalidVersion) {
    char* charURI = new char[200];
    char* charMeth = new char[200];
    char* charVer = new char[200];
    sscanf(client_req, "%s %s %s", charMeth, charURI, charVer);
    string strMeth(charMeth);
    string strUri(charURI);
    string strVer(charVer);
    if(strMeth != "GET" && (strMeth == "POST" || strMeth == "DELETE" || strMeth == "HEAD" || strMeth == "PUT" || strMeth == "OPTIONS")) {
        printf("Unimplemented HTTP method\n");
        *errCode = 501;
        return -1;
    } else if(strMeth != "GET") {
        printf("Completely invalid HTTP method\n");
        *invalidMethod = true;
        *errCode = 400;
        return -1;
    } else if(strUri.find(' ') != string::npos || strUri[0] != '/'){
        cout<<"Invalid URI"<<endl;
        *errCode = 404;
        return -1;
    } else if(strVer.find("HTTP/") == string::npos) {
        printf("Invalid HTTP version\n");
        *invalidVersion = true;
        *errCode = 400;
        return -1;
    } else if(strVer != "HTTP/1.0" && strVer != "HTTP/1.1") {
        cout<<"Incorrect version (not implemented)"<<endl;
        *errCode = 501;
        return -1;
    }
    *uri = strUri;
    return 0;
}

int throwError(int client_fd, Header* header, int* errCode, bool* invalidMethod, bool* invalidVersion){
    cout<<"Inside throw error, errCode is "<<errCode<<endl;
    string msg;
    //set msg length
    switch(*errCode){
    case 200:
        return 0;
    case 400:
        if(*invalidMethod) msg = "<html><body>400 Bad Request Reason: Invalid Method :<<request method>></body></html>";
        else if(*invalidVersion) msg = "<html><body>400 Bad Request Reason: Invalid HTTPVersion: <<req version>></body></html>";
        break;
    case 404:
        msg = "<html><body>404 Not Found Reason URL does not exist :<<requested url>></body></html>";
        break;
    case 500:
        msg = "";
        break;
    case 501:
        msg = "<html><body>501 Not Implemented <<error type>>: <<requested data>></body></html>";
        break;
    }
    string header_str = header->version + " " + header->resp_code + " " + header->resp_human + "\r\n" + header->header1 + ": "+header->val1+"\r\n"+header->header2+": "+header->val2+"\r\n"+ header->header3+": "+header->val3+"\r\n\r\n";
    string payload = header_str + msg;
    const char* charLoad = payload.c_str();
    int sent = send(client_fd, charLoad, payload.length(), 0);
    return 1;
}

int sendFile(int client_fd, string client_msg) {
    FILE* response;
    struct sockaddr_in remoteSock;
    int fileSize = 0;
    int n;
    int i;
    int is404 = 0;
    string header_str;
    Header* header = new Header;
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

    string* uri = new string;
    parse_request(client_msg.c_str(), uri, errCode, invalidMethod, invalidVersion);
    string filepath = *uri;

    //PROBABLY ALL THE BROKEN
    if(filepath == "/") {
        int i = 0;
        do {
            fullPath = homeDir + "/"+indexes[i];
            cout<<"FULL PATH IS: "<<fullPath<<endl;
            response = fopen(fullPath.c_str(), "rb");
            if(response != NULL) {
                foundIdx = true;
                fclose(response);
                break;
            }
            i++;
        } while (!foundIdx && i < indexes.size());
        if(foundIdx) {
            foundIdx = true;
            filepath = "/"+indexes[i];
        } else {
            cout<<"No valid index file found"<<endl;
            is404 = 1;
        }
    }

    fullPath = homeDir + filepath;

    /*stringstream strstream;
    strstream.str(filepath);
    while(getline(strstream, ext, '.'));
    ext = "."+ext;*/
    cout<<filepath<<endl;
    int idx = filepath.find('.');
    ext = filepath.substr(idx, filepath.length()-1);
    ext = "/"+ext;
    cout<<"ext is "<<endl;
    pack_header(header, ext, errCode);

    if(!is404) response = fopen(fullPath.c_str(), "rb");
    if(response == NULL) {
        perror("Open file error in sendFile");
        printf("Full path: %s\n", fullPath.c_str());
        cout<<"homeDir is "<<homeDir<<endl;
        *errCode = 404;
        header->resp_code = "404";
        header->resp_human = "File not found";
    }
    if(throwError(client_fd, header, errCode, invalidMethod, invalidVersion)) return 0;

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
            sendFile(ele->client_fd, ele->client_msg);
        } else{
            //if queue was empty wait and check again
            int sleep_time = rand()%101;
            usleep(sleep_time);
        }
        delete(ele);
    }
}

void catch_sigseg(int s){
    //HORRIBLE HACKY CHEAT NEVER EVER DO THIS THE POINT OF SEG FAULT HANDLERS IS TO DEBUG THEM NOT HIDE THEM ALSO IF YOU GET SEG FAULTS ANYWHERE ELSE YOU'RE SCREWED
    //printf("Segmentation fault in thread %X\n", pthread_self());
    exit(1);
}

void init(){
    caughtSigInt = false;
    threadsActive = 0;
}

int main(int argc, char* argv[]) {
    struct sockaddr_in server, client;
    struct sigaction sigIntHandler;
    int client_fd, read_size;
    socklen_t sockaddr_len;

    sigIntHandler.sa_handler = catch_sigint;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;

    /*sigSegHandler.sa_handler = catch_sigseg;
    sigemptyset(&sigSegHandler.sa_mask);
    sigSegHandler.sa_flags = 0;*/

    sigaction(SIGINT, &sigIntHandler, NULL);
    //sigaction(SIGSEGV, &sigSegHandler, NULL);

    //Initialize mutexes and queue
    if(pthread_mutex_init(&q_lock, NULL) != 0) {
        fprintf(stderr, "ERROR: Mutex initialization failed. \n");
        exit(1);
    }

    set_home_dir();
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
    server.sin_port= htons(port);
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

    while(!caughtSigInt && threadsActive > 0) {
        cout<<"threads active = "<<threadsActive<<endl;
        if((client_fd = accept(sock_fd, (struct sockaddr *)&client, &sockaddr_len)) < 0) {
            perror("Accept error");
            while(threadsActive > 0);
            //cout<<"threadsActive after accept error: "<<threadsActive<<endl;
            return 1;
        }
        //if(caughtSigInt) return 0;
        while((read_size = recv(client_fd , client_req , 2000 , 0)) > 0 ) {
            Ele* ele = new Ele;
            string msg(client_req);
            ele->client_fd = client_fd;
            ele->client_msg = msg;
            pthread_mutex_lock(&q_lock);
            q.push(ele);
            pthread_mutex_unlock(&q_lock);
            bzero(client_req, 2000);
            fflush(stdout);
        }

        if(read_size < 0) {
            perror("Recv failed");
            while(threadsActive > 0);
            //cout<<"threadsActive after accept error: "<<threadsActive<<endl;
            return 1;
        }
    }
    return 0;
}
