#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define TAB_STOP 8
#define QUIT_TIMES 2
#define CTRL_KEY(k) ((k) & 0x1f)
#define ABUF_INIT {NULL, 0}

enum editorKey{
    BACKSPACE = 127, 
    ARROW_LEFT = 1000, 
    ARROW_RIGHT, 
    ARROW_UP, 
    ARROW_DOWN, 
    DEL_KEY, 
    HOME_KEY, 
    END_KEY, 
    PAGE_UP, 
    PAGE_DOWN
};

typedef struct erow{
    int size;
    int rsize;

    char *chars;
    char *render;
} erow;

struct editorConfig{
    int cursorX, cursorY;
    int renderX;

    int rowOff, colOff;

    int screenRows, screenColumns;

    int numRows;
    erow *row;

    int dirty;

    char *filename;

    char statusmsg[80];
    time_t statusmsgTime;

    struct termios origTermios;
};

struct appendBuffer{
    char *buffer;
    int length;
};

struct editorConfig editor;

void initEditor();

void enableRawMode();
void disableRawMode();

int getCursorPosition(int *rows, int *cols);
int getWindowSize(int *rows, int *cols);

int editorRowcursorXToRx(erow *row, int cursorX);
void editorUpdateRow(erow *row);
void editorInsertRow(int at, char *s, size_t length);
void editorFreeRow(erow *row);
void editorDelRow(int at);
void editorRowInsertChar(erow *row, int at, int character);
void editorRowAppendString(erow *row, char *s, size_t length);
void editorRowDelChar(erow *row, int at);
void editorInsertChar(int character);
void editorInsertNewLine();
void editorDelChar();
char *editorRowsToString(int *buflen);
int editorWordCount();

void editorOpen(char *filename);
void editorSave();

int editorReadKey();
void editorProcessKeypress();

void editorScroll();
void editorDrawRows(struct appendBuffer *ab);
void editorDrawStatusBar(struct appendBuffer *ab);
void editorDrawMessageBar(struct appendBuffer *ab);
char *editorPrompt(char *prompt);
void editorSetStatusMessage(const char *fmt, ...);
void editorRefreshScreen();
void editorMoveCursor(int key);

void abAppend(struct appendBuffer *ab, const char *s, int length);
void abFree(struct appendBuffer *ab);

void die(const char *s);

int main(int argc, char *argv[]){
    enableRawMode();
    initEditor();
    if(argc >= 2){
        editorOpen(argv[1]);
    }

    editorSetStatusMessage("HELP: Ctrl-S  = save | Ctrl-Q = quit");

    while(1){
        editorRefreshScreen();
        editorProcessKeypress();
    }

    return 0;
}

void initEditor(){
    editor.cursorX = 0;
    editor.cursorY = 0;
    editor.renderX = 0;
    editor.rowOff = 0;
    editor.colOff = 0;
    editor.numRows = 0;
    editor.row = NULL;
    editor.dirty = 0;
    editor.filename = NULL;
    editor.statusmsg[0] = '\0';
    editor.statusmsgTime = 0;

    if(getWindowSize(&editor.screenRows, &editor.screenColumns) == -1){
        die("getWindowSize");
    }

    editor.screenRows -= 2;
}

void enableRawMode(){
    if(tcgetattr(STDIN_FILENO, &editor.origTermios) == -1){
        die("tcgetattr");
    }

    atexit(disableRawMode);

    struct termios raw = editor.origTermios;

    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON| IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1){
        die("tcsetattr");
    }
}

void disableRawMode(){
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &editor.origTermios) == -1){
        die("tcsetattr");
    }
}

int getCursorPosition(int *rows, int *cols){
    char buf[32];
    unsigned int i = 0;

    if(write(STDOUT_FILENO, "\x1b[6n", 4) != 4){
        return -1;
    }

    while(i < sizeof(buf) - 1){
        if(read(STDIN_FILENO, &buf[i], 1) != 1){
            break;
        }

        if(buf[i] == 'R'){
            break;
        }

        i++;
    }
    buf[i] = '\0';

    if((buf[0] != '\x1b') || (buf[1] != '[')){
        return -1;
    }
    
    if(sscanf(&buf[2], "%d;%d", rows, cols) != 2){
        return -1;
    }

    return 0;
}

int getWindowSize(int *rows, int *cols){
    struct winsize ws;

    if((ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1) || (ws.ws_col == 0)){
        if(write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12){
            return -1;
        }

        return getCursorPosition(rows, cols);
    }
    else{
        *cols = ws.ws_col;
        *rows = ws.ws_row;

        return 0;
    }
}

int editorRowcursorXToRx(erow *row, int cursorX){
    int renderX = 0;

    for(int i = 0; i < cursorX; i++){
        if(row->chars[i] == '\t'){
            renderX += (TAB_STOP - 1) - (renderX % TAB_STOP);
        }

        renderX++;
    }

    return renderX;
}

void editorUpdateRow(erow *row){
    int tabs = 0;
    for(int i = 0; i < row->size; i++){
        if(row->chars[i] == '\t'){
            tabs++;
        }
    }

    free(row->render);
    row->render = malloc(row->size + (tabs * (TAB_STOP - 1)) + 1);

    int idx = 0;
    for(int i = 0; i < row->size; i++){
        if(row->chars[i] == '\t'){
            row->render[idx++] = ' ';

            while((idx % TAB_STOP) != 0){
                row->render[idx++] = ' ';
            }
        }
        else{
            row->render[idx++] = row->chars[i];
        }
    }
    row->render[idx] = '\0';
    row->rsize = idx;
}

void editorInsertRow(int at, char *s, size_t length){
    if((at < 0) || (at > editor.numRows)){
        return;
    }

    editor.row = realloc(editor.row, sizeof(erow) * (editor.numRows + 1));

    memmove(&editor.row[at + 1], &editor.row[at], sizeof(erow) * (editor.numRows - at));

    editor.row[at].size = length;
    editor.row[at].chars = malloc(length + 1);
    
    memcpy(editor.row[at].chars, s, length);
    editor.row[at].chars[length] = '\0';

    editor.row[at].rsize = 0;
    editor.row[at].render = NULL;

    editorUpdateRow(&editor.row[at]);

    editor.numRows++;
    editor.dirty++;
}

void editorFreeRow(erow *row){
    free(row->render);
    free(row->chars);
}

void editorDelRow(int at){
    if((at < 0) || (at >= editor.numRows)){
        return;
    }

    editorFreeRow(&editor.row[at]);

    memmove(&editor.row[at], &editor.row[at + 1], sizeof(erow) * (editor.numRows - at - 1));

    editor.numRows--;
    editor.dirty++;
}

void editorRowInsertChar(erow *row, int at, int character){
    if((at < 0) || (at > row->size)){
        at = row->size;
    }

    row->chars = realloc(row->chars, row->size + 2);

    memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);

    row->size++;
    row->chars[at] = character;

    editorUpdateRow(row);

    editor.dirty++;
}

void editorRowAppendString(erow *row, char *s, size_t length){
    row->chars = realloc(row->chars, row->size + length + 1);

    memcpy(&row->chars[row->size], s, length);

    row->size += length;
    row->chars[row->size] = '\0';

    editorUpdateRow(row);

    editor.dirty++;
}

void editorRowDelChar(erow *row, int at){
    if((at < 0) || (at >= row->size)){
        return;
    }

    memmove(&row->chars[at], &row->chars[at+1], row->size - at);

    row->size--;

    editorUpdateRow(row);

    editor.dirty++;
}

void editorInsertChar(int character){
    if(editor.cursorY == editor.numRows){
        editorInsertRow(editor.numRows, "", 0);
    }

    editorRowInsertChar(&editor.row[editor.cursorY], editor.cursorX, character);
    editor.cursorX++;
}

void editorInsertNewLine(){
    if(editor.cursorX == 0){
        editorInsertRow(editor.cursorY, "", 0);
    }
    else{
        erow *row = &editor.row[editor.cursorY];

        editorInsertRow(editor.cursorY + 1, &row->chars[editor.cursorX], row->size - editor.cursorX);

        row = &editor.row[editor.cursorY];
        row->size = editor.cursorX;
        row->chars[row->size] = '\0';
        editorUpdateRow(row);
    }

    editor.cursorY++;
    editor.cursorX = 0;
}

void editorDelChar(){
    if(editor.cursorY == editor.numRows){
        return;
    }

    if((editor.cursorX == 0) && (editor.cursorY == 0)){
        return;
    }

    erow *row = &editor.row[editor.cursorY];

    if(editor.cursorX > 0){
        editorRowDelChar(row, editor.cursorX - 1);
        editor.cursorX--;
    }
    else{
        editor.cursorX = editor.row[editor.cursorY - 1].size;

        editorRowAppendString(&editor.row[editor.cursorY - 1], row->chars, row->size);
        editorDelRow(editor.cursorY);

        editor.cursorY--;
    }
}

char *editorRowsToString(int *buflen){
    int totlen = 0;

    for(int i = 0; i < editor.numRows; i++){
        totlen += editor.row[i].size + 1;
    }

    *buflen = totlen;

    char *buf = malloc(totlen);
    char *p = buf;

    for(int i = 0; i < editor.numRows; i++){
        memcpy(p, editor.row[i].chars, editor.row[i].size);
        p += editor.row[i].size;
        *p = '\n';
        p++;
    }

    return buf;
}

int editorWordCount(){
    int count = 0;
    int isInWord = 0;

    for(int i = 0; i < editor.numRows; i++){
        erow *row = &editor.row[i];

        for(int j = 0; j < row->size; j++){
            int character = row->chars[j];

            if(isspace(character)){
                isInWord = 0;
            }
            else if(!isInWord){
                count++;
                isInWord = 1;
            }
        }
    }

    return count;
}

void editorOpen(char *filename){
    free(editor.filename);
    editor.filename = strdup(filename);

    FILE *fp = fopen(filename, "r");

    if(!fp){
        die("fopen");
    }

    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;

    while((linelen = getline(&line, &linecap, fp)) != -1){
        while((linelen > 0) && ((line[linelen - 1] == '\n') || (line[linelen - 1] == '\r'))){
            linelen--;
        }

        editorInsertRow(editor.numRows, line, linelen);
    }

    free(line);
    fclose(fp);

    editor.dirty = 0;
}

void editorSave(){
    if(editor.filename == NULL){
        editor.filename = editorPrompt("Save as: %s");

        if(editor.filename == NULL){
            editorSetStatusMessage("Save aborted");

            return;
        }
    }

    int length;
    char *buf = editorRowsToString(&length);

    int fd = open(editor.filename, O_RDWR | O_CREAT, 0644);

    if(fd != -1){
        if(ftruncate(fd, length) != -1){
            if(write(fd, buf, length) == length){
                close(fd);
                free(buf);

                editor.dirty = 0;

                editorSetStatusMessage("%d bytes written to disk", length);

                return;
            }
        }

        close(fd);
    }

    free(buf);

    editorSetStatusMessage("Can't save! I/O error: %s", strerror(errno));
}

int editorReadKey(){
    int nread;
    char character;

    while((nread = read(STDIN_FILENO, &character, 1)) != 1){
        if((nread == -1) && (errno != EAGAIN)){
            die("read");
        }
    }

    if(character == '\x1b'){
        char seq[3];

        if(read(STDIN_FILENO, &seq[0], 1) != 1){
            return '\x1b';
        }

        if(read(STDIN_FILENO, &seq[1], 1) != 1){
            return '\x1b';
        }

        if(seq[0] == '['){
            if(seq[1] >= '0' && seq[1] <= '9'){
                if(read(STDIN_FILENO, &seq[2], 1) != 1){
                    return '\x1b';
                }

                if(seq[2] == '~'){
                    switch(seq[1]){
                        case '1': return HOME_KEY;
                        case '3': return DEL_KEY;
                        case '4': return END_KEY;
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                        case '7': return HOME_KEY;
                        case '8': return END_KEY;
                    }
                }
            }
            else{
                switch(seq[1]){
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                    case 'H': return HOME_KEY;
                    case 'F': return END_KEY;
                }
            }
        }
        else if(seq[0] == 'O'){
            switch(seq[1]){
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
            }
        }

        return '\x1b';
    }
    else{
        return character;
    }
}

void editorProcessKeypress(){
    static int quitTimes = QUIT_TIMES;

    int character = editorReadKey();

    switch(character){
        case '\r':
            editorInsertNewLine();
            break;

        case CTRL_KEY('q'):
            if(editor.dirty && quitTimes > 0){
                editorSetStatusMessage("Unsaved changes! Press Ctrl-S to save the file or Ctrl-Q %d more times to quit.", quitTimes);
                quitTimes--;

                return;
            }
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;

        case CTRL_KEY('s'):
            editorSave();
            break;

        case HOME_KEY:
            editor.cursorX = 0;
            break;

        case END_KEY:
            if(editor.cursorY < editor.numRows){
                editor.cursorX = editor.row[editor.cursorY].size;
            }
            break;

        case BACKSPACE:
        case CTRL_KEY('h'):
        case DEL_KEY:
            if(character == DEL_KEY){
                editorMoveCursor(ARROW_RIGHT);
            }
            editorDelChar();
            break;

        case PAGE_UP:
        case PAGE_DOWN:
            {
                if(character == PAGE_UP){
                    editor.cursorY = editor.rowOff;
                }
                else if(character == PAGE_DOWN){
                    editor.cursorY = editor.rowOff + editor.screenRows - 1;

                    if(editor.cursorY > editor.numRows){
                        editor.cursorY = editor.numRows;
                    }
                }

                int times = editor.screenRows;

                while(times--){
                    editorMoveCursor(character == PAGE_UP ? ARROW_UP : ARROW_DOWN);
                }
            }
            break;
        
        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            editorMoveCursor(character);
            break;

        case CTRL_KEY('l'):
        case '\x1b':
            break;

        default:
            editorInsertChar(character);
            break;
    }

    quitTimes = QUIT_TIMES;
}

void editorScroll(){
    editor.renderX = 0;
    if(editor.cursorY < editor.numRows){
        editor.renderX = editorRowcursorXToRx(&editor.row[editor.cursorY], editor.cursorX);
    }

    if(editor.cursorY < editor.rowOff){
        editor.rowOff = editor.cursorY;
    }

    if(editor.cursorY >= editor.rowOff + editor.screenRows){
        editor.rowOff = editor.cursorY - editor.screenRows + 1;
    }

    if(editor.renderX < editor.colOff){
        editor.colOff = editor.renderX;
    }

    if(editor.renderX >= editor.colOff + editor.screenColumns){
        editor.colOff = editor.renderX - editor.screenColumns + 1;
    }
}

void editorDrawRows(struct appendBuffer *ab){
    for(int i = 0; i < editor.screenRows; i++){
        int filerow = i + editor.rowOff;

        if(filerow < editor.numRows){
            int length = editor.row[filerow].rsize - editor.colOff;

            if(length < 0){
                length = 0;
            }

            if(length > editor.screenColumns){
                length = editor.screenColumns;
            }

            abAppend(ab, &editor.row[filerow].render[editor.colOff], length);
        }

        abAppend(ab, "\x1b[K", 3);

        abAppend(ab, "\r\n", 2);
    }
}

void editorDrawStatusBar(struct appendBuffer *ab){
    abAppend(ab, "\x1b[7m", 4);

    int wordCount = editorWordCount();

    char status[80], rstatus[80];
    
    int length = snprintf(status, sizeof(status), "Line: %d, Column: %d, Words: %d", editor.cursorY + 1, editor.renderX + 1, wordCount);
    int renderLength = snprintf(rstatus, sizeof(rstatus), "%.20s - %d lines %s", editor.filename ? editor.filename : "[No Name]", editor.numRows, editor.dirty ? "(modified)" : "");

    if(length > editor.screenColumns){
        length = editor.screenColumns;
    }

    abAppend(ab, status, length);

    while(length < editor.screenColumns){
        if((editor.screenColumns - length) == renderLength){
            abAppend(ab, rstatus, renderLength);
            break;
        }
        else{
            abAppend(ab, " ", 1);
            length++;
        }
    }

    abAppend(ab, "\x1b[m", 3);

    abAppend(ab, "\r\n", 2);
}

void editorDrawMessageBar(struct appendBuffer *ab){
    abAppend(ab, "\x1b[K", 3);

    int msglen = strlen(editor.statusmsg);

    if(msglen > editor.screenColumns){
        msglen = editor.screenColumns;
    }

    if(msglen && ((time(NULL) - editor.statusmsgTime) < 5)){
        abAppend(ab, editor.statusmsg, msglen);
    }
}

char *editorPrompt(char *prompt){
    size_t bufsize = 128;
    char *buf = malloc(bufsize);

    size_t buflen = 0;
    buf[0] = '\0';

    while(1){
        editorSetStatusMessage(prompt, buf);
        editorRefreshScreen();

        int character = editorReadKey();

        if((character == DEL_KEY) || (character == CTRL_KEY('h')) || (character == BACKSPACE)){
            if(buflen != 0){
                buf[--buflen] = '\0';
            }
        }
        else if(character == '\x1b'){
            editorSetStatusMessage("");
            
            free(buf);

            return NULL;
        }
        else if(character == '\r'){
            if(buflen != 0){
                editorSetStatusMessage("");
                
                return buf;
            }
        }
        else if((!iscntrl(character)) && (character < 128)){
            if(buflen == (bufsize - 1)){
                bufsize *= 2;
                buf = realloc(buf, bufsize);
            }

            buf[buflen++] = character;
            buf[buflen] = '\0';
        }
    }
}

void editorSetStatusMessage(const char *fmt, ...){
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(editor.statusmsg, sizeof(editor.statusmsg), fmt, ap);
    va_end(ap);

    editor.statusmsgTime = time(NULL);
}

void editorRefreshScreen(){
    editorScroll();

    struct appendBuffer ab = ABUF_INIT;

    abAppend(&ab, "\x1b[?25l", 6);
    abAppend(&ab, "\x1b[H", 3);

    editorDrawRows(&ab);
    editorDrawStatusBar(&ab);
    editorDrawMessageBar(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (editor.cursorY - editor.rowOff) + 1, (editor.renderX - editor.colOff) + 1);
    abAppend(&ab, buf, strlen(buf));

    abAppend(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.buffer, ab.length);

    abFree(&ab);
}

void editorMoveCursor(int key){
    erow *row = (editor.cursorY >= editor.numRows) ? NULL : &editor.row[editor.cursorY];

    switch(key){
        case ARROW_LEFT:
            if(editor.cursorX != 0){
                editor.cursorX--;
            }
            else if(editor.cursorY > 0){
                editor.cursorY--;
                editor.cursorX = editor.row[editor.cursorY].size;
            }
            break;
        case ARROW_RIGHT:
            if(row && editor.cursorX < row->size){
                editor.cursorX++;
            }
            else if(row && editor.cursorX == row->size){
                editor.cursorY++;
                editor.cursorX = 0;
            }
            break;
        case ARROW_UP:
            if(editor.cursorY != 0){
                editor.cursorY--;
            }
            break;
        case ARROW_DOWN:
            if(editor.cursorY < editor.numRows){
                editor.cursorY++;
            }
            break;
    }

    row = (editor.cursorY >= editor.numRows) ? NULL : &editor.row[editor.cursorY];
    
    int rowlen = row ? row->size : 0;

    if(editor.cursorX > rowlen){
        editor.cursorX = rowlen;
    }
}

void abAppend(struct appendBuffer *ab, const char *s, int length){
    char *new = realloc(ab->buffer, ab->length + length);

    if(new == NULL){
        return;
    }

    memcpy(&new[ab->length], s, length);
    
    ab->buffer = new;
    ab->length += length;
}

void abFree(struct appendBuffer *ab){
    free(ab->buffer);
}

void die(const char *s){
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(s);
    exit(1);
}