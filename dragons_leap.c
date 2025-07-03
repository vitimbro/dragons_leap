//--------------------------------------------------------//
//               Dragon's Leap para o NES                 //
//--------------------------------------------------------//

// Bibliotecas Padrão da Linguagem C
#include <stdlib.h>
#include <string.h>

// Bibliotecas Específicas do NES
#include "neslib.h"                 // Biblioteca NESlib com funções úteis para o NES
#include <nes.h>                    // Header do CC65 para o NES (definições da PPU)

// Utilitários de Aritmética e VRAM
#include "bcd.h"                    // Suporte para aritmética BCD (Decimal Codificado em Binário)
//#link "bcd.c"

#include "vrambuf.h"                // Buffer de atualização da VRAM
//#link "vrambuf.c"

// Dados Gráficos (CHR)
//#resource "tileset.chr"           // Dados do conjunto de caracteres (CHR)
//#link "chr_generic.s"             // Vincula a pattern table à ROM de CHR

// Nametable do Background
#include "nametable_background.h"


//--------------------------------------------------------//
//                CONFIGURAÇÃO DA PALETA                  //
//--------------------------------------------------------//

/*{pal:"nes",layout:"nes"}*/
const char PALETTE[32] = { 
    0x21,			    // Cor de fundo da tela

    0x0F,0x00,0x2D,0x00,	    // Paleta 0 do Background
    0x0F,0x26,0x36,0x00,	    // Paleta 1 do Background
    0x0F,0x2A,0x2B,0x00,	    // Paleta 2 do Background
    0x0F,0x2A,0x2B,0x00,            // Paleta 3 do Background

    0x0F,0x1A,0x38,0x00,	    // Paleta 0 dos Sprites
    0x00,0x37,0x25,0x00,	    // Paleta 1 dos Sprites
    0x0D,0x2D,0x3A,0x00,	    // Paleta 2 dos Sprites
    0x0D,0x27,0x2A	            // Paleta 3 dos Sprites
};

//--------------------------------------------------------//
//                   SISTEMA DE SUBPIXEL                  //
//--------------------------------------------------------//

// Configuração de ponto fixo (subpixels) para movimento suave
#define SUBPIXEL_SHIFT 4                        // 2^4 = 16 subpixels por pixel
#define SUBPIXEL_UNIT (1 << SUBPIXEL_SHIFT)     // Representa 1 pixel em unidades de subpixel (16)


//--------------------------------------------------------//
//                 METASPRITE DO DRAGÃO                   //
//--------------------------------------------------------//

// Define os índices dos tiles que compõem o dragão
#define TILE_DRAGON_TL 0x111                         // Tile Superior Esquerdo do Dragão
#define TILE_DRAGON_TR 0x112                         // Tile Superior Direito do Dragão
#define TILE_DRAGON_BL 0x121                         // Tile Inferior Esquerdo do Dragão
#define TILE_DRAGON_BR 0x122                         // Tile Inferior Direito do Dragão

// Definição de um metasprite de 16x16 pixels, composto por 4 sprites de 8x8.
// Formato: {posição_x, posição_y, id_do_tile, atributos}
const unsigned char dragon_metasprite[] = {
    0,  0,  TILE_DRAGON_TL, 0,                    // Sprite superior esquerdo
    8,  0,  TILE_DRAGON_TR, 0,                    // Sprite superior direito
    0,  8,  TILE_DRAGON_BL, 0,                    // Sprite inferior esquerdo
    8,  8,  TILE_DRAGON_BR, 0,                    // Sprite inferior direito
    128                                           // Terminador da lista de sprites
};


//--------------------------------------------------------//
//                   VARIÁVEIS DO DRAGÃO                  //
//--------------------------------------------------------//

// Constantes de posição do dragão
#define DRAGON_X_POS 50                 // Posição X fixa do dragão em pixels
#define DRAGON_INIT_Y_POS 50            // Posição Y inicial do dragão em pixels

// Limites de movimento vertical do dragão na tela
#define DRAGON_MIN_Y 28                 // Limite superior da tela para o jogador
#define DRAGON_MAX_Y 194                // Limite inferior da tela para o jogador

// Constantes da física do dragão
#define GRAVITY 4                       // Força da gravidade aplicada ao dragão (em subpixels/quadro²)
#define MAX_GRAVITY 80                  // Velocidade máxima de queda (em subpixels/quadro)
#define JUMP_SPEED -64                  // Velocidade inicial do pulo (negativa para subir)


// Estrutura que armazena todas as variáveis do dragão
typedef struct {
    byte x_pos;                 // Posição horizontal (em pixels)
    byte y_pos;                 // Posição vertical (em pixels)
    
    int y_vel;                  // Velocidade vertical (em subpixels por quadro)
    int y_pos_subpixel;         // Posição vertical em subpixels (para cálculos de física)
} Dragon;

// Declara a variável global para o nosso dragão
Dragon dragon;


// Inicializa a posição e o estado do dragão.
void initialize_dragon() {
    dragon.x_pos = DRAGON_X_POS;        // Define a posição X do jogador
    dragon.y_pos = DRAGON_INIT_Y_POS;   // Define a posição Y inicial do jogador
    
    // Converte a posição Y inicial de pixels para subpixels
    dragon.y_pos_subpixel = DRAGON_INIT_Y_POS << SUBPIXEL_SHIFT;

    // Zera a velocidade vertical inicial do jogador
    dragon.y_vel = 0;
}


// Atualiza a física de movimento do dragão (pulo e gravidade).
void update_dragon_physics() {
    // Verifica se o botão A foi pressionado para iniciar um pulo
    if (pad_trigger(0) & PAD_A) {  
        dragon.y_vel = JUMP_SPEED;      // Define a velocidade vertical para o pulo
    }
    
    // Aplica a força da gravidade à velocidade vertical
    dragon.y_vel += GRAVITY;
    // Limita a velocidade de queda para evitar que o dragão caia rápido demais
    if (dragon.y_vel > MAX_GRAVITY) { 
        dragon.y_vel = MAX_GRAVITY;
    }
    
    // Atualiza a posição de subpixel com base na velocidade atual
    dragon.y_pos_subpixel += dragon.y_vel;
    
    // Converte a posição de subpixel de volta para pixels para o desenho na tela
    // (divisão por 16, feita com um deslocamento de bits à direita)
    dragon.y_pos = dragon.y_pos_subpixel >> SUBPIXEL_SHIFT;
    
    // Mantém o dragão dentro dos limites verticais da tela
    if (dragon.y_pos < DRAGON_MIN_Y) {
        dragon.y_pos = DRAGON_MIN_Y;
        dragon.y_pos_subpixel = dragon.y_pos << SUBPIXEL_SHIFT;
        dragon.y_vel = 0; // Para o movimento ascendente ao bater no teto
    }
  
    if (dragon.y_pos > DRAGON_MAX_Y) {
        dragon.y_pos = DRAGON_MAX_Y;
        dragon.y_pos_subpixel = dragon.y_pos << SUBPIXEL_SHIFT;
        dragon.y_vel = 0; // Para o movimento descendente ao bater no chão
    }
}


//--------------------------------------------------------//
//                   SCROLL HORIZONTAL                    //
//--------------------------------------------------------//

#define NES_MIRRORING 1         // Ativa o Vertical Mirroring para o scroll horizontal

#define SCROLL_SPEED 16         // Velocidade do scroll em subpixels por quadro

#define TILE_SPRITE_ZERO 0x11E  // Índice do Tile utilizado como Sprite Zero

int scroll_x_subpixel = 0;    // Posição do scroll em subpixels
word scroll_x = 0;            // Posição do scroll em pixels (0-511)

// Atualiza a variável de scroll (a posição da câmera)
void update_scroll() { 
  scroll_x_subpixel += SCROLL_SPEED;

  // Converte a posição de subpixel para pixel
  scroll_x = scroll_x_subpixel >> SUBPIXEL_SHIFT;
}

// Função dedicada para configurar o sprite zero uma única vez.
void setup_sprite_zero() {
    // oam_spr(x, y, tile, attr, oam_id)
    oam_spr(10, 22, TILE_SPRITE_ZERO, OAM_BEHIND, 0); 
    //  y: linha do split
    //  tile: um tile com pelo menos um pixel não transparente, para garantir colisão com o background
    //  attr = OAM_BEHIND (prioridade atrás do fundo, para ser invisível)
    //  oam_id = 0 (TEM que ser o primeiro sprite na OAM)
}


//--------------------------------------------------------//
//                  FUNÇÕES AUXILIARES                    //
//--------------------------------------------------------//

// Configura a PPU (Unidade de Processamento de Imagem) e as tabelas gráficas.
void setup_graphics() {
    oam_clear();              // Limpa o buffer OAM, escondendo todos os sprites

    pal_all(PALETTE);         // Carrega a paleta de cores predefinida para o fundo e os sprites

    bank_bg(0);               // Usa o banco de CHR 0 para os tiles do background
    bank_spr(1);              // Usa o banco de CHR 1 para os tiles dos sprites
  
    // Desenha o fundo inicial nas nametables
    vram_adr(NAMETABLE_A);
    vram_write(nametable_background, 1024); // Escreve os 1024 bytes da nametable 
    vram_adr(NAMETABLE_B);
    vram_write(nametable_background, 1024);
}


// Desenha todos os sprites do jogo na tela.
void draw_sprites() { 
    // Começa a desenhar a partir do slot 1, pois o slot 0 está reservado para o sprite zero.
    // Cada sprite ocupa 4 bytes no OAM, então o próximo ID livre é 4.
    char oam_id = 4; // Inicializa o contador de sprites no primeiro slot livre da OAM

    // Desenha o metasprite do dragão na sua posição atual
    oam_id = oam_meta_spr(dragon.x_pos, dragon.y_pos, oam_id, dragon_metasprite);

    // Esconde todos os outros slots de sprite não utilizados para evitar "sprites fantasmas"
    oam_hide_rest(oam_id); 
}


//--------------------------------------------------------//
//                 LOOP PRINCIPAL DO JOGO                 //
//--------------------------------------------------------//

void main(void)
{
    // Executa a configuração inicial dos gráficos
    setup_graphics();
  
    // Configura o sprite zero uma vez, na inicialização.
    setup_sprite_zero();

    // Ativa a renderização da PPU para mostrar os gráficos na tela
    ppu_on_all();

    // Define a posição inicial do dragão
    initialize_dragon();

    // Loop infinito que executa o jogo
    while(1) {
        // A chamada a split() substitui ppu_wait_nmi()
        // Ela espera pelo sprite zero e atualiza o scroll horizontal
        split(scroll_x, 0);
      
        // Atualiza a posição da câmera
        update_scroll();

        // Atualiza a lógica da física do dragão (movimento)
        update_dragon_physics();

        // Desenha todos os sprites na tela
        draw_sprites();
    }
}
