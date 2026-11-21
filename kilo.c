// Bibliotecas
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

// Constantes
#define BUFAN_INI {NULL, 0}
#define ESC '\x1b'
#define TAB_PARADA 8
#define TECLA_CTRL(tecla)  ((tecla) & 0x1f)
#define VERSAO "0.0.1"

enum editorTecla {
    SETA_ESQUERDA = 1000,
    SETA_DIREITA,
    SETA_BAIXO,
    SETA_CIMA,
    PAGE_UP,
    PAGE_DOWN,
    HOME,
    END,
    DELETE
};

// Dados
typedef struct editLinha {
    int tam;
    int renderTam;
    char *chars;
    char *render;
} editLinha;

struct editorConfiguracao {
    int cursorX, cursorY;
    int renderX;
    int linhaOffset;
    int colunaOffset;
    int telaLinhas;
    int telaColunas;
    int numLinhas;
    editLinha *linha;
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

int editorLerTecla() {
    int numLido;
    char caracterLido;
    while((numLido = read(STDIN_FILENO, &caracterLido, 1)) != 1) {
        if(numLido == -1 && errno != EAGAIN) erro("read");
    }

    if(caracterLido == '\x1b') {
        char sequencia[3];

        if(read(STDIN_FILENO, &sequencia[0], 1) != 1) return '\x1b';
        if(read(STDIN_FILENO, &sequencia[1], 1) != 1) return '\x1b';
    
        if(sequencia[0] == '[') {
            if(sequencia[1] >= '0' && sequencia[1] <= '9') {
                if(read(STDIN_FILENO, &sequencia[2], 1) != 1) return '\x1b';
                if(sequencia[2] == '~') {
                    switch(sequencia[1]) {
                        case '1':
                        case '7':
                            return HOME;
                        case '3': return DELETE;
                        case '4':
                        case '8':
                            return END;
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                    }
                }
            }
            else {
                switch(sequencia[1]) {
                    case 'A': return SETA_CIMA;
                    case 'B': return SETA_BAIXO;
                    case 'C': return SETA_DIREITA;
                    case 'D': return SETA_ESQUERDA;
                    case 'F': return END;
                    case 'H': return HOME;
                }
            }
        }
        else if(sequencia[0] == '0') {
            switch(sequencia[1]) {
                case 'F': return END;
                case 'H': return HOME;
            }
        }
        return '\x1b';
    }
    else return caracterLido;
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

// Operações com linhas
void editorAtualizarLinha(editLinha *linha)  {
    int tabs = 0;
    for(int i = 0; i < linha->tam; i++) {
        if(linha->chars[i] == '\t') tabs++;
    }

    free(linha->render);
    linha->render = malloc(linha->tam + tabs*(TAB_PARADA - 1) + 1);

    int index = 0;
    for(int i = 0; i < linha->tam; i++) {
        if(linha->chars[i] == '\t') {
            linha->render[index++] = ' ';
            while(index % TAB_PARADA != 0) linha->render[index++] = ' ';
        }
        else linha->render[index++] = linha->chars[i];
    }
    linha->render[index] = '\0';
    linha->renderTam = index;
}

void editorAnexarLinha(char *string, size_t tam) {
    editConfig.linha = realloc(editConfig.linha, sizeof(editLinha) * (editConfig.numLinhas + 1));

    int naLinha = editConfig.numLinhas; 
    editConfig.linha[naLinha].tam = tam;
    editConfig.linha[naLinha].chars = malloc(tam + 1);
    memcpy(editConfig.linha[naLinha].chars, string, tam);
    editConfig.linha[naLinha].chars[tam] = '\0';

    editConfig.linha[naLinha].renderTam = 0;
    editConfig.linha[naLinha].render = NULL;
    editorAtualizarLinha(&editConfig.linha[naLinha]);

    editConfig.numLinhas++;

}

int editorLinhaCxParaRx (editLinha *linha, int cx) {
    int rx = 0;
    for(int i = 0; i < cx; i++) {
        if(linha->chars[i] == '\t')  
            rx += (TAB_PARADA - 1) - (rx % TAB_PARADA);
        rx++;
    }
    return rx;
}

// Arquivos i/o
void editorAbrir(char *nomeArquivo) {
    FILE *abrirArq = fopen(nomeArquivo, "r");
    if(!abrirArq) erro("fopen");

    char *linha = NULL;
    size_t linhaCap = 0;
    ssize_t linhaTam;
    
    while((linhaTam = getline(&linha, &linhaCap, abrirArq)) != -1) {
        while(linhaTam > 0 && (linha[linhaTam - 1] == '\n' ||
                                linha[linhaTam - 1] == '\r')) 
            linhaTam--;
        editorAnexarLinha(linha, linhaTam);
    }
    free(linha);
    fclose(abrirArq);
}

// Output
void editorScroll() {
    editConfig.renderX = 0;
    if(editConfig.cursorX < editConfig.numLinhas) {
        editConfig.renderX = editorLinhaCxParaRx(&editConfig.linha[editConfig.cursorY], editConfig.cursorX);
    }

    if(editConfig.cursorY < editConfig.linhaOffset) editConfig.linhaOffset = editConfig.cursorY;
    if(editConfig.cursorY >= editConfig.linhaOffset + editConfig.telaLinhas) 
        editConfig.linhaOffset = editConfig.cursorY - editConfig.telaLinhas + 1;
    if(editConfig.renderX < editConfig.colunaOffset) editConfig.colunaOffset = editConfig.renderX;
    if(editConfig.renderX >= editConfig.colunaOffset + editConfig.telaColunas) 
        editConfig.colunaOffset = editConfig.renderX - editConfig.telaColunas + 1;
}

void editorDesenharLinhas(struct bufferAnexo *bufan) {
    for(int y = 0; y < editConfig.telaLinhas; y++) {
        int arqLinha = y + editConfig.linhaOffset;
        if(arqLinha >= editConfig.numLinhas) {
            if(editConfig.numLinhas == 0 && y == editConfig.telaLinhas/3) {
                char boasVindas[80];
                int boasVindasTam = snprintf(boasVindas, sizeof(boasVindas), "Kilo - Editor de texto -- versão %s", VERSAO);
                if(boasVindasTam > editConfig.telaColunas) boasVindasTam = editConfig.telaColunas;
                int padding = (editConfig.telaColunas - boasVindasTam)/2;
                if(padding) {
                    bufanAnexar(bufan, "||", 2);
                    padding--;
                }
                while(padding--) bufanAnexar(bufan, " ", 2);
                bufanAnexar(bufan, boasVindas, boasVindasTam);
            }
            else { bufanAnexar(bufan, "||", 2); }
        }
        else {
            int tam = editConfig.linha[arqLinha].renderTam - editConfig.colunaOffset;
            if(tam < 0) tam = 0;
            if(tam > editConfig.telaColunas) tam = editConfig.telaColunas;
            bufanAnexar(bufan, &editConfig.linha[arqLinha].render[editConfig.colunaOffset], tam);
        }

        bufanAnexar(bufan, "\x1b[K", 3);
        if(y < editConfig.telaLinhas - 1) {
            bufanAnexar(bufan, "\r\n", 3);
        }
    }
}

void editorRecarregarTela() {
    editorScroll();

    struct bufferAnexo bufan = BUFAN_INI;
    
    bufanAnexar(&bufan, "\x1b[?25l", 6);
    bufanAnexar(&bufan, "\x1b[H", 3);
    
    editorDesenharLinhas(&bufan);
    
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "\x1b[%d;%dH", (editConfig.cursorY - editConfig.linhaOffset) +1,
                                                    (editConfig.renderX - editConfig.colunaOffset) +1);
    bufanAnexar(&bufan, buffer, strlen(buffer));

    bufanAnexar(&bufan, "\x1b[?25h", 6);

    write(STDOUT_FILENO, bufan.buffer, bufan.tamBuffer);
    bufanFree(&bufan);
}

// Input
void editorMoverCursor(int tecla) {
    editLinha *linha = (editConfig.cursorY >= editConfig.numLinhas) ? NULL : &editConfig.linha[editConfig.cursorY];

    switch(tecla) {
        case SETA_ESQUERDA:
            if(editConfig.cursorX != 0) editConfig.cursorX--;
            else if(editConfig.cursorY > 0) {
                editConfig.cursorY--;
                editConfig.cursorX = editConfig.linha[editConfig.cursorY].tam;
            }
            break;
        case SETA_DIREITA:
            if(linha && editConfig.cursorX < linha->tam) editConfig.cursorX++;
            else if(linha && editConfig.cursorX == linha->tam) {
                editConfig.cursorY++;
                editConfig.cursorX = 0;
            }
            break;
        case SETA_CIMA:
            if(editConfig.cursorY != 0) editConfig.cursorY--;
            break;
        case SETA_BAIXO:
            if(editConfig.cursorY < editConfig.numLinhas) editConfig.cursorY++;
            break;
    }

    linha = (editConfig.cursorY >= editConfig.numLinhas) ? NULL : &editConfig.linha[editConfig.cursorY];
    int linhaTam = linha ? linha->tam : 0;
    if(editConfig.cursorX > linhaTam) editConfig.cursorX = linhaTam;
}

void editorProcessarTeclasPressionadas() {
    int caracter = editorLerTecla();

    switch(caracter) {
        case ESC:
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;

        case HOME:
            editConfig.cursorX = 0;
            break;
        case END:
            if(editConfig.cursorY < editConfig.numLinhas)
                editConfig.cursorX = editConfig.linha[editConfig.cursorY].tam;
            break;
        case PAGE_UP:
        case PAGE_DOWN:
            {
                if(caracter == PAGE_UP) editConfig.cursorY = editConfig.linhaOffset;
                else if(caracter == PAGE_DOWN) { 
                    editConfig.cursorY = editConfig.linhaOffset + editConfig.telaLinhas - 1;
                    if(editConfig.cursorY > editConfig.numLinhas) editConfig.cursorY = editConfig.numLinhas;
                }
                int vezes = editConfig.telaLinhas;
                while(vezes--) editorMoverCursor(caracter == PAGE_UP ? SETA_CIMA : SETA_BAIXO);
            }
            break;
                
        case SETA_CIMA:
        case SETA_ESQUERDA:
        case SETA_BAIXO:
        case SETA_DIREITA:
            editorMoverCursor(caracter);
            break;  
    }
}

// Inicialização
void inicializarEditor() {
    editConfig.cursorX = 0;
    editConfig.cursorY = 0;
    editConfig.renderX = 0;
    editConfig.linhaOffset = 0;
    editConfig.colunaOffset = 0;
    editConfig.numLinhas = 0;
    editConfig.linha = NULL;

    if(getTamanhoTela(&editConfig.telaLinhas, &editConfig.telaColunas) == -1) erro("getWindowsSize");
}

int main(int argContagem, char *argValor[]) {
    ligarRawMode();
    inicializarEditor();
    if(argContagem >= 2) editorAbrir(argValor[1]);

    while(1) {
        editorRecarregarTela();
        editorProcessarTeclasPressionadas();
    }

    return 0;
}