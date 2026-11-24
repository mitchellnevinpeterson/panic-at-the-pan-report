#include "BenchCanvas.h"
#include "IngredientSprites.h"
#include <string.h>
static EInkFrame *frame;
static unsigned clearing,item,row;
static uint8_t mask;
void BenchCanvas_Start(EInkFrame *f,uint8_t selected) {
    frame=f; mask=selected; clearing=item=row=0;
}
bool BenchCanvas_Service(void) {
    if(clearing<sizeof *frame) {
        size_t count=sizeof *frame-clearing;
        if(count>64) count=64;
        memset((uint8_t *)frame+clearing,0,count); clearing+=(unsigned)count;
        return false;
    }
    if(item==INGREDIENT_SPRITE_COUNT) return true;
    if(mask&(1u<<item)) for(unsigned x=0;x<48;++x) {
        unsigned offset=row*6u+x/8u;
        uint8_t bit=(uint8_t)(0x80u>>(x&7u));
        EInkColor color=IngredientSpriteRed[item][offset]&bit ? EINK_RED :
            IngredientSpriteBlack[item][offset]&bit ? EINK_BLACK : EINK_WHITE;
        (void)EInk_Pixel(frame,8+item*58+x,40+row,color);
    }
    if(++row==48) { row=0; ++item; }
    return false;
}
