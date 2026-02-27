/* music_player.h - Play custom music files */
#ifndef MUSIC_PLAYER_H
#define MUSIC_PLAYER_H

void music_player_init(void);
void music_player_play(const char *filepath);
void music_player_stop(void);
double music_player_elapsed(void);

#endif
