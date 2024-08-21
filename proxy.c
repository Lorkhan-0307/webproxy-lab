#include <stdio.h>
#include "csapp.h"

/* 과제 조건: HTTP/1.0 GET 요청을 처리하는 기본 시퀀셜 프록시

  클라이언트가 프록시에게 다음 HTTP 요청 보내면 
  GET http://www.google.com:80/index.html HTTP/1.1 
  프록시는 이 요청을 파싱해야한다 ! 호스트네임, www.google.com, /index.html 
  
  이렇게 프록시는 서버에게 다음 HTTP 요청으로 보내야함.
  GET /index.html HTTP/1.0   
  
  즉, proxy는 HTTP/1.1로 받더라도 server에는 HTTP/1.0으로 요청해야함 
*/

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";
static const char *new_version = "HTTP/1.0";

/* Function prototyps => 함수 순서 상관이 없게됨 */
void do_it(int fd);
void do_request(int clientfd, char *method, char *uri_ptos, char *host);
void do_response(int connfd, int clientfd);
int parse_uri(char *uri, char *uri_ptos, char *host, char *port);
void *thread(void *vargp);

int main(int argc, char **argv) { 
  int listenfd, *connfdp;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;
  pthread_t tid;

  /* Check command line args */
  if (argc != 2) {  
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);    
  }

  listenfd = Open_listenfd(argv[1]);

  while(1) {
    clientlen = sizeof(clientaddr);
    
    // Accept를 통해 connfd 열기
    connfdp = Malloc(sizeof(int));
    *connfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen);

    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);

    // Pthread_create를 통해 쓰레드를 생성
    Pthread_create(&tid, NULL, thread, connfdp);    
  }
}

void *thread(void *vargp){
  int connfd = *((int *)vargp);
  Pthread_detach(pthread_self());
  Free(vargp);
  do_it(connfd);
  Close(connfd);
  return NULL;
}

void do_it(int connfd){
  int clientfd;
  char buf[MAXLINE],  host[MAXLINE], port[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char uri_ptos[MAXLINE];
  rio_t rio;

  /* Read request line and headers from Client */
  Rio_readinitb(&rio, connfd);                      
  Rio_readlineb(&rio, buf, MAXLINE);               
  printf("Request headers to proxy:\n");
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);     

  parse_uri(uri, uri_ptos, host, port);

  clientfd = Open_clientfd(host, port);            
  do_request(clientfd, method, uri_ptos, host);     
  do_response(connfd, clientfd);        
  Close(clientfd);                                 
}

// send request to server
void do_request(int clientfd, char *method, char *uri_ptos, char *host){
  char buf[MAXLINE];
  printf("Request headers to server: \n");     
  printf("%s %s %s\n", method, uri_ptos, new_version);

  /* Read request headers */        
  sprintf(buf, "GET %s %s\r\n", uri_ptos, new_version);     // GET /index.html HTTP/1.0
  sprintf(buf, "%sHost: %s\r\n", buf, host);                // Host: www.google.com     
  sprintf(buf, "%s%s", buf, user_agent_hdr);                // User-Agent: ~(bla bla)
  sprintf(buf, "%sConnections: close\r\n", buf);            // Connections: close
  sprintf(buf, "%sProxy-Connection: close\r\n\r\n", buf);   // Proxy-Connection: close

  Rio_writen(clientfd, buf, (size_t)strlen(buf));
}


void do_response(int connfd, int clientfd){
  char buf[MAX_CACHE_SIZE];
  ssize_t n;
  rio_t rio;

  Rio_readinitb(&rio, clientfd);  
  n = Rio_readnb(&rio, buf, MAX_CACHE_SIZE);  
  Rio_writen(connfd, buf, n);
}

int parse_uri(char *uri, char *uri_ptos, char *host, char *port){ 
  char *ptr;

  if (!(ptr = strstr(uri, "://"))) 
    return -1;                         
  ptr += 3;                       
  strcpy(host, ptr);                  

  if((ptr = strchr(host, '/'))){  
    *ptr = '\0';                      // host = www.google.com:80
    ptr += 1;
    strcpy(uri_ptos, "/");            // uri_ptos = /
    strcat(uri_ptos, ptr);            // uri_ptos = /index.html
  }
  else strcpy(uri_ptos, "/");

  /* port 추출 */
  if ((ptr = strchr(host, ':'))){     // host = www.google.com:80
    *ptr = '\0';                      // host = www.google.com
    ptr += 1;     
    strcpy(port, ptr);                // port = 80
  }  
  else strcpy(port, "80");            // port가 없을 경우 "80"을 넣어줌

  return 0; // function int return => for valid check
}