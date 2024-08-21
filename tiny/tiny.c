/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize, char* method);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void echo(int connfd);


int main(int argc, char **argv) {
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);
  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr,
                    &clientlen);  // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);   // line:netp:tiny:doit
    //echo(connfd);
    Close(connfd);  // line:netp:tiny:close
  }
}

// doit함수의 역할은 한개의 HTTP 트랜잭션을 처리하는 것.
// 먼저, 요청 라인을 읽고 분석.
// GET 메소드만 지원하는 Tiny 서버... GET 메소드가 아니면 error 메세지를 보내야 한다. 이후, main으로 복귀하여 대기한다.
// 
void doit(int fd)
{
  // doit에서 받는 fd int는 connfd. 이거는 이전 echo server를 만들때 사용했었는데, Accept로 받은(요청 라인을 통해 받은) 연결 식별자.
  // 다시 말해, 클라이언트에서 도달해야 하는 연결의 끝점. 이는 매번 요청할 때마다 생성되는 것임을 기억해두도록 하자.
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;

  Rio_readinitb(&rio, fd);
  Rio_readlineb(&rio, buf, MAXLINE);
  printf("Request headers:\n");
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);

  // 만약 수신받은 method가 GET이면 strcasecmp함수는 0을 반환(false)한다. 즉, 이것이 아니면 POST 등의 메서드로 서버에 접근한 것.
  if(strcasecmp(method, "GET")){
    clienterror(fd, filename, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }
  read_requesthdrs(&rio);

  // cgiargs는 CGI에서 사용하는 arguments라는 뜻.
  // CGI는 Common Gateway Interface.
  is_static = parse_uri(uri, filename, cgiargs);
  if(stat(filename, &sbuf) < 0) {
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't read the file");
    return;
  }

  if(is_static){
    // Serve static content
    // S_ISREG 은 파일이 일반 파일인지를 확인한다. sbuf는 파일의 상태 정보가 담긴 'stat'구조체. 파일의 유형과 권한을 포함한다.
    // S_ISREG 는 이중 sbuf.st_mode를 확인한다. __S_IFREG은 0100000으로, 이는 일반 파일임을 말하고, 이것의 참, 거짓을 반환한다.
    // S_IRUSR 는 파일의 권한이 User에게 있는지를 확인하는 매크로이다. st_mode의 권한을 확인한다고 생각하면 될 것 같다.
    // 즉, 일반 파일이 아니거나, 파일의 권한이 사용자에게 없다면 403 에러를 반환하는 부분이다.
    if(!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)){
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }
    // 권한도 충분하고 일반 파일이라면
    serve_static(fd, filename, sbuf.st_size, method);
  }
  else{
    // serve dynamic content
    if(!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)){
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }
    serve_dynamic(fd, filename, cgiargs);
  }
}


void clienterror(int fd, char *cause, char *errnum, char *shortmsg,char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];

  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s <body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s %s : %s \r\n", body, errnum, shortmsg);
  sprintf(body, "%s <p>%s : %s \r\n", body, longmsg, cause);
  sprintf(body, "%s <hr><em>The Tiny Web server -- ERROR</em>\r\n", body);

  sprintf(buf, "HTTP/1.1 %s  %s \r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);
  // 만약 buf가 \r\n이면 종료한다.
  // 책에서는 이렇게 작성되어 있다.
  // 요청 헤더를 종료하는 빈 텍스트 줄이 6번 줄(While문)에서 체크하고 있는 carriage return과 1ine feed 쌍으로 구성되어 있다는 점에서 주목하라.
  // 이게 무슨 말일까?
  // 먼저, 1ine feed는 당연히 line feed의 오타고, \n을 의미한다. carriage return은 \r로, 커서를 제일 앞으로 가져가는 문자.
  // 그니까, 이것이 들어오기 전까지 확인한다~ 라는 얘기다.
  while(strcmp(buf, "\r\n")){
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}


// parse_uri는 정적 컨텐츠를 위한 홈 디렉토리가 자신의 현재 디렉토리이며, 실행파일의 홈 디렉토리는 /cgi-bin 이라고 가정한다.
// string cgi-bin을 포함하는 모든 URI는 동적 컨텐츠를 요청하는 것을 나타낸다고 가정한다.
// 기본 파일 이름은 ./home.html이다.

int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char  *ptr;

  if(!strstr(uri, "cgi-bin")){ // static content
  // 정적 컨텐츠를 사용하기 위해, CGI인자 스트링을 지우고 URI를 ./index.html 등의 상대 리눅스 경로이름으로 변환한다.
    strcpy(cgiargs, "");
    strcpy(filename, ".");
    strcat(filename, uri);
    // 만약 uri가 / 로 끝난다면 기본 파일 이름을 추가한다.
    if(uri[strlen(uri)-1] == '/') strcat(filename, "home.html");
    return 1;
  }
  else{ // dynamic content 
  // 동적 컨텐츠를 사용하기 위해, 모든 CGI 인자들을 추출한다.
    ptr = index(uri, '?');
    if(ptr){
      strcpy(cgiargs, ptr+1);
      *ptr = '\0';
    }
    else{
      strcpy(cgiargs, "");
    }
    // 나머지 URI부분을 상대 리눅스 파일 이름으로 변환한다.
    strcpy(filename, ".");
    strcat(filename, uri);
    return 0;
  }
}

void serve_static(int fd, char *filename, int filesize, char* method)
{
  //  serve 할 static source 의 fd
  int srcfd;
  //  source 를 가르킬 포인터, 명시해줄 filetype 을 위한 문자열,
  char *srcp, filetype[MAXLINE], buf[MAXBUF];
  char header[MAXLINE] = "";
  //  filetype 을 filetype 문자열에 복사
  get_filetype(filename, filetype);
  //  헤더 작성해서 buf 에 복사 (sprintf 로 하므로 buf 는 갱신되지 않으므로)
  //  계속 %s , buf 쌍을 써서 작성해야 함
  sprintf(buf, "HTTP/1.1 200 OK\r\n"); /* write response */
  strcat(header, buf);
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  strcat(header, buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  strcat(header, buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  strcat(header, buf);
  //  줄바꿈 두 번으로 빈 줄 만들어서 헤더 끝 표시
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  strcat(header, buf);
  //  다 쓴 헤더 fd 에 복사
  Rio_writen(fd, header, strlen(header));
  printf("Response headers:\n");
  printf("%s", header);
  if (!strcasecmp(method, "GET"))
  {
    //  요청 들어온 static file open
    srcfd = Open(filename, O_RDONLY, 0);
    //  mmap = 파일 내용을 프로세스의 가상 메모리 영역에 매핑해주는 함수
    //  첫 번째 인자로 주소를 받는데, NULL 에 해당하는 값을 넣으면 알아서 매핑해줌
    //  (보통 이렇게들 씀) 마지막 인자는 매핑을 시작할 오프셋
    // srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    srcp = (char *)Malloc(filesize);
    Rio_readn(srcfd, srcp, filesize);
    //  매핑 해 뒀으니 fd 는 close
    Close(srcfd);
    //  fd 에 파일 내용 복사
    Rio_writen(fd, srcp, filesize);
    //  그 후 mmap 에 대한 명시적 할당 해제
    // Munmap(srcp, filesize);
    free(srcp);
  }
}

// filename을 통해 file type을 만드는...?
void get_filetype(char *filename, char *filetype)
{
  if(strstr(filename, ".html")) strcpy(filetype, "text/html");
  else if(strstr(filename, ".gif")) strcpy(filetype, "image/gif");
  else if(strstr(filename, ".png")) strcpy(filetype, "image/png");
  else if(strstr(filename, ".jpg")) strcpy(filetype, "image/jpeg");
  // 11.7
  else if(strstr(filename, ".mpg")) strcpy(filetype, "video/mpeg");
  else if(strstr(filename, ".mp4")) strcpy(filetype, "video/mp4");
  else strcpy(filetype, "text/plain");
}

void serve_dynamic(int fd, char *filename, char *cgiargs)
{
  char buf[MAXLINE], *emptylist[] = {NULL};

  // return the first part of http response
  sprintf(buf, "HTTP/1.1 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  if(Fork() == 0) {// chile process
    // Real server would set all CGI vars here...?
    setenv("QUERY_STRING", cgiargs, 1);
    Dup2(fd, STDOUT_FILENO);
    Execve(filename, emptylist, environ);
  }
  Wait(NULL);
}

void echo(int connfd)
{
    size_t n;
    char buf[MAXLINE];
    rio_t rio;

    Rio_readinitb(&rio, connfd);
    while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
        printf("echoed request line and header : %s\n", buf);
        Rio_writen(connfd, buf, n);
    }
    printf("END OF ECHO\n");
}