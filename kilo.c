/*** includes ***/

#include <asm-generic/ioctls.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

/*** defines ***/

#define CTRL_KEY(k) ((k) & 0x1f)

/*** data
 * ===========================================================================
 * ***/

// struct to store original attributes of terminal
struct termios
    orig_termios; // struct to store the terminal attributes read by tcgetattr

struct editorConfig { // set global state foe the terminal
  int screenrows;
  int screencols;
  struct termios orig_termios;
};

struct editorConfig E;

/*** terminal
 * ====================================================================== ***/
// deals with low level terminal input

void die(const char *s) {
  /*
   * a function that prints an error message and  exits the program .
   */

  // clear screen on exit
  write(STDOUT_FILENO, "\x1b[J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);

  perror(s);
  exit(1);
}

// disabling raw mode at exit
void disableRawMode(void) {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) ==
      -1) { // sets terminal attributes to original one
    die("tcsetattr");
  }
}

// enabling raw mode
void enableRawMode(void) {
  if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
    die("tcsetattr"); // reads terminal attributes

  atexit(disableRawMode); // register disableRawMode function to be called
  // automatically when the program exits

  struct termios raw = E.orig_termios; // struct that copies original terminal
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

/* get cursor position */
int getCursorPosition(int *rows, int *cols) {
  char buf[32];
  unsigned int i = 0;

  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
    return -1;

  while (i < sizeof(buf) - 1) {
    if (read(STDIN_FILENO, &buf[i], 1) != 1)
      break;
    if (buf[i] == 'R')
      break;
    i++;
  }
  buf[i] = '\0';

  if (buf[0] != '\x1b' || buf[1] != '[')
    return -1;
  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2)
    return -1;

  return -1;
}

int getWindowSize(int *rows, int *cols) {
  struct winsize ws;

  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
      return getCursorPosition(rows, cols);
    editorReadKey();
    return -1;
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

/*** append buffer ***/

struct abuf {
  char *b; // pointer to our buffer in memory
  int len; // length
};

#define ABUF_INIT {NULL, 0} // empty buffer

void abAppend(struct abuf *ab, const char *s, int len) {
  /* we ask realloc to gice us a block of memory that is the
   * size of the current string plus the size of the string
   * we are appending.
   */
  char *new = realloc(ab->b, ab->len + len);

  if (new == NULL)
    return;
  memcpy(&new[ab->len], s, len);
  ab->b = new;
  ab->len += len;
}

void abFree(
    struct abuf *ab) { // deallocates the dynamic memory used by an abuf.
  free(ab->b);
}

/*** Input
 * ===========================================================================
 * ***/
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

/*** output
 * ======================================================================== ***/

void editorDrawRows(struct abuf *ab) {
  int y;
  for (y = 0; y < E.screenrows; y++) {
    abAppend(ab, "~", 1);
    if (y < E.screenrows - 1) {
      abAppend(ab, "\r\n", 2);
    }
  }
}

void editorRefreshScreen(void) {
  // \x1b stands for escape character of 27 bytes
  struct abuf ab = ABUF_INIT;

  abAppend(&ab, "\x1b[2j", 4);
  abAppend(&ab, "\x1b[H", 3);

  editorDrawRows(&ab);

  abAppend(&ab, "\x1b[H", 3);

  write(STDOUT_FILENO, ab.b, ab.len);
  abFree(&ab);
}

/*** init
 * ======================================================================= ***/

void initEditor(void) {
  if (getWindowSize(&E.screenrows, &E.screencols) == -1)
    die("getWindowSize");
}

// main
int main(void) {
  enableRawMode();
  initEditor();

  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  }
  return 0;
}
