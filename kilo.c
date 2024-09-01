#include <unistd.h>

int main(int argc, char *argv[]){
  char c;
  // asking read() to 1 byte from the standard input into the variable c
  while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q');
  return 0;
}
