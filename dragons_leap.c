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


void initialize_dragon();
void update_dragon_physics();


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


void update_scroll();
void setup_sprite_zero();
  
  
// Atualiza a variável de scroll (a posição da câmera)
void update_scroll() { 
    scroll_x_subpixel += SCROLL_SPEED;
    scroll_x = scroll_x_subpixel >> SUBPIXEL_SHIFT;

    if (scroll_x >= 512) {
        scroll_x -= 512;
        scroll_x_subpixel -= (512 << SUBPIXEL_SHIFT);
    }
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
//                  VARIÁVEIS DAS TORRES                  //
//--------------------------------------------------------//

// Tiles da torre
#define TILE_TOP_LEFT   0x88
#define TILE_TOP_MID    0x89
#define TILE_TOP_RIGHT  0x8A

#define TILE_BOT_LEFT   0xA8
#define TILE_BOT_MID    0xA9
#define TILE_BOT_RIGHT  0xAA

// Constantes
#define TOWER_HEIGHT       22      // 8 + 6 + 8
#define TOWER_COLUMNS      4       // L, M, M, R
#define TOWER_GAP_START    8
#define TOWER_GAP_END      14

#define SCREEN_WIDTH_TILES 32
#define SCORE_HEIGHT 4

#define NUM_TOWERS 4

#define SCROLL_TOWER_0 31
#define SCROLL_TOWER_1 46
#define SCROLL_TOWER_2 0
#define SCROLL_TOWER_3 15


typedef struct {
    byte nametable_id;       // 0: Nametable A, 1: Nametable B
    byte collum_index;       // 0 a 3 (qual das 4 colunas da torre está sendo desenhada)
    byte base_collum;        // coluna base na nametable (0 ou 16)
    bool drawn;              // se a torre já foi desenhada
} Tower;

Tower towers[NUM_TOWERS];

byte tower_palette_index = 0;  // escolher entre 0–3 (qual das 4 paletas BG)

byte color_buffer[TOWER_HEIGHT / 4];


void initialize_towers();
void fill_tower_column(byte *buffer, byte column);
word nametable_to_attribute_addr(word a);
void fill_color_buffer(byte palette_index);
void put_color(word addr);
void draw_tower_column(Tower* tower);
void update_towers(word scroll_x);
  
  
void initialize_towers() {
    byte i;
  
    // Torre 0 → NT A, colunas 0–3
    towers[0].nametable_id = 0;
    towers[0].base_collum = 0;

    // Torre 1 → NT A, colunas 16–19
    towers[1].nametable_id = 0;
    towers[1].base_collum = 16;

    // Torre 2 → NT B, colunas 0–3 (32-35)
    towers[2].nametable_id = 1;
    towers[2].base_collum = 0;

    // Torre 3 → NT B, colunas 16–19 (48-51)
    towers[3].nametable_id = 1;
    towers[3].base_collum = 16;

    for (i = 0; i < NUM_TOWERS; i++) {
        towers[i].collum_index = 0;
        towers[i].drawn = false;
    }
}


// Preenche a coluna de tiles no buffer
void fill_tower_column(byte *buffer, byte column) {
    byte top_tile;
    byte bottom_tile;
  
    byte i;

    // Define quais tiles usar com base na coluna (0 = L, 1 ou 2 = M, 3 = R)
    switch (column) {
        case 0:
            top_tile = TILE_TOP_LEFT;
            bottom_tile = TILE_BOT_LEFT;
            break;
        case 3:
            top_tile = TILE_TOP_RIGHT;
            bottom_tile = TILE_BOT_RIGHT;
            break;
        default:
            top_tile = TILE_TOP_MID;
            bottom_tile = TILE_BOT_MID;
            break;
    }

    for (i = 0; i < TOWER_HEIGHT; i++) {
        if (i >= TOWER_GAP_START && i < TOWER_GAP_END) {
            buffer[i] = 0x00;  // lacuna
        } else if (i < TOWER_GAP_START) {
            buffer[i] = top_tile;
        } else {
            buffer[i] = bottom_tile;
        }
    }
}



void draw_tower_column(Tower* tower) {
    static byte buffer[TOWER_HEIGHT];
  
    word base_nametable;
    word addr;

    fill_tower_column(buffer, tower->collum_index);

    base_nametable = (tower->nametable_id == 0) ? NAMETABLE_A : NAMETABLE_B;

    // Desenha da linha 4 até 25 (posição vertical de torre no background)
    addr = base_nametable + tower->base_collum + tower->collum_index + (SCREEN_WIDTH_TILES * SCORE_HEIGHT); 

    vrambuf_put(addr | VRAMBUF_VERT, buffer, TOWER_HEIGHT);

    // Se for a primeira coluna da torre, escreve os atributos
    if (tower->collum_index == 0) {
        word attr_addr = nametable_to_attribute_addr(addr);
        fill_color_buffer(tower_palette_index);  // ou 1, 2, 3...
        put_color(attr_addr);
    }
  
    tower->collum_index++;

    // Quando todas as 4 colunas forem desenhadas, marcamos como "completa"
    if (tower->collum_index >= TOWER_COLUMNS) {
        tower->drawn = true;
        tower->collum_index = 0;
    }
}


void update_towers(word scroll_x) {
    byte scroll_tile = scroll_x >> 3;

    // Torre 2 → scroll tile 0 (inicio da NT B)
    if (scroll_tile == SCROLL_TOWER_2 && !towers[2].drawn) {
        draw_tower_column(&towers[2]);

        // Torre 0 saiu da tela
        towers[0].drawn = false;
        towers[0].collum_index = 0;
    }

    // Torre 3 → scroll tile 15 (meio da NT B)
    if (scroll_tile == SCROLL_TOWER_3 && !towers[3].drawn) {
        draw_tower_column(&towers[3]);

        // Torre 1 saiu da tela
        towers[1].drawn = false;
        towers[1].collum_index = 0;
    }

    // Torre 0 → scroll tile 31 (inicio da NT A)
    if (scroll_tile == SCROLL_TOWER_0 && !towers[0].drawn) {
        draw_tower_column(&towers[0]);

        // Torre 2 saiu da tela
        towers[2].drawn = false;
        towers[2].collum_index = 0;
    }

    // Torre 1 → scroll tile 46 (meio da NT A)
    if (scroll_tile == SCROLL_TOWER_1 && !towers[1].drawn) {
        draw_tower_column(&towers[1]);

        // Torre 3 saiu da tela
        towers[3].drawn = false;
        towers[3].collum_index = 0;
    }
}


word nametable_to_attribute_addr(word a) {
    return (a & 0x2C00)       // mantém origem da nametable (0x2000 ou 0x2400)
         | 0x03C0             // início da attribute table
         | ((a >> 4) & 0x38)  // linha (cada 4 tiles → shift 4)
         | ((a >> 2) & 0x07); // coluna (cada 4 tiles → shift 2)
}


void fill_color_buffer(byte palette_index) {
    byte attr = palette_index;
    byte i;
  
    attr |= (palette_index << 2);
    attr |= (palette_index << 4);
    attr |= (palette_index << 6);

    for (i = 0; i < (TOWER_HEIGHT / 4) + 1; i++) {
        color_buffer[i] = attr;  // aplica mesma cor em toda a coluna
    }
}


void put_color(word addr) {
    byte i;
    for (i = 0; i < (TOWER_HEIGHT / 4) + 1; i++) {
        vrambuf_put(addr, &color_buffer[i], 1);
        addr += 8;  // próxima linha da attribute table (cada linha tem 8 bytes)
    }
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
  
    vrambuf_clear();          // Clear VRAM buffer
    set_vram_update(updbuf);  // Link VRAM update buffer

    // Define a posição inicial do dragão
    initialize_dragon();
  
    initialize_towers();
  
    // Ativa a renderização da PPU para mostrar os gráficos na tela
    ppu_on_all();

    // Loop infinito que executa o jogo
    while(1) {
        ppu_wait_nmi();   // wait for NMI to ensure previous frame finished
        vrambuf_clear();  // Clear VRAM buffer each frame immediately after NMI
      
        // Ela espera pelo sprite zero e atualiza o scroll horizontal
        split(scroll_x, 0);
      
        // Atualiza a posição da câmera
        update_scroll();

        // Atualiza a lógica da física do dragão (movimento)
        update_dragon_physics();
      
        update_towers(scroll_x);

        // Desenha todos os sprites na tela
        draw_sprites();
    }
}
