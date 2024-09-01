#include <asm-generic/termbits.h>
#include <bits/termios_inlines.h>
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>

// ==================================================================
// disabling raw mode
void disableRawMode(void){

}

// ==================================================================
// enabling raw mode
void enableRawMode(void){
  struct termios raw; // struct to store the terminal attributes read by tcgetattr

  tcgetattr(STDIN_FILENO, &raw); // reads terminal attributes

  raw.c_lflag &= ~(ECHO); // reverses the bits of echo i.e., flipping bits 

  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw); // apply terminal attributes
}

// ==================================================================
// main
int main(void){
  enableRawMode();

  char c; // stores input from the keyboard
  // asking read() to 1 byte from the standard input into the variable c
  while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q');
  return 0;
}
