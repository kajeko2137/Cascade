#include "windowctl.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdlib>
#include <unistd.h>
#include <limits.h>
#include <sys/ioctl.h>
#include <algorithm>

using namespace std;

bool should_spawn_window(int argc, char* argv[]) {
    if (getenv("CASCADE_WINDOW") != nullptr) {
        return false;
    }
    if (getenv("DISPLAY") == nullptr) {
        return false;
    }
    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "--in-window" || arg == "--no-window" || arg == "--inline") {
            return false;
        }
    }
    return true;
}

static bool get_resolution_xrandr(int& width, int& height) {
    FILE* fp = popen("xrandr --current 2>/dev/null", "r");
    if (!fp) return false;

    char buf[256];
    bool found = false;
    while (fgets(buf, sizeof(buf), fp)) {
        string s(buf);
        size_t pos = s.find("current ");
        if (pos != string::npos) {
            size_t x_pos = s.find(" x ", pos);
            if (x_pos != string::npos) {
                try {
                    width = stoi(s.substr(pos + 8, x_pos - (pos + 8)));
                    height = stoi(s.substr(x_pos + 3));
                    found = (width > 0 && height > 0);
                    break;
                } catch (...) {}
            }
        }
    }
    pclose(fp);
    return found;
}

static int get_font_scale_percent() {
    int width = 0, height = 0;
    if (!get_resolution_xrandr(width, height)) {
        return 150; // Fallback to 1080p scale
    }

    // 1080p is 1920x1080. If larger than 1080p, use 200%, otherwise 150%
    if (width > 1920 || height > 1080) {
        return 200;
    }
    return 150;
}

void setup_lxterminal_cascade_profile() {
    const char* home = getenv("HOME");
    if (!home) return;

    string config_dir = string(home) + "/.config/lxterminal";
    string default_conf = config_dir + "/lxterminal.conf";
    string cascade_conf = config_dir + "/lxterminal-cascade.conf";

    system(("mkdir -p \"" + config_dir + "\"").c_str());

    ifstream in(default_conf);
    if (!in.is_open()) {
        in.open("/usr/share/lxterminal/lxterminal.conf");
    }

    int scale_percent = get_font_scale_percent();
    int fallback_size = (scale_percent >= 200) ? 20 : 15;

    vector<string> lines;
    string line;
    bool found_font = false;

    while (getline(in, line)) {
        if (line.rfind("fontname=", 0) == 0) {
            string rest = line.substr(9);
            size_t last_space = rest.rfind(' ');
            if (last_space != string::npos) {
                string name = rest.substr(0, last_space);
                string size_str = rest.substr(last_space + 1);
                try {
                    int size = stoi(size_str);
                    int new_size = max(1, (size * scale_percent) / 100);
                    line = "fontname=" + name + " " + to_string(new_size);
                    found_font = true;
                } catch (...) {
                    line = "fontname=Monospace " + to_string(fallback_size);
                    found_font = true;
                }
            } else {
                line = "fontname=Monospace " + to_string(fallback_size);
                found_font = true;
            }
        }
        lines.push_back(line);
    }
    in.close();

    if (!found_font) {
        lines.push_back("[general]");
        lines.push_back("fontname=Monospace " + to_string(fallback_size));
    }

    ofstream out(cascade_conf);
    if (out.is_open()) {
        for (const auto& l : lines) {
            out << l << "\n";
        }
        out.close();
    }
}

void open_in_new_fullscreen_window(int argc, char* argv[]) {
    (void)argc;
    char exe_buf[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exe_buf, sizeof(exe_buf) - 1);
    string exe_path;
    if (len != -1) {
        exe_buf[len] = '\0';
        exe_path = exe_buf;
    } else {
        exe_path = argv[0];
    }

    string term_cmd = "";
    if (system("which lxterminal >/dev/null 2>&1") == 0) {
        setup_lxterminal_cascade_profile();
        term_cmd = "CASCADE_WINDOW=1 lxterminal --profile=cascade -t Cascade -e \"" + exe_path + " --in-window\" &";
    } else if (system("which x-terminal-emulator >/dev/null 2>&1") == 0) {
        term_cmd = "CASCADE_WINDOW=1 x-terminal-emulator -t Cascade -e \"" + exe_path + " --in-window\" &";
    } else if (system("which xterm >/dev/null 2>&1") == 0) {
        int scale_percent = get_font_scale_percent();
        int fs_size = (scale_percent >= 200) ? 20 : 15;
        term_cmd = "CASCADE_WINDOW=1 xterm -fa Monospace -fs " + to_string(fs_size) + " -fullscreen -title Cascade -e \"" + exe_path + " --in-window\" &";
    }

    if (!term_cmd.empty()) {
        int res = system(term_cmd.c_str());
        (void)res;
    }
}

void wait_for_fullscreen() {
    // Set terminal window title to Cascade
    cout << "\033]0;Cascade\007" << flush;

    // Retry icesh fullscreen until the window is mapped and resized past initial 80 cols
    for (int i = 0; i < 40; ++i) { // up to 800ms
        struct winsize w;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 80) {
            break;
        }
        system("icesh -n '^Cascade$' fullscreen activate >/dev/null 2>&1");
        usleep(20000); // 20ms
    }
    usleep(20000);
}
