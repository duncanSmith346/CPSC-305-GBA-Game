#include "gba.c"

void setSize(struct Sprite* sprite, int shape_bits, int size_bits);

void sprite_set_size(struct Sprite* sprite, enum SpriteSize size)
{
    int size_bits, shape_bits;
    switch (size) {
        case SIZE_8_8:   size_bits = 0; shape_bits = 0; break;
        case SIZE_16_16: size_bits = 1; shape_bits = 0; break;
        case SIZE_32_32: size_bits = 2; shape_bits = 0; break;
        case SIZE_64_64: size_bits = 3; shape_bits = 0; break;
        case SIZE_16_8:  size_bits = 0; shape_bits = 1; break;
        case SIZE_32_8:  size_bits = 1; shape_bits = 1; break;
        case SIZE_32_16: size_bits = 2; shape_bits = 1; break;
        case SIZE_64_32: size_bits = 3; shape_bits = 1; break;
        case SIZE_8_16:  size_bits = 0; shape_bits = 2; break;
        case SIZE_8_32:  size_bits = 1; shape_bits = 2; break;
        case SIZE_16_32: size_bits = 2; shape_bits = 2; break;
        case SIZE_32_64: size_bits = 3; shape_bits = 2; break;
    }

    setSize(sprite, shape_bits, size_bits);
}

struct Attack
{
    int st, ac, rc;
    int hby, hbw, hbh;
    int dmg;
    int spo, spx, spy;
    enum SpriteSize size;
};

struct Attack attack_init(int params[], enum SpriteSize size)
{
    struct Attack atk;
    atk.st = params[0];
    atk.ac = params[1];
    atk.rc = params[2];
    atk.hby = params[3];
    atk.hbw = params[4];
    atk.hbh = params[5];
    atk.dmg = params[6];
    atk.spo = params[7];
    atk.spx = params[8];
    atk.spy = params[9];
    atk.size = size;

    return atk;
}

enum State
{
    STANDING, WALKING, CROUCHING, AIRBORNE, DASHING, BACKDASH, BLOCKSTUN, HITSTUN,
    STARTUP, ACTIVE, RECOVERY
};

#define FTR_WIDTH 32
#define FTR_HEIGHT 48
#define FTR_BORDER 3
#define FTR_HEALTH 100
struct Fighter // Fighter struct
{
    int x, y; // x & y position in pixels
    int vx, vy; // x & y velocity in 1/256 pixels/second
    int bx; // the 'big x', i.e. the x << 4 for fine speed and shit
    int dy; // y acceleration in 1/256 pixels/second^2
    enum State state;
    int move;
    int scounter; // counts how many frames until you're back in neutral
    int dir; // 0 for facing right, 1 for left

    int health;
    int vwf, vwb; // walkspeed forward and backward, respectively (1/16 pixels/frame)
    int vjf, vjb; // forward and backward jump velocity (1/16 pixels/frame)

    int animation_delay;
    int counter;
    int color; // the offset in the sprite sheet for the fighter's color
    int frame; // the frame of the animation the Fighter is on

    // the sprites that make up the fighter
    struct Sprite* body;
    struct Sprite* horns;
    struct Sprite* backLeg;
    struct Sprite* frontLeg;
    struct Sprite* attack;

    struct Attack moves[9];
};

// Initializes a Fighter
void fighter_init(struct Fighter* fighter, int player)
{
    // 16 is the height of the floor
    fighter->y = SCREEN_HEIGHT - FTR_HEIGHT - 16;
    fighter->x = player ? 76 : 132;
    fighter->vx = 0;
    fighter->vy = 0;
    fighter->bx = fighter->x << 4;
    fighter->dy = 50;
    fighter->state = STANDING;
    fighter->scounter = 0;
    fighter->dir = player ? 0 : 1;

    fighter->health = FTR_HEALTH;
    fighter->vwf = 24;
    fighter->vwb = 13;
    fighter->vjf = 24;
    fighter->vjb = 13;

    fighter->animation_delay = 10;
    fighter->counter = 0;
    fighter->frame = 0;

    fighter->body = sprite_init(fighter->x, fighter->y, SIZE_32_32, fighter->dir,
            0, 0, 0);
    fighter->horns = sprite_init(fighter->x + (fighter->dir ? 0 : 16), fighter->y - 8, 
            SIZE_16_8, fighter->dir, 0, 32, 0);
    fighter->backLeg = sprite_init(fighter->x + (fighter->dir ? 19 : 5),
            fighter->y + 32, SIZE_8_16, fighter->dir, 0, 36, 0);
    fighter->frontLeg = sprite_init(fighter->x + (fighter->dir ? 3 : 21),
            fighter->y + 32, SIZE_8_16, fighter->dir, 0, 36, 0);
    fighter->attack = sprite_init(fighter->x + (fighter->dir ? -29 : 29), fighter->y,
            SIZE_8_8, fighter->dir, 0, 100, 0);

    // 5A
    int params[] = {20, 15, 30, 14, 16, 8, 6, 36, 0, 14};
    fighter->moves[0] = attack_init(params, SIZE_16_8);
}

int fighter_right(struct Fighter* fighter, struct Fighter* enemy)
{
    fighter->state = WALKING;

    if (fighter->x + FTR_WIDTH > SCREEN_WIDTH - FTR_BORDER)
        return 1;

    else if (!fighter->dir && fighter->bx + fighter->vwf + (FTR_WIDTH << 4)
            >= enemy->bx)
    {
        fighter->vx = 0;
        return 0;
    }

    else
    {
        fighter->vx = fighter->dir ? fighter->vwb : fighter->vwf;
        return 0;
    }
}

int fighter_left(struct Fighter* fighter, struct Fighter* enemy)
{
    fighter->state = WALKING;

    if (fighter->x < FTR_BORDER)
        return 1;

    /* the dir check is actually necessary
       if it's not there, you can't walk backwards
       */
    else if (fighter->dir && fighter->bx - fighter->vwf
            <= enemy->bx + (FTR_WIDTH << 4))
    {
        fighter->vx = 0;
        return 0;
    }

    else
    {
        fighter->vx = fighter->dir ? -(fighter->vwf) : -(fighter->vwb);
        return 0;
    }
}

void fighter_stop(struct Fighter* fighter)
{
    fighter->state = STANDING;
    fighter->vx = 0;
    fighter->frame = 0;
    fighter->counter = 10;

    sprite_set_size(fighter->frontLeg, SIZE_8_16);
    sprite_set_offset(fighter->frontLeg, 36);
}

void fighter_crouch(struct Fighter* fighter)
{
    fighter->state = CROUCHING;
    fighter->vx = 0;
    fighter->y = SCREEN_HEIGHT - 48;

    sprite_set_size(fighter->frontLeg, SIZE_8_16);
    sprite_set_offset(fighter->frontLeg, 36);
}

void fighter_jump(struct Fighter* fighter)
{
    fighter->state = AIRBORNE;
    fighter->vy = -1350;
    fighter->frame = 384;

    sprite_set_size(fighter->frontLeg, SIZE_8_16);
    sprite_set_offset(fighter->frontLeg, 36);

    if (button_pressed(BUTTON_RIGHT))
        fighter->vx = fighter->dir ? fighter->vjb : fighter->vjf;
    else if (button_pressed(BUTTON_LEFT))
        fighter->vx = fighter->dir ? -(fighter->vjf) : -(fighter->vjb);
    else
        fighter->vx = 0;
}

void fighter_attack(struct Fighter* fighter, int move)
{
    fighter->state = STARTUP;
    fighter->move = move;
    fighter->scounter = fighter->moves[move].st;
    fighter->vx = 0;
    fighter->frame = 0;

    sprite_set_size(fighter->frontLeg, SIZE_8_16);
    sprite_set_offset(fighter->frontLeg, 36);
}

void fighter_flip(struct Fighter* fighter)
{
    sprite_set_horizontal_flip(fighter->body, fighter->dir);
    sprite_set_horizontal_flip(fighter->horns, fighter->dir);
    sprite_set_horizontal_flip(fighter->backLeg, fighter->dir);
    sprite_set_horizontal_flip(fighter->frontLeg, fighter->dir);
    sprite_set_horizontal_flip(fighter->attack, fighter->dir);
}



void fighter_update(struct Fighter* fighter, struct Fighter* enemy)
{
    int blx = fighter->x + (fighter->dir ? 19 : 5);
    int bly = fighter->y + 32;
    int flx = fighter->x + (fighter->dir ? 3 : 21);
    int fly = fighter->y + 32;
    int atx = fighter->x + (fighter->dir ? -29 : 29);
    int aty = fighter->y;

    switch (fighter->state)
    {
        case STARTUP:
            if (fighter->scounter <= 0)
            {
                fighter->scounter = fighter->moves[fighter->move].ac;
                fighter->state = ACTIVE;
                break;
            }

            fighter->scounter--;
            sprite_set_size(fighter->attack,
                    fighter->moves[fighter->move].size);
            sprite_set_offset(fighter->attack,
                    fighter->moves[fighter->move].spo);
            aty += fighter->moves[fighter->move].spy;
            break;

        case ACTIVE:
            if (fighter->scounter <= 0)
            {
                fighter->scounter = fighter->moves[fighter->move].rc;
                fighter->state = RECOVERY;
                break;
            }

            fighter->scounter--;
            sprite_set_size(fighter->attack,
                    fighter->moves[fighter->move].size);
            sprite_set_offset(fighter->attack,
                    fighter->moves[fighter->move].spo);
            aty += fighter->moves[fighter->move].spy;
            break;

        case RECOVERY:
            if (fighter->scounter <= 0)
            {
                fighter->state = STANDING;
            }

            fighter->scounter--;
            sprite_set_size(fighter->attack,
                    fighter->moves[fighter->move].size);
            sprite_set_offset(fighter->attack,
                    fighter->moves[fighter->move].spo);
            aty += fighter->moves[fighter->move].spy;
            break;

        case CROUCHING:
            bly -= 16;
            fly -= 16;
            break;

        case AIRBORNE:
            fighter->y += (fighter->vy >> 8);
            fighter->vy += fighter->dy;

            bly -= 16;
            fly -= 16;

            if (fighter->y >= SCREEN_HEIGHT - 48)
            {
                fighter->state = STANDING;
                fighter->vy = 0;
            }
            break;

        case WALKING:
            fighter->y = SCREEN_HEIGHT - 64;
            fighter->counter++;

            if (fighter->counter >= fighter->animation_delay)
            {
                if (fighter->frame)
                {
                    sprite_set_size(fighter->frontLeg, SIZE_8_16);
                    sprite_set_offset(fighter->frontLeg, 36);
                    fighter->frame = 0;
                }
                else
                {
                    sprite_set_size(fighter->frontLeg, SIZE_16_16);
                    sprite_set_offset(fighter->frontLeg, 40);
                    fighter->frame = 1;
                }

                fighter->counter = 0;
            }

            if (fighter->frame && fighter->dir)
                flx -= 8;
            break;

        case STANDING:
            fighter->y = SCREEN_HEIGHT - 64;
            sprite_set_offset(fighter->attack, 100);
            break;
    }
    
    fighter->x = (fighter->bx >> 4);
    fighter->bx += fighter->vx;

    // turns fighters around
    if (fighter->dir && fighter->x + (FTR_WIDTH / 2) < enemy->x + (FTR_WIDTH / 2))
        fighter->dir = 0;

    else if (!fighter->dir && fighter->x + (FTR_WIDTH / 2) > enemy->x + (FTR_WIDTH / 2))
        fighter->dir = 1;

    if (fighter->dir != (fighter->body->attribute1 & (1 << 12)))
        fighter_flip(fighter);

    // updates the sprites
    sprite_position(fighter->body, fighter->x, fighter->y);
    sprite_position(fighter->horns, fighter->x + (fighter->dir ? 0 : 16),
            fighter->y - 8);
    sprite_position(fighter->backLeg, blx, bly);
    sprite_position(fighter->frontLeg, flx, fly);
    sprite_position(fighter->attack, atx, aty);
}

int background_update(struct Fighter* player, struct Fighter* enemy)
{
    int scroll = 0;
    if (player->x < FTR_BORDER)
    {
        player->bx += (FTR_BORDER << 4) - player->bx;
        enemy->bx -= player->vx;
        scroll += player->vx;
    }
    else if (enemy->x < FTR_BORDER)
    {
        enemy->bx += (FTR_BORDER << 4) - player->bx;
        player->bx -= enemy->vx;
        scroll += enemy->vx;
    }
    if (player->x + FTR_WIDTH > SCREEN_WIDTH - FTR_BORDER)
    {
        player->bx -= (player->bx + (FTR_WIDTH << 4))
            - ((SCREEN_WIDTH - FTR_BORDER) << 4);
        enemy->bx -= player->vx;
        scroll += player->vx;
    }
    else if (enemy->x + FTR_WIDTH > SCREEN_WIDTH - FTR_BORDER)
    {
        enemy->bx -= (enemy->bx + (FTR_WIDTH << 4))
            - ((SCREEN_WIDTH - FTR_BORDER) << 4);
        player->bx -= enemy->vx;
        scroll += enemy->vx;
    }

    return scroll;
}

/* the main function */
int main() {
    /* we set the mode to mode 0 with bg0 on */
    *display_control = MODE0 | BG0_ENABLE | SPRITE_ENABLE | SPRITE_MAP_1D;

    /* setup the background 0 */
    setup_background();

    /* setup the sprite image data */
    setup_sprite_image();

    /* clear all the sprites on screen now */
    sprite_clear();

    /* create the player*/
    struct Fighter player;
    fighter_init(&player, 1);
    struct Fighter enemy;
    fighter_init(&enemy, 0);

    /* set initial scroll to 0 */
    int xscroll = 0;

    /* loop forever */
    while (1)
    {
        fighter_update(&player, &enemy);
        fighter_update(&enemy, &player);

        // input handling
        switch (player.state)
        {
            case HITSTUN: case BACKDASH: case STARTUP: case ACTIVE:
                player.scounter--;
                break;
            case BLOCKSTUN:
                break;
            case RECOVERY:
                break;
            case AIRBORNE:
                break;
            case STANDING: case WALKING: case CROUCHING: case DASHING:
                if (button_pressed(BUTTON_DOWN))
                {
                    fighter_crouch(&player);
                }
                else if (button_pressed(BUTTON_UP))
                {
                    fighter_jump(&player);
                }
                else if (button_pressed(BUTTON_RIGHT))
                {
                    fighter_right(&player, &enemy);
                }
                else if (button_pressed(BUTTON_LEFT))
                {
                    fighter_left(&player, &enemy);
                }
                else if (button_pressed(BUTTON_A))
                {
                    fighter_attack(&player, 0);
                }
                else
                {
                    fighter_stop(&player);
                }
                break;
        }

        if (button_pressed(BUTTON_SELECT))
        {
            struct Fighter temp;
            temp = player;
            player = enemy;
            enemy = temp;
        }

        // manage the scrolling of the screen
        /* If either fighter moves to the edge of the screen for any reason,
           prevent them from moving past it and scroll the background to compensate
           Move the other fighter as well
           If the other fighter is at the other edge of the screen, don't scroll
           */
        xscroll += background_update(&player, &enemy);
        
        /* wait for vblank before scrolling and moving sprites */
        wait_vblank();
        *bg0_x_scroll = xscroll >> 4;
        sprite_update_all();

        /* delay some */
        delay(300);
    }
}
