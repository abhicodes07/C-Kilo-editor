/*** includes ***/

#include <asm-generic/termbits.h>
#include <bits/termios_inlines.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

/*** defines ***/

#define CTRL_KEY(k) ((k) & 0x1f)

/*** data ***/
// struct to store original attributes of terminal
struct termios
    orig_termios; // struct to store the terminal attributes read by tcgetattr

/*** terminal ***/
// deals with low level terminal input

// Expection handling
// a function that prints an error message and  exits the program  .
void die(const char *s) {
  // clear screen on exit
  write(STDOUT_FILENO, "\x1b[J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);

  perror(s);
  exit(1);
}

// disabling raw mode at exit
void disableRawMode(void) {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) ==
      -1) { // sets terminal attributes to original one
    die("tcsetattr");
  }
}

// enabling raw mode
void enableRawMode(void) {
  if (tcgetattr(STDIN_FILENO, &orig_termios) == -1)
    die("tcsetattr"); // reads terminal attributes

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
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
    die("tcsetattr"); // apply terminal attributes
}

/* it deals with low-level terminal input.*/
char editorReadKey(void) {
  /* This function's  job is to wait for one keypress and return it.*/
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN)
      die("read");
  }
  return c;
}

/*** Input ***/
// deals with mapping keys to editor functions at a much higher level

void editorProcessKeypress(void) {
  // This function waits for a keypress, and then handles it.
  // Later it maps various ctrl key combinations and other special key to
  // different editor functions.

  char c = editorReadKey();

  switch (c) {
  case CTRL_KEY('q'):
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    exit(0);
    break;
  }
}

/*** output ***/

void editorDrawRows(void) {
  int y;
  for (y = 0; y < 24; y++) {
    write(STDOUT_FILENO, "~\r\n", 3);
  }
}

void editorRefreshScreen(void) {
  // \x1b stands for escape character of 27 bytes
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3); // H command to postion the cursor

  editorDrawRows();

  write(STDOUT_FILENO, "\x1b[H", 3);
}

/*** init ***/
int main(void) {
  enableRawMode();

  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  }
  return 0;
}
