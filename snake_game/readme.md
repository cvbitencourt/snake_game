# 🐍 Snake Game

Um jogo da cobrinha desenvolvido em C utilizando a biblioteca **Raylib**.

---

## 🛠️ Guia de Instalação, Compilação e Execução

Abra o terminal do **MSYS2 (MinGW UCRT64 ou MinGW 64-bit)** e execute os comandos abaixo. Todo o processo está documentado linha por linha para facilitar o entendimento:

```bash
# ==============================================================================
# 1. INSTALAÇÃO DOS PRÉ-REQUISITOS
# ==============================================================================

# Instala a biblioteca Raylib específica para o ambiente MinGW de 64 bits
pacman -S mingw-w64-x86_64-raylib

# Instala o compilador GCC para permitir a compilação de códigos em C/C++
pacman -S mingw-w64-x86_64-gcc

# ==============================================================================
# 2. COMPILAÇÃO E EXECUÇÃO DO JOGO
# ==============================================================================

# Altera o diretório atual do terminal para a pasta onde o código do jogo está salvo
cd /c/snake_game

# Compila o arquivo "snake.c" e gera o executável "meu_jogo.exe"
# Explicação das flags de linkagem:
# -lraylib: Une a biblioteca gráfica Raylib ao seu código
# -lopengl32: Linka a API gráfica OpenGL (necessária para a Raylib renderizar os gráficos)
# -lgdi32: Linka a API de interface de dispositivos de exibição nativa do Windows
# -lwinmm: Linka funções multimídia do Windows (gerenciamento de tempo e som)
gcc snake.c -o meu_jogo.exe -lraylib -lopengl32 -lgdi32 -lwinmm

# Inicializa e roda o jogo diretamente pelo terminal do MSYS2
./meu_jogo.exe