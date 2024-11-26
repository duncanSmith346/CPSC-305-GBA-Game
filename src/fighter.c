#include "gba.c"

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

    sprite->attribute0 &= 0x3fff; // clears the shape bits
    sprite->attribute0 |= shape_bits << 14; // sets the new bits

    sprite->attribute1 &= 0x3fff; // clears the size bits
    sprite->attribute1 |= size_bits << 14; // sets the new bits
}

enum State
{
    STANDING, WALKING, CROUCHING, AIRBORNE, DASHING, BACKDASH, STARTUP, ACTIVE,
    RECOVERY, BLOCKSTUN, HITSTUN,
    SA, SB, CA, CB, JA, JB, QCF_A, DP_B, QCB_B
};

#define FTR_WIDTH 32
#define FTR_HEIGHT 64
#define FTR_BORDER 3
#define FTR_HEALTH 100
struct Fighter // Fighter struct
{
    int x, y; // x & y position in pixels
    int vx, vy; // x & y velocity in 1/256 pixels/second
    int bx; // the 'big x', i.e. the x << 4 for fine speed and shit
    int dy; // y acceleration in 1/256 pixels/second^2
    enum State state;
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
};

// Initializes a Fighter
void fighter_init(struct Fighter* fighter, int player)
{
    // 16 is the height of the floor, 48 is the height of the fighter
    fighter->y = SCREEN_HEIGHT - 64;
    fighter->x = player ? 76 : 132;
    fighter->vx = 0;
    fighter->vy = 0;
    fighter->bx = fighter->x << 4;
    fighter->dy = 50;
    fighter->state = STANDING;
    fighter->scounter = 0;
    fighter->dir = player ? 0 : 1;

    fighter->health = 100;
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
    fighter->attack = sprite_init(fighter->x - (fighter->dir ? 32 : 0), fighter->y,
            SIZE_8_8, fighter->dir, 0, 992, 0);
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
    int atx = fighter->x - (fighter->dir ? 32 : 0);
    int aty = fighter->y;

    switch (fighter->state)
    {
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
            break;
    }
    
    fighter->x = (fighter->bx >> 4);
    fighter->bx += fighter->vx;

    // turns fighters around
    if (fighter->dir && fighter->x < enemy->x + (FTR_WIDTH / 2))
        fighter->dir = 0;

    else if (!fighter->dir && fighter->x > enemy->x + (FTR_WIDTH / 2))
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
    // if there is a fighter at both edges of the screen
    if (player->x < FTR_BORDER && enemy->x + FTR_WIDTH > SCREEN_WIDTH - FTR_BORDER)
    {
        player->x += FTR_BORDER - player->x;
        enemy->x -= FTR_BORDER - enemy->x;
        return 0;
    }
    else if (player->x + FTR_WIDTH > SCREEN_WIDTH - FTR_BORDER && enemy->x < FTR_BORDER)
    {
        player->x -= FTR_BORDER - player->x;
        enemy->x += FTR_BORDER - enemy->x;
        return 0;
    }
 

    if (player->x < FTR_BORDER)
    {
        player->bx += (FTR_BORDER << 4) - player->bx;
        enemy->bx -= player->vx;
        return player->vx;
    }
    else if (enemy->x < FTR_BORDER)
    {
        enemy->bx += (FTR_BORDER << 4) - player->bx;
        player->bx -= enemy->vx;
        return enemy->vx;
    }
    else if (player->x + FTR_WIDTH > SCREEN_WIDTH - FTR_BORDER)
    {
        player->bx -= player->bx + (FTR_WIDTH << 4)
            - ((SCREEN_WIDTH - FTR_BORDER) << 4);
        enemy->bx -= player->vx;
        return player->vx;
    }
    else if (enemy->x + FTR_WIDTH > SCREEN_WIDTH - FTR_BORDER)
    {
        enemy->bx -= enemy->bx - ((SCREEN_WIDTH - FTR_BORDER) << 4);
        player->bx -= enemy->vx;
        return enemy->vx;
    }
    else
        return 0;
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
            case HITSTUN: case STARTUP: case ACTIVE: case BACKDASH:
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
                    /*
                    if (fighter_right(&player, &enemy))
                    {
                        xscroll += 13;
                    }
                    */
                    fighter_right(&player, &enemy);
                }
                else if (button_pressed(BUTTON_LEFT))
                {
                    /*
                    if (fighter_left(&player, &enemy))
                    {
                        xscroll -= 13;
                    }
                    */
                    fighter_left(&player, &enemy);
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
