// Bibliotecas
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

// Constantes
#define BUFAN_INI {NULL, 0}
#define Esc 27
#define TECLA_CTRL(tecla)  ((tecla) & 0x1f)

// Dados
struct editorConfiguracao {
    int telaLinhas;
    int telaColunas;
    struct termios termiosEstadoOriginal;
};

struct editorConfiguracao editConfig;

// Buffer de anexo
struct bufferAnexo {
    char *buffer;
    int tamBuffer;
};

void bufanAnexar(struct bufferAnexo *bufan, const char *string, int tam) {
    char *novo = realloc(bufan->buffer, bufan->tamBuffer+tam);

    if(novo == NULL) return;
    memcpy(&novo[bufan->tamBuffer], string, tam);
    bufan->buffer = novo;
    bufan->tamBuffer += tam;
}

void bufanFree(struct bufferAnexo *bufan) {
    free(bufan->buffer);
}

// Terminal
void erro(const char *string) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(string);
    exit(1);
}

void desligarRawMode() {
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &editConfig.termiosEstadoOriginal) == -1) erro("tcsetattr");
}

void ligarRawMode() {
    if(tcgetattr(STDIN_FILENO, &editConfig.termiosEstadoOriginal) == -1) erro("tcgetattr");
    atexit(desligarRawMode);

    struct termios raw = editConfig.termiosEstadoOriginal;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) erro("tcsetattr");
}

char editorLerTecla() {
    int numLido;
    char caracterLido;
    while((numLido = read(STDIN_FILENO, &caracterLido, 1)) != 1) {
        if(numLido == -1 && errno != EAGAIN) erro("read");
    }
    return caracterLido;
}

int getPosicaoCursor(int *linhas, int *colunas) {
    char buffer[32];
    unsigned int i = 0;

    if(write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

    while(i < sizeof(buffer) - 1) {
        if(read(STDIN_FILENO, &buffer[i], 1) != 1) break;
        if(buffer[i] == 'R') break;
        i++;
    }

    buffer[i] = '\0';

    if(buffer[0] != '\x1b' || buffer[1] != '[') return -1;
    if(sscanf(&buffer[2], "%d;%d", linhas, colunas) != 2) return -1;

    return 0;
}

int getTamanhoTela(int *linhas, int *colunas) {
    struct winsize tamTela;

    if(1 || ioctl(STDOUT_FILENO, TIOCGWINSZ, &tamTela) == -1 || tamTela.ws_col == 0) {
        if(write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
        return getPosicaoCursor(linhas, colunas);
    }
    else {
        *linhas = tamTela.ws_row;
        *colunas = tamTela.ws_col;
        return 0;
    }
}

// Output
void editorDesenharLinhas(struct bufferAnexo *bufan) {
    for(int y = 0; y < editConfig.telaLinhas; y++) {
        bufanAnexar(bufan, "||", 2);

        if(y < editConfig.telaLinhas - 1) {
            bufanAnexar(bufan, "\r\n", 3);
        }
    }
}

void editorRecarregarTela() {
    struct bufferAnexo bufan = BUFAN_INI;

    bufanAnexar(&bufan, "\x1b[2J", 4);
    bufanAnexar(&bufan, "\x1b[H", 3);

    editorDesenharLinhas(&bufan);

    bufanAnexar(&bufan, "\x1b[H", 3);

    write(STDOUT_FILENO, bufan.buffer, bufan.tamBuffer);
    bufanFree(&bufan);
}

// Input
void editorProcessarTeclasPressionadas() {
    char caracter = editorLerTecla();

    switch(caracter) {
        case Esc:
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;
    }
}

// Inicialização
void inicializarEditor() {
    if(getTamanhoTela(&editConfig.telaLinhas, &editConfig.telaColunas) == -1) erro("getWindowsSize");
}

int main() {
    ligarRawMode();
    inicializarEditor();

    while(1) {
        editorRecarregarTela();
        editorProcessarTeclasPressionadas();
    }

    return 0;
}