// Bibliotecas

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

// Dados

struct termios termiosEstadoOriginal;

// Terminal

void erro(const char *string) {
    perror(string);
    exit(1);
}

void desligarRawMode() {
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &termiosEstadoOriginal) == -1) erro("tcsetattr");
}

void ligarRawMode() {
    if(tcgetattr(STDIN_FILENO, &termiosEstadoOriginal) == -1) erro("tcgetattr");
    atexit(desligarRawMode);

    struct termios raw = termiosEstadoOriginal;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) erro("tcsetattr");
}

// Inicialização

int main() {
    ligarRawMode();

    while(1) {
        char byteLido = '\0';
        if(read(STDIN_FILENO, &byteLido, 1) == -1 && errno != EAGAIN) erro("read");
        if(iscntrl(byteLido)) printf("%d\r\n", byteLido);
        else printf("%d ('%c')\r\n", byteLido, byteLido);
        
        if(byteLido == 'q') break;
    }

    return 0;
}