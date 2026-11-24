#include "PanCanvas.h"
#include "IngredientSprites.h" /* Reuse the approved generated icon masks. */
#include "FontStuff.h"
#include <string.h>
static EInkFrame *canvas;
static const char *title;
static uint8_t ingredients, item, row, character;
static unsigned clearing;
static int left;
static bool complete;
bool PanCanvas_Start(EInkFrame *frame, PanDisplaySMState_t state, uint8_t mask)
{
    if (!frame) return false;
    canvas = frame; ingredients = state == PAN_ATTRACT ? 31u : mask;
    title = state == PAN_ATTRACT ? "PANIC AT THE PAN" :
            state == PAN_GAME_OVER_TEXT ? "GAME OVER" :
            state == PAN_TIMEOUT_ICON ? "TIMEOUT" : "CHICKEN FRIED RICE";
    left = ((int)EINK_WIDTH-(int)strlen(title)*12)/2;
    clearing = item = row = character = 0; complete = false;
    return true;
}
bool PanCanvas_Service(void)
{
    if (!canvas || complete) return complete;
    if (clearing < sizeof *canvas) {
        size_t count = sizeof *canvas-clearing;
        if (count > 64) count = 64;
        memset((uint8_t *)canvas+clearing,0,count); clearing += (unsigned)count;
        return false;
    }
    if (item < INGREDIENT_SPRITE_COUNT) {
        if (ingredients & (1u<<item)) for (unsigned x=0;x<48;++x) {
            unsigned offset = row*6u+x/8u;
            uint8_t bit = (uint8_t)(0x80u>>(x&7u));
            EInkColor color = IngredientSpriteRed[item][offset]&bit ? EINK_RED :
                              IngredientSpriteBlack[item][offset]&bit ? EINK_BLACK : EINK_WHITE;
            (void)EInk_Pixel(canvas,8+item*58+x,40+row,color);
        }
        if (++row == 48) { row=0; ++item; }
        return false;
    }
    if (title[character]) {
        uint8_t bits=getFontLine((unsigned char)title[character],row);
        for (int x=0;x<4;++x) if (bits&(1u<<(3-x)))
            for (int dy=0;dy<3;++dy) for (int dx=0;dx<3;++dx)
                (void)EInk_Pixel(canvas,left+character*12+x*3+dx,8+row*3+dy,EINK_BLACK);
        if (++row == 6) { row=0; ++character; }
        return false;
    }
    return complete=true;
}
