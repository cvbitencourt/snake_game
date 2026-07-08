#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>

/* ---------------------- Constantes fixas da tela ------------------ 
#define LARGURA_TELA     800
#define ALTURA_TELA      600
#define TAMANHO_INICIAL   3
#define PONTUACAO_VITORIA 20
#define ARQUIVO_PLACAR   "placar.txt"

/* Capacidades maximas (o pior caso e o nivel DIFICIL, com grade fina) */
#define MAX_TAMANHO           5000   /* segmentos possiveis da cobra   */
#define MAX_OBSTACULOS_CAP    30
#define MAX_FRUTAS_RUINS_CAP  8

#define SEGMENTOS_REMOVIDOS   2      /* quantos segmentos cada fruta marrom tira da cobra */
#define INTERVALO_FRUTA_RUIM  3.0f   /* segundos entre tentativas de gerar uma nova fruta marrom */
#define DURACAO_FRUTA_RUIM    5.0f   /* segundos que cada fruta marrom fica na tela */
#define DURACAO_COMIDA        8.0f   /* segundos ate a comida boa sumir e reaparecer em outro lugar */

/* ---------------------- Tipos de dados --------------------------- */
typedef enum { MENU, JOGANDO, GAME_OVER, VITORIA } EstadoJogo;
typedef enum { CIMA, BAIXO, ESQUERDA, DIREITA } Direcao;
typedef enum { FACIL, MEDIO, DIFICIL } Dificuldade;

typedef struct {
    int x, y;
} Posicao;

typedef struct {
    Posicao corpo[MAX_TAMANHO];
    int tamanho;
    Direcao direcao;
} Cobra;

typedef struct {
    Posicao pos;
    bool ativa;
    float tempoAtiva;   /* ha quanto tempo esta na tela */
} FrutaRuim;

typedef struct {
    Cobra cobra;
    Posicao comida;

    Posicao obstaculos[MAX_OBSTACULOS_CAP];
    int numObstaculos;

    FrutaRuim frutasRuins[MAX_FRUTAS_RUINS_CAP];
    float cronSpawnFrutaRuim;   /* contador ate tentar gerar a proxima fruta marrom */
    float cronComida;           /* contador ate a comida boa sumir e trocar de lugar */

    int pontuacao;
    int recorde;
    EstadoJogo estado;
    bool pausado;
    float tempoAcumulado;
} Jogo;

/* Parametros que mudam de acordo com o nivel escolhido no menu */
typedef struct {
    int tamanhoCelula;
    int numObstaculos;
    int maxFrutasRuins;
    float intervaloMovimento;
    const char *nome;
} ConfigDificuldade;

static const ConfigDificuldade CONFIGS_DIFICULDADE[3] = {
    /* tamanhoCelula, numObstaculos, maxFrutasRuins, intervaloMovimento, nome */
    { 40,  0, 0, 0.17f, "FACIL"   },
    { 20,  8, 2, 0.13f, "MEDIO"   },
    { 10, 18, 5, 0.08f, "DIFICIL" },
};

/* ---------------------- Estado global de configuracao --------------
   A grade (colunas/linhas), o tamanho da celula, a velocidade e a
   quantidade de obstaculos/frutas ruins dependem do nivel escolhido,
   entao viram variaveis (nao mais #define fixos) definidas quando o
   jogador escolhe a dificuldade no menu. */
static Dificuldade g_dificuldade       = MEDIO;
static int         g_tamanhoCelula     = 20;
static int         g_colunas           = LARGURA_TELA / 20;
static int         g_linhas            = ALTURA_TELA  / 20;
static float       g_intervaloMovimento = 0.13f;
static int         g_numObstaculosAlvo  = 8;
static int         g_maxFrutasRuins     = 2;

/* ---------------------- Texturas (PNG opcionais) -------------------
   Se os arquivos nao existirem, o jogo continua funcionando normalmente
   desenhando retangulos coloridos no lugar (ver DesenharEntidade). */
static Texture2D texturaObstaculo = { 0 };
static Texture2D texturaComida    = { 0 };
static Texture2D texturaFrutaRuim = { 0 };

/* ---------------------- Prototipos -------------------------------- */
void InitGame(Jogo *jogo);
void UpdateGame(Jogo *jogo);
void DrawGame(Jogo *jogo);

static void AplicarConfiguracao(Dificuldade d);
static void ConfigurarNovaPartida(Jogo *jogo);

static void GerarObstaculos(Jogo *jogo);
static void GerarComida(Jogo *jogo);
static void GerarFrutaRuim(Jogo *jogo, int indice);
static bool PosicaoOcupada(const Jogo *jogo, Posicao pos, bool considerarComida, bool considerarFrutasRuins);
static bool DentroDaZonaSegura(Posicao pos);
static bool PosicaoOcupadaPeloCorpo(const Cobra *cobra, Posicao pos);
static bool ColisaoComParede(Posicao pos);
static bool ColisaoComCorpo(const Cobra *cobra);
static bool ColisaoComObstaculo(const Jogo *jogo, Posicao pos);
static int  CarregarRecorde(void);
static void SalvarPontuacao(int pontuacao, int recordeAtual);
static void DesenharEntidade(Texture2D textura, Posicao pos, Color corFallback);

/* ===================================================================
   main
   =================================================================== */
int main(void) {
    InitWindow(LARGURA_TELA, ALTURA_TELA, "Snake");

    /* icone da janela/barra de tarefas (opcional): precisa de um icon.png
       na mesma pasta do executavel, formato quadrado (ex.: 64x64). */
    Image icone = LoadImage("icon.png");
    if (icone.data != NULL) {
        SetWindowIcon(icone);
        UnloadImage(icone);
    }

    /* texturas opcionais dos elementos do jogo (mesma pasta do executavel).
       Se um arquivo nao existir, LoadTexture retorna uma textura vazia
       (id == 0) e o jogo desenha um retangulo colorido no lugar. */
    texturaObstaculo = LoadTexture("obstaculo.png");
    texturaComida    = LoadTexture("comida.png");
    texturaFrutaRuim = LoadTexture("fruta_ruim.png");

    SetTargetFPS(60);

    Jogo jogo;
    InitGame(&jogo);

    while (!WindowShouldClose()) {
        UpdateGame(&jogo);
        DrawGame(&jogo);
    }

    UnloadTexture(texturaObstaculo);
    UnloadTexture(texturaComida);
    UnloadTexture(texturaFrutaRuim);

    CloseWindow();
    return 0;
}

/* ===================================================================
   InitGame: chamada uma unica vez, ao ligar o jogo.
   Carrega o recorde do arquivo e deixa tudo pronto no MENU, esperando
   o jogador escolher a dificuldade.
   =================================================================== */
void InitGame(Jogo *jogo) {
    AplicarConfiguracao(MEDIO);   /* configuracao padrao ate o jogador escolher */
    jogo->recorde = CarregarRecorde();
    ConfigurarNovaPartida(jogo);
    jogo->estado = MENU;
}

/* Aplica os parametros (tamanho de celula, obstaculos, velocidade...)
   do nivel escolhido as variaveis globais de configuracao. */
static void AplicarConfiguracao(Dificuldade d) {
    const ConfigDificuldade *cfg = &CONFIGS_DIFICULDADE[d];
    g_dificuldade        = d;
    g_tamanhoCelula       = cfg->tamanhoCelula;
    g_colunas             = LARGURA_TELA / cfg->tamanhoCelula;
    g_linhas              = ALTURA_TELA  / cfg->tamanhoCelula;
    g_intervaloMovimento  = cfg->intervaloMovimento;
    g_numObstaculosAlvo   = cfg->numObstaculos;
    g_maxFrutasRuins      = cfg->maxFrutasRuins;
}

/* Reposiciona a cobra, zera pontuacao/timers e gera obstaculos/comida
   de acordo com a configuracao atual (g_colunas, g_linhas, etc.). Usada
   tanto no boot do jogo quanto toda vez que uma dificuldade e escolhida. */
static void ConfigurarNovaPartida(Jogo *jogo) {
    int centroX = g_colunas / 2;
    int centroY = g_linhas / 2;

    jogo->cobra.tamanho = TAMANHO_INICIAL;
    jogo->cobra.direcao = DIREITA;
    for (int i = 0; i < TAMANHO_INICIAL; i++) {
        jogo->cobra.corpo[i].x = centroX - i;
        jogo->cobra.corpo[i].y = centroY;
    }

    jogo->pontuacao = 0;
    jogo->pausado = false;
    jogo->tempoAcumulado = 0.0f;

    jogo->cronSpawnFrutaRuim = 0.0f;
    jogo->cronComida = 0.0f;
    for (int i = 0; i < MAX_FRUTAS_RUINS_CAP; i++) {
        jogo->frutasRuins[i].ativa = false;
        jogo->frutasRuins[i].tempoAtiva = 0.0f;
    }

    GerarObstaculos(jogo);
    GerarComida(jogo);
}

/* ===================================================================
   UpdateGame: entrada, movimento, colisoes, transicoes de estado
   =================================================================== */
void UpdateGame(Jogo *jogo) {

    if (jogo->estado == MENU) {
        if (IsKeyPressed(KEY_ONE)) {
            AplicarConfiguracao(FACIL);
            ConfigurarNovaPartida(jogo);
            jogo->estado = JOGANDO;
        } else if (IsKeyPressed(KEY_TWO)) {
            AplicarConfiguracao(MEDIO);
            ConfigurarNovaPartida(jogo);
            jogo->estado = JOGANDO;
        } else if (IsKeyPressed(KEY_THREE)) {
            AplicarConfiguracao(DIFICIL);
            ConfigurarNovaPartida(jogo);
            jogo->estado = JOGANDO;
        }
        return;
    }

    if (jogo->estado == GAME_OVER || jogo->estado == VITORIA) {
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
            jogo->estado = MENU; /* volta ao menu para escolher a dificuldade de novo */
        }
        return;
    }

    /* ---- estado JOGANDO ---- */

    if (IsKeyPressed(KEY_P)) {
        jogo->pausado = !jogo->pausado;
    }
    if (jogo->pausado) {
        return; /* congela tudo: nenhum timer ou movimento avanca */
    }

    /* leitura de direcao (impede inversao de 180 graus) */
    if ((IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) && jogo->cobra.direcao != BAIXO)
        jogo->cobra.direcao = CIMA;
    else if ((IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) && jogo->cobra.direcao != CIMA)
        jogo->cobra.direcao = BAIXO;
    else if ((IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) && jogo->cobra.direcao != DIREITA)
        jogo->cobra.direcao = ESQUERDA;
    else if ((IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) && jogo->cobra.direcao != ESQUERDA)
        jogo->cobra.direcao = DIREITA;

    /* comida boa some sozinha depois de um tempo e reaparece em outro lugar */
    jogo->cronComida += GetFrameTime();
    if (jogo->cronComida >= DURACAO_COMIDA) {
        GerarComida(jogo);
        jogo->cronComida = 0.0f;
    }

    /* ciclo das frutas marrons: roda a cada frame, independente do passo da cobra */
    jogo->cronSpawnFrutaRuim += GetFrameTime();
    if (jogo->cronSpawnFrutaRuim >= INTERVALO_FRUTA_RUIM) {
        jogo->cronSpawnFrutaRuim = 0.0f;
        /* tenta ocupar um slot livre, respeitando o limite do nivel atual */
        for (int i = 0; i < g_maxFrutasRuins; i++) {
            if (!jogo->frutasRuins[i].ativa) {
                GerarFrutaRuim(jogo, i);
                jogo->frutasRuins[i].ativa = true;
                jogo->frutasRuins[i].tempoAtiva = 0.0f;
                break; /* gera uma por vez, mesmo que haja mais de um slot livre */
            }
        }
    }
    for (int i = 0; i < MAX_FRUTAS_RUINS_CAP; i++) {
        if (jogo->frutasRuins[i].ativa) {
            jogo->frutasRuins[i].tempoAtiva += GetFrameTime();
            if (jogo->frutasRuins[i].tempoAtiva >= DURACAO_FRUTA_RUIM) {
                jogo->frutasRuins[i].ativa = false; /* tempo esgotou: some sem efeito */
            }
        }
    }

    /* movimento em grid, controlado por tempo (nao por frame) */
    jogo->tempoAcumulado += GetFrameTime();
    if (jogo->tempoAcumulado < g_intervaloMovimento) {
        return; /* ainda nao e hora de mover */
    }
    jogo->tempoAcumulado = 0.0f;

    Cobra *cobra = &jogo->cobra;
    Posicao novaCabeca = cobra->corpo[0];

    switch (cobra->direcao) {
        case CIMA:      novaCabeca.y -= 1; break;
        case BAIXO:     novaCabeca.y += 1; break;
        case ESQUERDA:  novaCabeca.x -= 1; break;
        case DIREITA:   novaCabeca.x += 1; break;
    }

    /* colisao com parede ou com obstaculo = game over */
    if (ColisaoComParede(novaCabeca) || ColisaoComObstaculo(jogo, novaCabeca)) {
        jogo->estado = GAME_OVER;
        SalvarPontuacao(jogo->pontuacao, jogo->recorde);
        if (jogo->pontuacao > jogo->recorde) jogo->recorde = jogo->pontuacao;
        return;
    }

    bool comeu = (novaCabeca.x == jogo->comida.x && novaCabeca.y == jogo->comida.y);

    int indiceFrutaRuimComida = -1;
    for (int i = 0; i < MAX_FRUTAS_RUINS_CAP; i++) {
        if (jogo->frutasRuins[i].ativa &&
            jogo->frutasRuins[i].pos.x == novaCabeca.x &&
            jogo->frutasRuins[i].pos.y == novaCabeca.y) {
            indiceFrutaRuimComida = i;
            break;
        }
    }
    bool comeuFrutaRuim = (indiceFrutaRuimComida != -1);

    /* desloca o corpo: cada segmento assume a posicao do anterior */
    int limite = comeu ? cobra->tamanho : cobra->tamanho - 1;
    for (int i = limite; i > 0; i--) {
        cobra->corpo[i] = cobra->corpo[i - 1];
    }
    cobra->corpo[0] = novaCabeca;

    if (comeu) {
        if (cobra->tamanho < MAX_TAMANHO) cobra->tamanho++;
        jogo->pontuacao++;
        GerarComida(jogo);
        jogo->cronComida = 0.0f;

        if (jogo->pontuacao >= PONTUACAO_VITORIA) {
            jogo->estado = VITORIA;
            SalvarPontuacao(jogo->pontuacao, jogo->recorde);
            if (jogo->pontuacao > jogo->recorde) jogo->recorde = jogo->pontuacao;
            return;
        }
    }

    if (comeuFrutaRuim) {
        cobra->tamanho -= SEGMENTOS_REMOVIDOS;
        jogo->frutasRuins[indiceFrutaRuimComida].ativa = false;

        if (cobra->tamanho < 1) {
            jogo->estado = GAME_OVER;
            SalvarPontuacao(jogo->pontuacao, jogo->recorde);
            if (jogo->pontuacao > jogo->recorde) jogo->recorde = jogo->pontuacao;
            return;
        }
    }

    /* colisao com o proprio corpo (checa depois de mover a cabeca) */
    if (ColisaoComCorpo(cobra)) {
        jogo->estado = GAME_OVER;
        SalvarPontuacao(jogo->pontuacao, jogo->recorde);
        if (jogo->pontuacao > jogo->recorde) jogo->recorde = jogo->pontuacao;
    }
}

/* ===================================================================
   DrawGame: apenas desenha, nunca altera logica
   =================================================================== */
void DrawGame(Jogo *jogo) {
    BeginDrawing();
    ClearBackground((Color){ 20, 20, 25, 255 });

    if (jogo->estado == MENU) {
        const char *titulo = "SNAKE";
        int larguraTitulo = MeasureText(titulo, 60);
        DrawText(titulo, (LARGURA_TELA - larguraTitulo) / 2, 90, 60, GREEN);

        const char *escolha = "Escolha a dificuldade:";
        DrawText(escolha, LARGURA_TELA/2 - MeasureText(escolha, 22)/2, 175, 22, RAYWHITE);

        const char *op1 = "[1] FACIL    - grade grande, bem devagar, sem obstaculos";
        const char *op2 = "[2] MEDIO    - grade media, alguns obstaculos e fruta podre";
        const char *op3 = "[3] DIFICIL  - grade pequena, muitos obstaculos, muita fruta podre, rapida";
        DrawText(op1, LARGURA_TELA/2 - MeasureText(op1, 18)/2, 215, 18, GREEN);
        DrawText(op2, LARGURA_TELA/2 - MeasureText(op2, 18)/2, 240, 18, YELLOW);
        DrawText(op3, LARGURA_TELA/2 - MeasureText(op3, 18)/2, 265, 18, RED);

        const char *avisoMover = "Use as SETAS ou WASD para mover  |  P para pausar";
        DrawText(avisoMover, LARGURA_TELA/2 - MeasureText(avisoMover, 18)/2, 320, 18, GRAY);

        const char *avisoObstaculo = "Cinza = obstaculo (mata a cobra)";
        DrawText(avisoObstaculo, LARGURA_TELA/2 - MeasureText(avisoObstaculo, 18)/2, 350, 18, (Color){160,160,170,255});
        const char *avisoFruta = "Marrom = fruta ruim (encolhe a cobra)";
        DrawText(avisoFruta, LARGURA_TELA/2 - MeasureText(avisoFruta, 18)/2, 373, 18, (Color){160,110,60,255});

        char recordeTxt[64];
        snprintf(recordeTxt, sizeof(recordeTxt), "Recorde: %d", jogo->recorde);
        DrawText(recordeTxt, LARGURA_TELA/2 - MeasureText(recordeTxt, 20)/2, 420, 20, YELLOW);

        EndDrawing();
        return;
    }

    /* grid de fundo (opcional, ajuda a visualizar as celulas) */
    for (int x = 0; x <= g_colunas; x++) {
        DrawLine(x * g_tamanhoCelula, 0, x * g_tamanhoCelula, ALTURA_TELA, (Color){40,40,45,255});
    }
    for (int y = 0; y <= g_linhas; y++) {
        DrawLine(0, y * g_tamanhoCelula, LARGURA_TELA, y * g_tamanhoCelula, (Color){40,40,45,255});
    }

    /* obstaculos */
    for (int i = 0; i < jogo->numObstaculos; i++) {
        DesenharEntidade(texturaObstaculo, jogo->obstaculos[i], (Color){90, 90, 100, 255});
    }

    /* comida */
    DesenharEntidade(texturaComida, jogo->comida, RED);

    /* frutas marrons (perigosas: encolhem a cobra) */
    for (int i = 0; i < MAX_FRUTAS_RUINS_CAP; i++) {
        if (jogo->frutasRuins[i].ativa) {
            DesenharEntidade(texturaFrutaRuim, jogo->frutasRuins[i].pos, (Color){101, 67, 33, 255});
        }
    }

    /* cobra */
    for (int i = 0; i < jogo->cobra.tamanho; i++) {
        Color cor = (i == 0) ? DARKGREEN : GREEN;
        DrawRectangle(jogo->cobra.corpo[i].x * g_tamanhoCelula,
                       jogo->cobra.corpo[i].y * g_tamanhoCelula,
                       g_tamanhoCelula, g_tamanhoCelula, cor);
    }

    /* placar */
    char placarTxt[96];
    snprintf(placarTxt, sizeof(placarTxt), "Pontuacao: %d   Recorde: %d   Nivel: %s",
              jogo->pontuacao, jogo->recorde, CONFIGS_DIFICULDADE[g_dificuldade].nome);
    DrawText(placarTxt, 10, 10, 20, RAYWHITE);
    DrawText("P = pausar", LARGURA_TELA - MeasureText("P = pausar", 18) - 10, 12, 18, GRAY);

    if (jogo->pausado) {
        DrawRectangle(0, 0, LARGURA_TELA, ALTURA_TELA, (Color){0, 0, 0, 150});
        const char *msgPausa = "PAUSADO";
        DrawText(msgPausa, LARGURA_TELA/2 - MeasureText(msgPausa, 50)/2, ALTURA_TELA/2 - 40, 50, RAYWHITE);
        const char *msgPausa2 = "Pressione P para continuar";
        DrawText(msgPausa2, LARGURA_TELA/2 - MeasureText(msgPausa2, 20)/2, ALTURA_TELA/2 + 20, 20, GRAY);
    }

    if (jogo->estado == GAME_OVER) {
        const char *msg = "GAME OVER";
        DrawText(msg, LARGURA_TELA/2 - MeasureText(msg, 50)/2, ALTURA_TELA/2 - 60, 50, RED);
        const char *msg2 = "Pressione ENTER para voltar ao menu";
        DrawText(msg2, LARGURA_TELA/2 - MeasureText(msg2, 20)/2, ALTURA_TELA/2 + 10, 20, RAYWHITE);
    } else if (jogo->estado == VITORIA) {
        const char *msg = "VOCE VENCEU!";
        DrawText(msg, LARGURA_TELA/2 - MeasureText(msg, 50)/2, ALTURA_TELA/2 - 60, 50, GOLD);
        const char *msg2 = "Pressione ENTER para voltar ao menu";
        DrawText(msg2, LARGURA_TELA/2 - MeasureText(msg2, 20)/2, ALTURA_TELA/2 + 10, 20, RAYWHITE);
    }

    EndDrawing();
}

/* ===================================================================
   Funcoes auxiliares
   =================================================================== */

/* Desenha uma "celula" do jogo (obstaculo, comida ou fruta ruim).
   Se a textura foi carregada (id > 0), desenha o PNG escalado para o
   tamanho da celula. Caso contrario, desenha um retangulo com a cor
   de reserva (fallback), para o jogo continuar funcionando sem imagens. */
static void DesenharEntidade(Texture2D textura, Posicao pos, Color corFallback) {
    Rectangle destino = {
        (float)(pos.x * g_tamanhoCelula),
        (float)(pos.y * g_tamanhoCelula),
        (float)g_tamanhoCelula,
        (float)g_tamanhoCelula
    };

    if (textura.id > 0) {
        Rectangle origem = { 0.0f, 0.0f, (float)textura.width, (float)textura.height };
        DrawTexturePro(textura, origem, destino, (Vector2){ 0.0f, 0.0f }, 0.0f, WHITE);
    } else {
        DrawRectangle((int)destino.x, (int)destino.y, (int)destino.width, (int)destino.height, corFallback);
    }
}

static bool PosicaoOcupadaPeloCorpo(const Cobra *cobra, Posicao pos) {
    for (int i = 0; i < cobra->tamanho; i++) {
        if (cobra->corpo[i].x == pos.x && cobra->corpo[i].y == pos.y) {
            return true;
        }
    }
    return false;
}

/* Checa se uma posicao esta livre (sem cobra, obstaculo, comida ou fruta ruim),
   para nao sortear duas coisas na mesma celula. */
static bool PosicaoOcupada(const Jogo *jogo, Posicao pos, bool considerarComida, bool considerarFrutasRuins) {
    if (PosicaoOcupadaPeloCorpo(&jogo->cobra, pos)) return true;

    for (int i = 0; i < jogo->numObstaculos; i++) {
        if (jogo->obstaculos[i].x == pos.x && jogo->obstaculos[i].y == pos.y) return true;
    }

    if (considerarComida && jogo->comida.x == pos.x && jogo->comida.y == pos.y) return true;

    if (considerarFrutasRuins) {
        for (int i = 0; i < MAX_FRUTAS_RUINS_CAP; i++) {
            if (jogo->frutasRuins[i].ativa &&
                jogo->frutasRuins[i].pos.x == pos.x && jogo->frutasRuins[i].pos.y == pos.y) {
                return true;
            }
        }
    }

    return false;
}

/* Mantem uma "zona segura" ao redor do centro (onde a cobra nasce),
   para o jogador nao morrer em obstaculo antes mesmo de conseguir se mover. */
static bool DentroDaZonaSegura(Posicao pos) {
    int centroX = g_colunas / 2;
    int centroY = g_linhas / 2;
    int folga = TAMANHO_INICIAL + 2;
    return (pos.x >= centroX - folga && pos.x <= centroX + folga &&
            pos.y >= centroY - folga && pos.y <= centroY + folga);
}

static void GerarObstaculos(Jogo *jogo) {
    jogo->numObstaculos = 0;

    int alvo = g_numObstaculosAlvo;
    if (alvo > MAX_OBSTACULOS_CAP) alvo = MAX_OBSTACULOS_CAP;

    for (int tentativa = 0; tentativa < alvo; tentativa++) {
        Posicao pos;
        int tentativasRestantes = 100;
        do {
            pos.x = GetRandomValue(0, g_colunas - 1);
            pos.y = GetRandomValue(0, g_linhas - 1);
            tentativasRestantes--;
        } while ((DentroDaZonaSegura(pos) || PosicaoOcupada(jogo, pos, false, false))
                 && tentativasRestantes > 0);

        if (tentativasRestantes > 0) {
            jogo->obstaculos[jogo->numObstaculos] = pos;
            jogo->numObstaculos++;
        }
    }
}

static void GerarComida(Jogo *jogo) {
    Posicao nova;
    do {
        nova.x = GetRandomValue(0, g_colunas - 1);
        nova.y = GetRandomValue(0, g_linhas - 1);
    } while (PosicaoOcupada(jogo, nova, false, true));

    jogo->comida = nova;
}

static void GerarFrutaRuim(Jogo *jogo, int indice) {
    Posicao nova;
    do {
        nova.x = GetRandomValue(0, g_colunas - 1);
        nova.y = GetRandomValue(0, g_linhas - 1);
    } while (PosicaoOcupada(jogo, nova, true, true));

    jogo->frutasRuins[indice].pos = nova;
}

static bool ColisaoComParede(Posicao pos) {
    return (pos.x < 0 || pos.x >= g_colunas || pos.y < 0 || pos.y >= g_linhas);
}

static bool ColisaoComObstaculo(const Jogo *jogo, Posicao pos) {
    for (int i = 0; i < jogo->numObstaculos; i++) {
        if (jogo->obstaculos[i].x == pos.x && jogo->obstaculos[i].y == pos.y) {
            return true;
        }
    }
    return false;
}

static bool ColisaoComCorpo(const Cobra *cobra) {
    Posicao cabeca = cobra->corpo[0];
    for (int i = 1; i < cobra->tamanho; i++) {
        if (cobra->corpo[i].x == cabeca.x && cobra->corpo[i].y == cabeca.y) {
            return true;
        }
    }
    return false;
}

static int CarregarRecorde(void) {
    FILE *arquivo = fopen(ARQUIVO_PLACAR, "r");
    if (arquivo == NULL) {
        return 0;
    }
    int recorde = 0;
    if (fscanf(arquivo, "%d", &recorde) != 1) {
        recorde = 0;
    }
    fclose(arquivo);
    return recorde;
}

static void SalvarPontuacao(int pontuacao, int recordeAtual) {
    int novoRecorde = (pontuacao > recordeAtual) ? pontuacao : recordeAtual;
    FILE *arquivo = fopen(ARQUIVO_PLACAR, "w");
    if (arquivo == NULL) {
        return;
    }
    fprintf(arquivo, "%d\n", novoRecorde);
    fclose(arquivo);
}