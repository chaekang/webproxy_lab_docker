/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "../csapp.h"

int main(void)
{
  char *buf, *p;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1 = 0, n2 = 0;

  /* Extract the two arguments */
  if ((buf = getenv("QUERY_STRING")) != NULL)  // QUERY_STRING 환경 변수에서 인자들을 읽어옴, CGI 프로그램이 인자들을 QUERY_STRING 환경 변수에서 읽을 수 있도록 설정했기 때문
  {
    p = strchr(buf, '&');  // buf에서 '&' 문자를 찾아서 p에 저장, '&' 문자는 두 인자 사이를 구분하는 문자이기 때문
    *p = '\0'; // '&' 문자를 '\0'으로 바꿔서 buf를 두 개의 문자열로 나눔, 예: "arg1=123&arg2=456" -> "arg1=123\0arg2=456"
    strcpy(arg1, buf); // arg1에 첫 번째 인자를 저장, 예: "arg1=123"을 arg1에 저장
    strcpy(arg2, p + 1);  // arg2에 두 번째 인자를 저장, 예: "arg2=456"을 arg2에 저장
    n1 = atoi(strchr(arg1, '=') + 1);
    n2 = atoi(strchr(arg2, '=') + 1);
  }

  /* Make the response body */
  sprintf(content, "QUERY_STRING=%s\r\n<p>", buf);
  sprintf(content + strlen(content), "Welcome to add.com: ");
  sprintf(content + strlen(content), "THE Internet addition portal.\r\n<p>");
  sprintf(content + strlen(content), "The answer is: %d + %d = %d\r\n<p>",
          n1, n2, n1 + n2);
  sprintf(content + strlen(content), "Thanks for visiting!\r\n");

  /* Generate the HTTP response */
  printf("Connection: close\r\n");
  printf("Content-length: %d\r\n", (int)strlen(content));
  printf("Content-type: text/html\r\n\r\n");
  printf("\r\n");
  printf("%s", content);
  fflush(stdout);

  exit(0);
}
/* $end adder */
