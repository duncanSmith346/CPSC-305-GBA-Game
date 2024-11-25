#include "gba.c"

enum State
{
    STANDING, WALKING, CROUCHING, AIRBORNE, DASHING, BACKDASH, STARTUP, ACTIVE,
    RECOVERY, BLOCKSTUN, HITSTUN
};

#define FTR_WIDTH 32
#define FTR_HEIGHT 64
#define FTR_BORDER 3
#define FTR_HEALTH 100
struct Fighter // Fighter struct
{
    struct Sprite* sprite; // The sprite attribute info
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
};

// Initializes a Fighter
void fighter_init(struct Fighter* fighter, int player)
{
    // 16 is the height of the floor, 64 is the height of the fighter
    fighter->y = SCREEN_HEIGHT - 16 - 64;
    fighter->x = player ? 60 : 116;
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

    fighter->animation_delay = 7;
    fighter->counter = 0;
    fighter->frame = 0;

    fighter->sprite = sprite_init(fighter->x, fighter->y, SIZE_64_64,
            fighter->dir, 0, fighter->frame, 0);
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
    fighter->counter = 7;
}

void fighter_crouch(struct Fighter* fighter)
{
    fighter->state = CROUCHING;
    fighter->vx = 0;
    fighter->frame = 256;
}

void fighter_jump(struct Fighter* fighter)
{
    fighter->state = AIRBORNE;
    fighter->vy = -1350;
    fighter->frame = 384;

    if (button_pressed(BUTTON_RIGHT))
        fighter->vx = fighter->dir ? fighter->vjb : fighter->vjf;
    else if (button_pressed(BUTTON_LEFT))
        fighter->vx = fighter->dir ? -(fighter->vjf) : -(fighter->vjb);
    else
        fighter->vx = 0;
}

void fighter_update(struct Fighter* fighter, struct Fighter* enemy)
{
    switch (fighter->state)
    {
        case AIRBORNE:
            fighter->y += (fighter->vy >> 8);
            fighter->vy += fighter->dy;

            if (fighter->y >= 80)
            {
                fighter->state = STANDING;
                fighter->y = 80;
                fighter->vy = 0;
                sprite_set_offset(fighter->sprite, 0);
            }
            break;

        case WALKING:
            fighter->counter++;

            if (fighter->counter >= fighter->animation_delay)
            {
                fighter->frame = fighter->frame + 128;

                if (fighter->frame > 128)
                {
                    fighter->frame = 0;
                }

                fighter->counter = 0;
            }
            break;

        case STANDING:
            break;
    }
    
    fighter->x = (fighter->bx >> 4);
    fighter->bx += fighter->vx;

    // turns fighters around
    if (fighter->dir && fighter->x < enemy->x + (FTR_WIDTH / 2))
        fighter->dir = 0;

    else if (!fighter->dir && fighter->x > enemy->x + (FTR_WIDTH / 2))
        fighter->dir = 1;

    if (fighter->dir != (fighter->sprite->attribute1 & (1 << 12)))
        sprite_set_horizontal_flip(fighter->sprite, fighter->dir);

    // updates the sprite
    sprite_set_offset(fighter->sprite, fighter->frame);
    sprite_position(fighter->sprite, fighter->x, fighter->y);
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
    }
    else if (player->x + FTR_WIDTH > SCREEN_WIDTH - FTR_BORDER)
    {
    }
    else if (enemy->x + FTR_WIDTH > SCREEN_WIDTH - FTR_BORDER)
    {
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
