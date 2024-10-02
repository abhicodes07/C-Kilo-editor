#include <asm-generic/termbits.h>
#include <bits/termios_inlines.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

// struct to store original attributes of terminal
struct termios
    orig_termios; // struct to store the terminal attributes read by tcgetattr

// =========================================================================================================
// disabling raw mode at exit
void disableRawMode(void) {
  tcsetattr(STDIN_FILENO, TCSAFLUSH,
            &orig_termios); // sets terminal attributes to original one
}

// =========================================================================================================
// enabling raw mode
void enableRawMode(void) {
  tcgetattr(STDIN_FILENO, &orig_termios); // reads terminal attributes
  atexit(disableRawMode); // register disableRawMode function to be called
  // automatically when the program exits

  struct termios raw = orig_termios; // struct that copies original terminal
  // attributes to enable raw mode
  raw.c_iflag &=
      ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON); // disable ctrl-S and ctrl-Q
  raw.c_oflag &= ~(OPOST); // OPOST Turns off all output processing features
  raw.c_cflag |= (CS8);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN |
                   ISIG); // reverses the bits of echo i.e., flipping bits
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw); // apply terminal attributes
}

// =========================================================================================================
// main
int main(void) {
  enableRawMode();

  while (1) {
    char c = '\0'; // stores input from the keyboard
    // asking read() to 1 byte from the standard input into the variable c
    read(STDIN_FILENO, &c, 1);
    if (iscntrl(c)) { // tests whether a character is a control character
                      // (non-printable characters)
      printf("%d\r\n", c);
    } else {
      printf("%d ('%c')\r\n", c, c);
    }
    if (c == 'q')
      break;
  }
  return 0;
}
