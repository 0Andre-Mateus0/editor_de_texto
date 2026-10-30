#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

struct termios termiosEstadoOriginal;

void desligarRawMode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &termiosEstadoOriginal);
}

void ligarRawMode() {
    tcgetattr(STDIN_FILENO, &termiosEstadoOriginal);
    atexit(desligarRawMode);

    struct termios raw = termiosEstadoOriginal;
    raw.c_lflag &= ~(ECHO | ICANON);
    
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void main() {
    ligarRawMode();

    char byteLido;
    while(read(STDIN_FILENO, &byteLido, 1) == 1 && byteLido !='q');
}