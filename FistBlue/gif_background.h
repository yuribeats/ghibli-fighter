#ifndef GIF_BACKGROUND_H
#define GIF_BACKGROUND_H

void gif_bg_init(void);
void gif_bg_load_stage(int stage_id);
void gif_bg_update(void);
void gif_bg_draw(void);
int  gif_bg_is_active(void);
void gif_bg_load_charselect(void);
void gif_bg_draw_charselect(void);
void gif_bg_draw_charselect_mask(void);
int  gif_bg_charselect_active(void);
void gif_bg_load_vs_screen(void);
void gif_bg_draw_vs_screen(void);
int  gif_bg_vs_screen_active(void);
void gif_bg_load_title(void);
void gif_bg_draw_title(void);
int  gif_bg_title_active(void);

#endif
