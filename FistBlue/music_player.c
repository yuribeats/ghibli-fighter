/* music_player.c - Play custom music using macOS afplay */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include "music_player.h"

static pid_t music_pid = 0;
static double music_start_time = 0.0;

static void signal_handler(int sig) {
    music_player_stop();
    _exit(0);
}

__attribute__((constructor))
static void install_signal_handlers(void) {
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    signal(SIGHUP, signal_handler);
}

static double now_seconds(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

void music_player_init(void)
{
    music_pid = 0;
    music_start_time = 0.0;
}

void music_player_stop(void)
{
    if (music_pid > 0) {
        kill(music_pid, SIGTERM);
        music_pid = 0;
    }
    music_start_time = 0.0;
}

void music_player_play(const char *filepath)
{
    music_player_stop();

    pid_t pid = fork();
    if (pid == 0) {
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        execlp("afplay", "afplay", filepath, NULL);
        _exit(1);
    } else if (pid > 0) {
        music_pid = pid;
        music_start_time = now_seconds();
        printf("music_player: playing %s (pid %d)\n", filepath, pid);
    }
}

double music_player_elapsed(void)
{
    if (music_start_time <= 0.0) return 999.0;
    return now_seconds() - music_start_time;
}
