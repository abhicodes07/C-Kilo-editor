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

#define KILO_VERSION "0.0.1"

#define CTRL_KEY(k) ((k) & 0x1f)

enum editorKey {
  ARROW_LEFT = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  PAGE_UP,
  PAGE_DOWN
};

/*** data ***/
// struct to store original attributes of terminal
struct termios
    orig_termios; // struct to store the terminal attributes read by tcgetattr

struct editorConfig { // set global state foe the terminal
  int cx, cy;         // cursor positions
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
int editorReadKey(void) {
  /* This function's  job is to wait for one keypress and return it.*/
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN)
      die("read");
  }

  if (c == '\x1b') {
    char seq[3];
    if (read(STDIN_FILENO, &seq[0], 1) != 1)
      return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1)
      return '\x1b';

    if (seq[0] == '[') {
      if (seq[1] >= '0' && seq[1] <= '9') {
        if (read(STDIN_FILENO, &seq[2], 1) != 1)
          return '\x1b';
        if (seq[2] == '~') {
          switch (seq[1]) {
          case '5':
            return PAGE_UP;
          case '6':
            return PAGE_DOWN;
          }
        }
      } else {
        switch (seq[1]) {
        case 'A':
          return ARROW_UP;
        case 'B':
          return ARROW_DOWN;
        case 'C':
          return ARROW_RIGHT;
        case 'D':
          return ARROW_LEFT;
        }
      }
    }

    return '\x1b';
  } else {
    return c;
  }
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

void editorMoveCursor(int key) {
  /* Move cursor around with a, d, w, s
   */
  switch (key) {
  case ARROW_LEFT:
    if (E.cx != 0) {
      E.cx--;
    }
    break;
  case ARROW_RIGHT:
    if (E.cx != E.screencols - 1) {
      E.cx++;
    }
    break;
  case ARROW_UP:
    if (E.cy != 0) {
      E.cy--;
    }
    break;
  case ARROW_DOWN:
    if (E.cy != E.screencols - 1) {
      E.cy++;
    }
    break;
  }
}

void editorProcessKeypress(void) {
  // This function waits for a keypress, and then handles it.
  // Later it maps various ctrl key combinations and other special key to
  // different editor functions.

  int c = editorReadKey();

  switch (c) {
  case CTRL_KEY('q'):
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    exit(0);
    break;

  case PAGE_UP:
  case PAGE_DOWN: {
    int times = E.screencols;
    while (times--)
      editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
  } break;

  case ARROW_UP:
  case ARROW_DOWN:
  case ARROW_LEFT:
  case ARROW_RIGHT:
    editorMoveCursor(c);
    break;
  }
}

/*** output
 * ======================================================================== ***/

void editorDrawRows(struct abuf *ab) {
  int y;
  for (y = 0; y < E.screenrows; y++) {
    if (y == E.screenrows / 3) {
      char welcome[80];
      int welcomelen = snprintf(welcome, sizeof(welcome),
                                "Kilo Editor -- version %s", KILO_VERSION);
      if (welcomelen > E.screencols)
        welcomelen = E.screencols;
      int padding = (E.screencols - welcomelen) / 2;
      if (padding) {
        abAppend(ab, "~", 1);
        padding--;
      }
      while (padding--)
        abAppend(ab, " ", 1);
      abAppend(ab, welcome, welcomelen);
    } else {
      abAppend(ab, "~", 1);
    }

    abAppend(ab, "\x1b[K", 3);
    if (y < E.screenrows - 1) {
      abAppend(ab, "\r\n", 2);
    }
  }
}

void editorRefreshScreen(void) {
  // \x1b stands for escape character of 27 bytes
  struct abuf ab = ABUF_INIT;

  abAppend(&ab, "\x1b[?25l", 6);
  abAppend(&ab, "\x1b[H", 3);

  editorDrawRows(&ab);

  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
  abAppend(&ab, buf, strlen(buf));

  abAppend(&ab, "\x1b[?25h", 6);

  write(STDOUT_FILENO, ab.b, ab.len);
  abFree(&ab);
}

/*** init
 * ======================================================================= ***/

void initEditor(void) {
  /* Here, we initialise cx and cy with value 0,
   * as we want the cursor to start at the-left of the screen
   */
  E.cx = 0; // Horizontal coordinate of the cursor (column)
  E.cy = 0; // Vertical coordinate of the cursor (row)

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
