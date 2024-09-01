#include <asm-generic/termbits.h>
#include <bits/termios_inlines.h>
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>

// struct to store original attributes of terminal
struct termios orig_termios; // struct to store the terminal attributes read by tcgetattr

// ==================================================================
// disabling raw mode at exit 
void disableRawMode(void){
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios); // sets terminal attributes to original one
}

// ==================================================================
// enabling raw mode
void enableRawMode(void){
  tcgetattr(STDIN_FILENO, &orig_termios); // reads terminal attributes
  atexit(disableRawMode); // register disableRawMode function to be called
  // automatically when the program exits

  struct termios raw = orig_termios; // struct that copies original terminal
  // attributes to enable raw mode
  raw.c_lflag &= ~(ECHO | ICANON); // reverses the bits of echo i.e., flipping bits 

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
