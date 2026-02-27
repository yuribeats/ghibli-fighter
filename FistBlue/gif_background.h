#ifndef GIF_BACKGROUND_H
#define GIF_BACKGROUND_H

void gif_bg_init(void);
void gif_bg_load_stage(int stage_id);
void gif_bg_update(void);
void gif_bg_draw(void);
int  gif_bg_is_active(void);
void gif_bg_load_charselect(void);
void gif_bg_draw_charselect(void);
int  gif_bg_charselect_active(void);

#endif
