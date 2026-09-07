#ifndef WINDOWCTL_H
#define WINDOWCTL_H

bool should_spawn_window(int argc, char* argv[]);
void setup_lxterminal_cascade_profile();
void open_in_new_fullscreen_window(int argc, char* argv[]);
void wait_for_fullscreen();

#endif // WINDOWCTL_H
