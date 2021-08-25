#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netdb.h>

//prototypes
char *sendRespond(int, int);
void iterativeMessage(char *, char *, char *, char *, int, int, FILE *);
int fileStat(char *, char *, int);
int loadMdbServer(char *, unsigned short);
int mdbReq(char *, FILE *, int, int);

static void die(const char *s) { perror(s); exit(1); }

int main(int argc, char **argv)
{
    if (argc != 5) { 
        fprintf(stderr,
        "usage: %s <server port> <web root> <mdb-lookup-host> <mdb-lookup-port>\n", argv[0]);
        exit(1);
    }

    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        die("signal() failed");

    unsigned short port = atoi(argv[1]);
    char *root = argv[2];
    char *mdbHost = argv[3];
    unsigned short mdbPort = atoi(argv[4]);
    //printf("port: %d\nroot: %s\nmdbHost: %s\nmdbPort: %d\n",
    //	      port, root, mdbHost, mdbPort);
    
   
    //Server connection 
    int servSock;
    if ((servSock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
            die("socket() failed");

    struct sockaddr_in servAddr;
    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr.sin_port = htons(port);

    if (bind(servSock, (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0)
            die("bind() failed");
    if (listen(servSock, 5) < 0)
            die("listen() failed");
    
    //mdb connecion
    int mdb = loadMdbServer(mdbHost, mdbPort);
    FILE *mdbFdo = fdopen(mdb, "r");
    if (mdbFdo == NULL)
         die("fdopen() failed");

    //client
    struct sockaddr_in clntAddr;
    unsigned int clntLen;
    int clntsock;
    int status;

    char buffer[4096];
    int ever = 1;

    //connect with client
    //errors that are not require die()
    //will make to loop to continue and reitarate
    for(;ever;) {

	int con = 0;

	clntLen = sizeof(clntAddr);
        if (( clntsock = accept(servSock, 
                (struct sockaddr *) &clntAddr, &clntLen)) < 0)
            die("accept() failed");     
    
        //fprintf(stderr, "\nconnection started\n");
	
	FILE *fdoClnt = fdopen(clntsock, "rb");
        if (fdoClnt == NULL)
                die("fdopen() failed");
        char getReq[4096];
        if(!fgets(getReq,sizeof(getReq), fdoClnt)) {
              //  fprintf(stderr,"fgets() error"); //first line
                status = 400;
		sendRespond(status, clntsock);
		fclose(fdoClnt);
		continue;       
	} 
	//printf("%s get request\n", getReq);
        //from specs
        char *token_separators = "\t \r\n"; // tab, space, new line
        char *method = strtok(getReq, token_separators);
        char *requestURI = strtok(NULL, token_separators);
        char *httpVersion = strtok(NULL, token_separators);

	//printf("%s \"%s %s %s\" %d %s\n", inet_ntoa(clntAddr.sin_addr), method, URI, ver, status);

        //error checking
	if (!requestURI || !method ||
            !httpVersion || strcmp(method, "GET") ||
            !strcmp(httpVersion, "HTTP/1.0")) {
            status = 501;
            iterativeMessage(inet_ntoa(clntAddr.sin_addr), method,
			    requestURI, httpVersion, status,clntsock, fdoClnt);
	    continue;
        
	} else if (strstr(requestURI, "..")) {          
            status = 400;
	    iterativeMessage(inet_ntoa(clntAddr.sin_addr), method,
			    requestURI, httpVersion, status,clntsock, fdoClnt);
            continue;

	} else {	    
	     
	     while(fgets(buffer, sizeof(buffer), fdoClnt) != NULL) {
	    
	         if(!strcmp("\r\n", buffer) || !strcmp("\n", buffer))
		       break;
	         else if(ferror(fdoClnt)) {
		       fprintf(stderr, "\nfgets() error");
	    	       con = 1;
		       status = 400;
		       break;
	         }   
	     }
	}
	//printf("%s main buffer\n", buffer);
	if (con) {
	    iterativeMessage(inet_ntoa(clntAddr.sin_addr), method,
  			     requestURI, httpVersion, status,clntsock, fdoClnt);
	    con = 0;
	    continue;
	}
	
	if (!strncmp(requestURI, "/mdb-lookup", 11) || !strncmp(requestURI, "/mdb-lookup?", 12))
	    status = mdbReq(requestURI, mdbFdo, mdb, clntsock);
	else
	    status = fileStat(root, requestURI, clntsock);
	
	iterativeMessage(inet_ntoa(clntAddr.sin_addr), method,	
			 requestURI, httpVersion, status,clntsock, fdoClnt);	
    }

    fclose(mdbFdo);
    return 0;
}

//gather and print all of the requests and statuses and close the file for
//the client socket each iteration.
void iterativeMessage(char *inet, char *method,
              char *URI, char *ver, int status, int clnt, FILE *fp)
{
    char *sr = sendRespond(status, clnt);
//    printf("IMsr: %s", sr); 
    fprintf(stderr, "%s \"%s %s %s\" %d %s\n", inet, method, URI,
            ver, status, sr);
    fclose(fp);
}

//get the message according to the status number and sends it.
//returning the message.
char *sendRespond(int err, int clnt)
{
    char *str;
    //status codes messages:
    switch(err) {
        case 200:
            str = "OK";
            break;
        case 201:
            str = "created";
            break;
        case 202:
            str = "Accepted";
            break;
        case 203:
            str = "Non-Authoritative Information (since HTTP/1.1)";
            break; 
        case 204:
            str = "No Content";
            break;
        case 205:
            str = "Reset Content";
            break;
        case 206:
            str = "Partial Content (RFC 7233)";
            break;
        //there are many more                                                   
        //
        //
        case 301:
            str = "Moved Permanently";
            break;
        case 302:
            str = "Moved Temporary";
            break; 
        //
        //
	case 400:
	    str = "Bad Request";
	    break;
	case 403:
	    str = "Forbidden";
	    break;
        case 404:
            str = "Not Found";
	    break;
        //
        //
        //
        case 501:
            str = "Not Implemented";
            break; 
        default:
            str = "Unkown protocol";
    }       
    
    char buffer[4096];
    sprintf(buffer, "HTTP/1.0 %d %s\r\n\r\n", err, str);

    if(err != 200) {
        char buf[4096];
        sprintf(buf,
                "<html><body>\n<h1>%d %s</h1>\n</body></html>\n", 
                err, str);
        strcat(buffer, buf); //concat both msgs when not OK.
    }
    //printf("%s respond buffer\n", buffer);
    //
    if(send(clnt, buffer, strlen(buffer), 0) != strlen(buffer))
            fprintf(stderr,"\nsend() failed");

    return str;
}

//creating and searching for the static file path request and sends the file
int fileStat(char *root, char *URI, int clntSock)
{

    int status;
    char *line = (char*)malloc(4096);
    if (line == NULL) {
        status = 400;
        sendRespond(status, clntSock); 
        die("malloc returned NULL");
    }
    strcpy(line, root);
    strcat(line, URI);//copy and concat the root and the URI
    if (line[strlen(line)-1] == '/') { //add index if needed
        strcat(line, "index.html");
    }
    //printf("%s line\n", line);

    struct stat sb; //give line to stat
    if (stat(line, &sb) == 0 && S_ISDIR(sb.st_mode)) {
        status = 403;
        sendRespond(status, clntSock);
        free(line);
        return status;
    }


    FILE *fp = fopen(line, "rb");
    if (fp == NULL) {
        status = 404;
        sendRespond(status, clntSock);
        free(line);
        return status;
    }
    status = 200;
    sendRespond(status, clntSock);


    size_t n;
    char buf[4096];//send the data
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {

        if (send(clntSock, buf, n, 0) != n) {
            fprintf(stderr, "\nsend() failed");
            break;
        }

        if (ferror(fp))
            fprintf(stderr, "\nfread() failed");
    }

    //printf("%s stat buf\n", buf);
    free(line);
    fclose(fp);
    return status;
}

//load the mdb server connection. return the socket.
int loadMdbServer(char *mdbHost, unsigned short mdbPort)
{
    int mdbSock;
    if ((mdbSock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
            die("socket() failed");
    struct hostent *mdbHe;
    if ((mdbHe = gethostbyname(mdbHost)) == NULL)
            die("gethoatbyname() failed");
    char *mdbServerIP = inet_ntoa(*(struct in_addr *)mdbHe->h_addr);


    struct sockaddr_in mdbAddr;
    memset(&mdbAddr, 0, sizeof(mdbAddr));
    mdbAddr.sin_family = AF_INET;
    mdbAddr.sin_addr.s_addr = inet_addr(mdbServerIP);
    mdbAddr.sin_port = htons(mdbPort);

    //connect
    if (connect(mdbSock, (struct sockaddr *)&mdbAddr, sizeof(mdbAddr)) < 0)
           die("connect() failed");

    return mdbSock;
}


int mdbReq(char *URI, FILE *mdbFp, int mdbsock, int clntsock)
{
    //table fomrmats
    const char *form =
            "<h1>mdb-lookup</h1>\n"
            "<p>\n"
            "<form method=GET action=/mdb-lookup>\n"
            "lookup: <input type=text name=key>\n"
            "<input type=submit>\n"
            "</form>\n"
            "<p>\n";
    
    char *qmURI = "/mdb-lookup?key="; //question mark URI
    char *table = "<p><table border>";
    char *closeTable = "\n</table>\n";
    char *closeBody = "</body></html>\n";
    char *row = "\n<tr><td>";
    char *yellowRow  = "<tr><td style=\"background-color:yellow;\">";
    
    int status = 200;
    char buffer[4096];

    if(!strncmp(URI, qmURI, strlen(URI)) ||
       !strncmp(URI, qmURI, strlen(qmURI))) {
	 
    	 sendRespond(status, clntsock);
   	 if (send(clntsock, form, strlen(form), 0) != strlen(form)) {
	     fprintf(stderr, "\nsend() failed"); 
	     return status;
	 }
	 //send the lookup to the mdb server
	 size_t keyLoc = strlen(qmURI);
	 char *key = URI + keyLoc;
	 fprintf(stderr, "looking up [%s]", key);
	 if (send(mdbsock, key, strlen(key), 0) != strlen(key) ) {
	     fprintf(stderr, "\nsend() failed"); 
	     return status;
	 }
	 if (send(mdbsock, "\n", 1, 0) != 1) {
	     fprintf(stderr, "\nsend() failed"); 
	     return status; 
	 }
	 //read from the server and it back.
	 if (send(clntsock, table, strlen(table), 0) != strlen(table)) {
	     fprintf(stderr, "\nsend() failed"); 
	     return status;
	 }
	 int rowColor = 1;
	 while(fgets(buffer, sizeof(buffer), mdbFp) != NULL ) {
		 
	     if (ferror(mdbFp)) {
		 fprintf(stderr, "\nmdb-lookup-server connection failed");
		 break;		 
	     } else if (!strcmp("\n", buffer)) {
	        // printf("\nnewline break\n");
		 break;
		 
	     //format a table and colose the table.
	     } else if (rowColor) {
	         if(send(clntsock, row, strlen(row), 0) != strlen(row)) {
		     fprintf(stderr, "\nsend() failed");
		     return status;
		 }
		 rowColor = 0;
		
	     } else {
	         if (send(clntsock, yellowRow, strlen(yellowRow), 0) != strlen(yellowRow)) {
		     fprintf(stderr, "\nsend() failed");
		     return status;
		 }
		 rowColor = 1;
	     }
	     
	     if (send(clntsock, buffer, strlen(buffer), 0) != strlen(buffer)) {
		 fprintf(stderr, "\nsend() failed");
       	         return status;
	     }
	 }
	 
	 //complete the formal
	 if (send(clntsock, closeTable, 
			       strlen(closeTable), 0) != strlen(closeTable)) {
	     fprintf(stderr, "\nsend() failed");
	     return status;
	 }

    	 if (send(clntsock, closeBody, strlen(closeBody), 0) != strlen(closeBody))
	     fprintf(stderr, "\nsend() failed");

  	  
    }
	
    return status;
}
