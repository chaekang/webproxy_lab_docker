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
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

int main(int argc, char **argv)
{
  int listenfd, connfd;  // listenfd: 듣기 소켓의 파일 디스크립터, connfd: 연결 소켓의 파일 디스크립터
  char hostname[MAXLINE], port[MAXLINE];  // hostname: 클라이언트의 호스트 이름, port: 클라이언트의 포트 번호
  socklen_t clientlen;  // clientlen: 클라이언트 주소 정보의 크기, socklen_t: 소켓 주소 구조체의 크기를 저장하는 타입
  struct sockaddr_storage clientaddr;  // clientaddr: 클라이언트의 주소 정보를 저장할 구조체, sockaddr_storage: 모든 종류의 소켓 주소를 저장할 수 있는 구조체

  /* Check command line args */
  if (argc != 2)  // 2: 프로그램 이름과 포트 번호, 총 2개의 인자 필요, 1: 프로그램 이름, 2: 포트 번호, argc: 인자의 개수, argv: 인자들의 배열
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);  // 프로그램 이름과 포트 번호를 입력하라는 메시지 출력
    exit(1);  // 프로그램 종료, 1: 비정상 종료, 0: 정상 종료
  }

  listenfd = Open_listenfd(argv[1]);  // argv[1]: 포트 번호, Open_listenfd: 포트 번호를 이용해서 듣기 소켓을 열고, 그 소켓의 파일 디스크립터를 반환
  while (1)
  {
    clientlen = sizeof(clientaddr);  // clientaddr: 클라이언트의 주소 정보를 저장할 구조체, clientlen: 클라이언트 주소 정보의 크기
    connfd = Accept(listenfd, (SA *)&clientaddr, // accept: 듣기 소켓에서 연결 요청을 받아들이고, 새로운 연결 소켓의 파일 디스크립터를 반환, 결국 클라이언트와의 통신에 사용할 소켓이 됨
                    &clientlen); // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0);  // 클라이언트의 주소 정보를 이용해서 호스트 이름과 포트 번호를 얻어옴
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);  // line:netp:tiny:doit
    Close(connfd); // line:netp:tiny:close
  }
}

// 클라이언트 요청을 처리하는 함수
void doit (int fd)
{
  int is_static;  // 요청된 콘텐츠가 정적 콘텐츠인지 동적 콘텐츠인지 구분하는 변수, 1: 정적 콘텐츠, 0: 동적 콘텐츠
  struct stat sbuf;  // sbuf: 파일의 상태 정보를 저장할 구조체, stat: 파일의 상태 정보를 얻어오는 함수, 파일의 크기, 권한, 수정 시간 등의 정보를 포함
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];  // buf: 클라이언트로부터 읽어온 요청 라인을 저장할 버퍼, method: HTTP 메서드, uri: 요청된 URI, version: HTTP 버전
  char filename[MAXLINE], cgiargs[MAXLINE];  // filename: 요청된 파일의 이름, cgiargs: CGI 프로그램에 전달할 인자들을 저장할 버퍼
  rio_t rio;  // rio: 클라이언트로부터 읽어온 데이터를 저장할 버퍼와 관련된 정보를 담는 구조체

  rio_readinitb(&rio, fd);  // rio 구조체 초기화, 왜냐하면 클라이언트로부터 데이터를 읽어오기 전에 버퍼와 관련된 정보를 초기화해야 하기 때문
  if (!Rio_readlineb(&rio, buf, MAXLINE))  // 클라이언트로부터 요청 라인을 읽어오는데 실패한다면, 클라이언트가 연결을 끊었다는 의미이므로 함수 종료
  {
    return;
  }

  printf("Request headers:\n");
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);  // 메모리에 들어있는 문자열에서 읽음(scanf: 키보드 입력), buf에 있는 문자열을 읽어서 method uri version에 넣음
  //소켓에서 들어오는 데이터가 "GET /home.html HTTP/1.0\r\n"처럼 한 줄 문자열 전체로 들어온다. 파싱되어 있지 않기 때문에 sscanf로 method, uri, version을 구분해서 저장해야 한다.

  if (strcasecmp(method, "GET"))  // 문자열을 비교해서 method가 get이 아니면
  {
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");  // 에러 반환, 501: 구현되지 않은 메서드
    return;
  }

  read_requesthdrs(&rio);  // 요청 헤더를 읽어서 무시, 클라이언트가 보낸 요청 헤더는 이 서버에서 사용하지 않기 때문에 읽어서 버림, 왜냐하면 이 서버는 GET 메서드만 지원하고, 요청 헤더는 정적 콘텐츠와 동적 콘텐츠를 구분하는 데 필요하지 않기 때문

  is_static = parse_uri(uri, filename, cgiargs);  // uri를 파싱해서 filename과 cgiargs에 저장
  if (stat(filename, &sbuf) < 0)  // 파일의 상태 정보를 얻어오는데 실패한다면, 파일이 존재하지 않거나 접근할 수 없다는 의미이므로 에러 반환
  {
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find the file");
    return;
  }

  if (is_static)  // 요청된 콘텐츠가 정적 콘텐츠라면
  {
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))  // 파일이 일반 파일이 아니거나 읽기 권한이 없다면, S_ISREG: 파일이 일반 파일인지 확인하는 매크로, S_IRUSR: 소유자에게 읽기 권한이 있는지 확인하는 매크로
    {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }
    serve_static(fd, filename, sbuf.st_size);  // 정적 콘텐츠 제공
  }
  else  // 요청된 콘텐츠가 동적 콘텐츠라면
  {
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode))  // 파일이 일반 파일이 아니거나 실행 권한이 없다면, S_IXUSR: 소유자에게 실행 권한이 있는지 확인하는 매크로
    {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs);  // 동적 콘텐츠 제공
  }
}

// 클라이언트에게 에러 메시지를 보내는 함수
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];  // buf: HTTP 응답 라인과 헤더를 저장할 버퍼, body: HTTP 응답 본문을 저장할 버퍼, 이때 http 응답이란 서버가 브라우저한테 돌려주는 내용 전체

  // printf: 화면에 바료 출력, sprintf: 문자열을 버퍼에 저장
  sprintf(body, "<html><title>Tiny Error</title>");  // HTTP 응답 본문 저장, 예: "<html><title>Tiny Error</title><body bgcolor=\"ffffff\">\r\n"
  sprintf(body + strlen(body), "<body bgcolor=\"ffffff\">\r\n");  // body + strlen(body): body에 이미 저장된 문자열의 끝 부분에 새로운 문자열을 이어서 저장, 예: "<html><title>Tiny Error</title><body bgcolor=\"ffffff\">\r\n"
  sprintf(body + strlen(body), "%s: %s\r\n", errnum, shortmsg);  // HTTP 응답 본문 저장, 예: "<html><title>Tiny Error</title><body bgcolor=\"ffffff\">\r\n404: Not found\r\n"
  sprintf(body + strlen(body), "<p>%s: %s\r\n", longmsg, cause);  // HTTP 응답 본문 저장, 예: "<html><title>Tiny Error</title><body bgcolor=\"ffffff\">\r\n404: Not found\r\n<p>Tiny couldn't find the file: /home.html\r\n"
  sprintf(body + strlen(body), "<hr><em>The Tiny Web server</em>\r\n");  // HTTP 응답 본문 저장, 예: "<html><title>Tiny Error</title><body bgcolor=\"ffffff\">\r\n404: Not found\r\n<p>Tiny couldn't find the file: /home.html\r\n<hr><em>The Tiny Web server</em>\r\n"

  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);  // HTTP 응답 라인 저장, 예: "HTTP/1.0 404 Not found\r\n"
  Rio_writen(fd, buf, strlen(buf));  // HTTP 응답 라인 전송, Rio_writen: 소켓에 데이터를 쓰는 함수, fd: 소켓의 파일 디스크립터, buf: 전송할 데이터가 저장된 버퍼, strlen(buf): 전송할 데이터의 크기
  sprintf(buf, "Content-type: text/html\r\n");  // HTTP 응답 헤더 저장, 예: "Content-type: text/html\r\n"
  Rio_writen(fd, buf, strlen(buf));  // HTTP 응답 헤더 전송
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));  // HTTP 응답 헤더 저장, 예: "Content-length: 123\r\n\r\n", \r\n\r\n: 헤더와 본문을 구분하는 빈 줄
  Rio_writen(fd, buf, strlen(buf)); // HTTP 응답 헤더 전송
  Rio_writen(fd, body, strlen(body));  // HTTP 응답 본문 전송, body: 전송할 데이터가 저장된 버퍼, strlen(body): 전송할 데이터의 크기
}

// 요청 헤더를 읽어서 무시하는 함수
void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];  // buf: 요청 헤더를 읽어서 저장할 버퍼, 요청 헤더는 클라이언트가 보낸 추가적인 정보로, 이 서버에서는 사용하지 않기 때문에 읽어서 버림

  rio_readlineb(rp, buf, MAXLINE);  // 요청 헤더를 한 줄씩 읽어서 buf에 저장
  while (strcmp(buf, "\r\n"))  // 요청 헤더의 끝을 나타내는 빈 줄이 나올 때까지 반복, 요청 헤더는 \r\n으로 끝나기 때문에, strcmp(buf, "\r\n")가 0이 될 때까지 반복
  {
    printf("%s", buf);
    rio_readlineb(rp, buf, MAXLINE);  // 요청 헤더를 한 줄씩 읽어서 buf에 저장, 요청 헤더는 클라이언트가 보낸 추가적인 정보로, 이 서버에서는 사용하지 않기 때문에 읽어서 버림
  }
  printf("\n");
}

// URI를 파싱해서 filename과 cgiargs에 저장하는 함수, is_static을 반환, 1: 정적 콘텐츠, 0: 동적 콘텐츠
int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;  // ptr: URI에서 CGI 인자들을 구분하는 '?' 문자의 위치를 저장할 포인터

  if (!strstr(uri, "cgi-bin"))  // URI에 "cgi-bin"이 포함되어 있지 않다면, 정적 콘텐츠로 간주, cgi-bin: 동적 콘텐츠를 제공하는 디렉토리
  {
    strcpy(cgiargs, "");  // CGI 인자들을 빈 문자열로 초기화, 정적 콘텐츠는 CGI 인자가 없기 때문에 빈 문자열로 초기화
    strcpy(filename, ".");  // filename을 현재 디렉토리로 초기화, 정적 콘텐츠는 현재 디렉토리에서 파일을 찾기 때문에 현재 디렉토리로 초기화
    strcat(filename, uri);  // filename에 URI를 붙여서 완전한 파일 경로를 만듦, 예: "./home.html"

    if (uri[strlen(uri)-1] == '/')  // URI가 '/'로 끝난다면, 디렉토리를 요청한 것으로 간주, 예: "/home/"는 "/home/index.html"로 간주
    {
      strcat(filename, "home.html");  // filename에 "home.html"을 붙여서 완전한 파일 경로를 만듦, 예: "./home/index.html"
    }
    return 1; // 정적 콘텐츠로 간주
  }
  else  // URI에 "cgi-bin"이 포함되어 있다면, 동적 콘텐츠로 간주
  {
    ptr = index(uri, '?');  // URI에서 '?' 문자의 위치를 찾음, 예: "/cgi-bin/adder?123&456"에서 '?'의 위치를 찾음, 왜냐하면 CGI 인자들은 '?' 문자 뒤에 있기 때문
    if (ptr)  // '?' 문자가 있다면
    {
      strcpy(cgiargs, ptr+1); // CGI 인자들을 cgiargs에 저장, 예: "123&456"을 cgiargs에 저장
      *ptr = '\0';  // 그리고 '?' 문자를 '\0'으로 바꿔서 URI를 파일 경로로 만듦, 예: "/cgi-bin/adder"로 만듦
    }
    else  // '?' 문자가 없다면, CGI 인자들이 없다는 의미이므로 cgiargs를 빈 문자열로 초기화
    {
      strcpy(cgiargs, "");
    }

    strcpy(filename, ".");  // filename을 현재 디렉토리로 초기화, 동적 콘텐츠는 현재 디렉토리에서 파일을 찾기 때문에 현재 디렉토리로 초기화
    strcat(filename, uri);  // filename에 URI를 붙여서 완전한 파일 경로를 만듦, 예: "./cgi-bin/adder"
    return 0;  // 동적 콘텐츠로 간주
  }
}

void serve_static(int fd, char *filename, int filesize)
{
  int srcfd;  // srcfd: 요청된 파일을 열어서 얻은 파일 디스크립터
  char *srcp, filetype[MAXLINE], buf[MAXBUF];  // srcp: 요청된 파일의 내용을 메모리에 매핑한 포인터, filetype: 파일의 MIME 타입을 저장할 버퍼, buf: HTTP 응답 라인과 헤더를 저장할 버퍼

  get_filetype(filename, filetype);  // filename에서 파일의 MIME 타입을 구해서 filetype에 저장
  sprintf(buf, "HTTP/1.0 200 OK\r\n");  // HTTP 응답 라인 저장, 예: "HTTP/1.0 200 OK\r\n"
  Rio_writen(fd, buf, strlen(buf));  // HTTP 응답 라인 전송
  sprintf(buf, "Server: Tiny Web Server\r\n");  // HTTP 응답 헤더 저장, 예: "Server: Tiny Web Server\r\n"
  Rio_writen(fd, buf, strlen(buf));  // HTTP 응답 헤더 전송
  sprintf(buf, "Content-length: %d\r\n", filesize);  // HTTP 응답 헤더 저장, 예: "Content-length: 123\r\n"
  Rio_writen(fd, buf, strlen(buf));  // HTTP 응답 헤더 전송
  snprintf(buf, MAXBUF, "Content-type: %s\r\n\r\n", filetype);  // HTTP 응답 헤더 저장, 예: "Content-type: text/html\r\n\r\n", \r\n\r\n: 헤더와 본문을 구분하는 빈 줄
  Rio_writen(fd, buf, strlen(buf));  // HTTP 응답 헤더 전송

  srcfd = Open(filename, O_RDONLY, 0); // 요청된 파일을 읽기 전용으로 열어서 srcfd에 저장
  srcp = malloc(filesize); // 요청된 파일의 크기만큼 메모리를 할당해서 srcp에 저장
  if (srcp == NULL) // 메모리 할당에 실패했다면, 에러 반환
  {
    clienterror(fd, filename, "500", "Internal Server Error", "Tiny couldn't allocate memory");
    return;
  }
  Rio_readn(srcfd, srcp, filesize); // 요청된 파일의 내용을 srcp에 읽어옴
  Close(srcfd); // 파일 디스크립터 닫기

  Rio_writen(fd, srcp, filesize); // 요청된 파일의 내용을 클라이언트로 전송
  free(srcp); // 할당한 메모리 해제
}

void get_filetype(char *filename, char *filetype)
{
  if (strstr(filename, ".html"))  // filename에 ".html"이 포함되어 있다면, HTML 파일로 간주
    strcpy(filetype, "text/html");  // filetype에 "text/html" 저장
  else if (strstr(filename, ".gif"))  // filename에 ".gif"가 포함되어 있다면, GIF 파일로 간주
    strcpy(filetype, "image/gif");  // filetype에 "image/gif" 저장
  else if (strstr(filename, ".jpg"))  // filename에 ".jpg"가 포함되어 있다면, JPEG 파일로 간주
    strcpy(filetype, "image/jpeg");  // filetype에 "image/jpeg" 저장
  else if (strstr(filename, ".mpg"))
    strcpy(filetype, "video/mpeg");
  else if (strstr(filename, ".mpeg"))
    strcpy(filetype, "video/mpeg");
  else  // 그 외의 경우에는 일반 텍스트 파일로 간주
    strcpy(filetype, "text/plain");  // filetype에 "text/plain" 저장
}

void serve_dynamic(int fd, char *filename, char *cgiargs)
{
  char buf[MAXLINE], *emptylist[] = {NULL};  // buf: HTTP 응답 라인과 헤더를 저장할 버퍼, emptylist: CGI 프로그램에 전달할 인자 리스트, CGI 프로그램은 인자 리스트가 필요하지만 이 서버에서는 인자를 전달하지 않기 때문에 빈 리스트로 초기화

  sprintf(buf, "HTTP/1.0 200 OK\r\n");  // HTTP 응답 라인 저장, 예: "HTTP/1.0 200 OK\r\n"
  Rio_writen(fd, buf, strlen(buf));  // HTTP 응답 라인 전송
  sprintf(buf, "Server: Tiny Web Server\r\n");  // HTTP 응답 헤더 저장, 예: "Server: Tiny Web Server\r\n"
  Rio_writen(fd, buf, strlen(buf));  // HTTP 응답 헤더 전송

  if (Fork() == 0) // 자식 프로세스에서 CGI 프로그램 실행, 왜냐하면 CGI 프로그램이 클라이언트와 통신하기 위해서는 별도의 프로세스에서 실행되어야 하기 때문
  {
    setenv("QUERY_STRING", cgiargs, 1); // QUERY_STRING 환경 변수에 cgiargs를 설정, CGI 프로그램이 인자들을 QUERY_STRING 환경 변수에서 읽을 수 있도록 설정
    Dup2(fd, STDOUT_FILENO);  // 표준 출력을 클라이언트와의 연결 소켓으로 리다이렉션, CGI 프로그램이 표준 출력에 쓰는 내용이 클라이언트로 전송되도록 설정
    Execve(filename, emptylist, environ); // filename에 있는 CGI 프로그램을 실행, emptylist: CGI 프로그램에 전달할 인자 리스트, environ: 현재 프로세스의 환경 변수 리스트
   }
   Wait(NULL); // 부모 프로세스는 자식 프로세스가 종료될 때까지 기다림
}
