#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/uinput.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define SYS_INPUT_DIR "/sys/devices/virtual/input/"

struct control_set {
  bool ctrl;
  bool shift;
  bool meta;
  bool alt;
};

typedef struct control_set control_set_t;

control_set_t meta_codes(char **remaining);
static char *fetch_device_node(const char *path);
static int fetch_syspath_and_devnode(int fd, char **syspath, char **devnode);
static int is_event_device(const struct dirent *dent);
static void ensure_device(int fd);
static void usage();
void emit(int fd, int type, int code, int val);
void emit_cmd(int fd, char *cmd);
void emit_cset(int fd, control_set_t cset, int val);
void emit_key(int fd, int code);

void emit(int fd, int type, int code, int val) {
  struct input_event ie;

  ie.type = type;
  ie.code = code;
  ie.value = val;
  /* timestamp values below are ignored */
  ie.time.tv_sec = 0;
  ie.time.tv_usec = 0;

  write(fd, &ie, sizeof(ie));
}

void emit_cset(int fd, control_set_t cset, int val) {
  if (cset.shift) {
    emit(fd, EV_KEY, KEY_LEFTSHIFT, val);
    emit(fd, EV_SYN, SYN_REPORT, 0);
  }
  if (cset.ctrl) {
    emit(fd, EV_KEY, KEY_RIGHTCTRL, val);
    emit(fd, EV_SYN, SYN_REPORT, 0);
  }
  if (cset.meta) {
    emit(fd, EV_KEY, KEY_RIGHTMETA, val);
    emit(fd, EV_SYN, SYN_REPORT, 0);
  }
  if (cset.alt) {
    emit(fd, EV_KEY, KEY_RIGHTALT, val);
    emit(fd, EV_SYN, SYN_REPORT, 0);
  }
}

control_set_t meta_codes(char **remaining) {
  control_set_t cset;
  memset(&cset, 0, sizeof(control_set_t));

  while(strlen(*remaining) > 2) {
    char letter = (*remaining)[0];
    char dash = (*remaining)[1];
    if (dash != '-') {
      return cset;
    }
    switch (letter) {
      case 'C':
        cset.ctrl = true;
        break;
      case 'S':
        cset.shift = true;
        break;
      case 's':
        cset.meta = true;
        break;
      case 'M':
        cset.alt = true;
        break;
      default:
        return cset;
    }

    *remaining = (*remaining) + 2;
  }

  return cset;
}


void emit_key(int fd, int code) {
  emit(fd, EV_KEY, code, 1);
  emit(fd, EV_SYN, SYN_REPORT, 0);
  emit(fd, EV_KEY, code, 0);
  emit(fd, EV_SYN, SYN_REPORT, 0);
}

int parse_special_code(char *cmd) {
  int single_code = -1;

  if (strcmp(cmd, "<space>") == 0) {
    single_code = KEY_SPACE;
  } else if (strcmp(cmd, "<esc>") == 0) {
    single_code = KEY_ESC;
  } else if (strcmp(cmd, "<tab>") == 0) {
    single_code = KEY_TAB;
  } else if (strcmp(cmd, "<return>") == 0
             || strcmp(cmd, "<enter>") == 0
             || strcmp(cmd, "<ret>") == 0) {
    single_code = KEY_ENTER;
  } else if (strcmp(cmd, "<ctrl>") == 0
             || strcmp(cmd, "<control>") == 0) {
    single_code = KEY_RIGHTCTRL;
  } else if (strcmp(cmd, "<shift>") == 0) {
    single_code = KEY_RIGHTSHIFT;
  } else if (strcmp(cmd, "<alt>") == 0) {
    single_code = KEY_RIGHTALT;
  } else if (strcmp(cmd, "<capslock>") == 0) {
    single_code = KEY_CAPSLOCK;
  } else if (strcmp(cmd, "<f1>") == 0) {
    single_code = KEY_F1;
  } else if (strcmp(cmd, "<f2>") == 0) {
    single_code = KEY_F2;
  } else if (strcmp(cmd, "<f3>") == 0) {
    single_code = KEY_F3;
  } else if (strcmp(cmd, "<f4>") == 0) {
    single_code = KEY_F4;
  } else if (strcmp(cmd, "<f5>") == 0) {
    single_code = KEY_F5;
  } else if (strcmp(cmd, "<f6>") == 0) {
    single_code = KEY_F6;
  } else if (strcmp(cmd, "<f7>") == 0) {
    single_code = KEY_F7;
  } else if (strcmp(cmd, "<f8>") == 0) {
    single_code = KEY_F8;
  } else if (strcmp(cmd, "<f9>") == 0) {
    single_code = KEY_F9;
  } else if (strcmp(cmd, "<f10>") == 0) {
    single_code = KEY_F10;
  } else if (strcmp(cmd, "<f11>") == 0) {
    single_code = KEY_F11;
  } else if (strcmp(cmd, "<f12>") == 0) {
    single_code = KEY_F12;
  } else if (strcmp(cmd, "<f13>") == 0) {
    single_code = KEY_F13;
  } else if (strcmp(cmd, "<f14>") == 0) {
    single_code = KEY_F14;
  } else if (strcmp(cmd, "<f15>") == 0) {
    single_code = KEY_F15;
  } else if (strcmp(cmd, "<f16>") == 0) {
    single_code = KEY_F16;
  } else if (strcmp(cmd, "<f17>") == 0) {
    single_code = KEY_F17;
  } else if (strcmp(cmd, "<f18>") == 0) {
    single_code = KEY_F18;
  } else if (strcmp(cmd, "<f19>") == 0) {
    single_code = KEY_F19;
  } else if (strcmp(cmd, "<f20>") == 0) {
    single_code = KEY_F20;
  } else if (strcmp(cmd, "<up>") == 0) {
    single_code = KEY_UP;
  } else if (strcmp(cmd, "<down>") == 0) {
    single_code = KEY_DOWN;
  } else if (strcmp(cmd, "<left>") == 0) {
    single_code = KEY_LEFT;
  } else if (strcmp(cmd, "<right>") == 0) {
    single_code = KEY_RIGHT;
  } else if (strcmp(cmd, "<rightctrl>") == 0) {
    single_code = KEY_RIGHTCTRL;
  } else if (strcmp(cmd, "<kpasterisk>") == 0) {
    single_code = KEY_KPASTERISK;
  } else if (strcmp(cmd, "<leftalt>") == 0) {
    single_code = KEY_LEFTALT;
  } else if (strcmp(cmd, "<space>") == 0) {
    single_code = KEY_SPACE;
  } else if (strcmp(cmd, "<capslock>") == 0) {
    single_code = KEY_CAPSLOCK;
  } else if (strcmp(cmd, "<numlock>") == 0) {
    single_code = KEY_NUMLOCK;
  } else if (strcmp(cmd, "<scrolllock>") == 0) {
    single_code = KEY_SCROLLLOCK;
  } else if (strcmp(cmd, "<kp7>") == 0) {
    single_code = KEY_KP7;
  } else if (strcmp(cmd, "<kp8>") == 0) {
    single_code = KEY_KP8;
  } else if (strcmp(cmd, "<kp9>") == 0) {
    single_code = KEY_KP9;
  } else if (strcmp(cmd, "<kpminus>") == 0) {
    single_code = KEY_KPMINUS;
  } else if (strcmp(cmd, "<kp4>") == 0) {
    single_code = KEY_KP4;
  } else if (strcmp(cmd, "<kp5>") == 0) {
    single_code = KEY_KP5;
  } else if (strcmp(cmd, "<kp6>") == 0) {
    single_code = KEY_KP6;
  } else if (strcmp(cmd, "<kpplus>") == 0) {
    single_code = KEY_KPPLUS;
  } else if (strcmp(cmd, "<kp1>") == 0) {
    single_code = KEY_KP1;
  } else if (strcmp(cmd, "<kp2>") == 0) {
    single_code = KEY_KP2;
  } else if (strcmp(cmd, "<kp3>") == 0) {
    single_code = KEY_KP3;
  } else if (strcmp(cmd, "<kp0>") == 0) {
    single_code = KEY_KP0;
  } else if (strcmp(cmd, "<kpdot>") == 0) {
    single_code = KEY_KPDOT;
  } else if (strcmp(cmd, "<zenkakuhankaku>") == 0) {
    single_code = KEY_ZENKAKUHANKAKU;
  } else if (strcmp(cmd, "<102nd>") == 0) {
    single_code = KEY_102ND;
  } else if (strcmp(cmd, "<ro>") == 0) {
    single_code = KEY_RO;
  } else if (strcmp(cmd, "<katakana>") == 0) {
    single_code = KEY_KATAKANA;
  } else if (strcmp(cmd, "<hiragana>") == 0) {
    single_code = KEY_HIRAGANA;
  } else if (strcmp(cmd, "<henkan>") == 0) {
    single_code = KEY_HENKAN;
  } else if (strcmp(cmd, "<katakanahiragana>") == 0) {
    single_code = KEY_KATAKANAHIRAGANA;
  } else if (strcmp(cmd, "<muhenkan>") == 0) {
    single_code = KEY_MUHENKAN;
  } else if (strcmp(cmd, "<kpjpcomma>") == 0) {
    single_code = KEY_KPJPCOMMA;
  } else if (strcmp(cmd, "<kpenter>") == 0) {
    single_code = KEY_KPENTER;
  } else if (strcmp(cmd, "<kpslash>") == 0) {
    single_code = KEY_KPSLASH;
  } else if (strcmp(cmd, "<sysrq>") == 0) {
    single_code = KEY_SYSRQ;
  } else if (strcmp(cmd, "<rightalt>") == 0) {
    single_code = KEY_RIGHTALT;
  } else if (strcmp(cmd, "<linefeed>") == 0) {
    single_code = KEY_LINEFEED;
  } else if (strcmp(cmd, "<home>") == 0) {
    single_code = KEY_HOME;
  } else if (strcmp(cmd, "<up>") == 0) {
    single_code = KEY_UP;
  } else if (strcmp(cmd, "<pageup>") == 0) {
    single_code = KEY_PAGEUP;
  } else if (strcmp(cmd, "<left>") == 0) {
    single_code = KEY_LEFT;
  } else if (strcmp(cmd, "<right>") == 0) {
    single_code = KEY_RIGHT;
  } else if (strcmp(cmd, "<end>") == 0) {
    single_code = KEY_END;
  } else if (strcmp(cmd, "<down>") == 0) {
    single_code = KEY_DOWN;
  } else if (strcmp(cmd, "<pagedown>") == 0) {
    single_code = KEY_PAGEDOWN;
  } else if (strcmp(cmd, "<insert>") == 0) {
    single_code = KEY_INSERT;
  } else if (strcmp(cmd, "<delete>") == 0) {
    single_code = KEY_DELETE;
  } else if (strcmp(cmd, "<macro>") == 0) {
    single_code = KEY_MACRO;
  } else if (strcmp(cmd, "<mute>") == 0) {
    single_code = KEY_MUTE;
  } else if (strcmp(cmd, "<volumedown>") == 0) {
    single_code = KEY_VOLUMEDOWN;
  } else if (strcmp(cmd, "<volumeup>") == 0) {
    single_code = KEY_VOLUMEUP;
  } else if (strcmp(cmd, "<power>") == 0) {
    single_code = KEY_POWER;
  } else if (strcmp(cmd, "<kpequal>") == 0) {
    single_code = KEY_KPEQUAL;
  } else if (strcmp(cmd, "<kpplusminus>") == 0) {
    single_code = KEY_KPPLUSMINUS;
  } else if (strcmp(cmd, "<pause>") == 0) {
    single_code = KEY_PAUSE;
  } else if (strcmp(cmd, "<scale>") == 0) {
    single_code = KEY_SCALE;
  } else if (strcmp(cmd, "<kpcomma>") == 0) {
    single_code = KEY_KPCOMMA;
  } else if (strcmp(cmd, "<hangeul>") == 0) {
    single_code = KEY_HANGEUL;
  } else if (strcmp(cmd, "<hanguel>") == 0) {
    single_code = KEY_HANGUEL;
  } else if (strcmp(cmd, "<hanja>") == 0) {
    single_code = KEY_HANJA;
  } else if (strcmp(cmd, "<yen>") == 0) {
    single_code = KEY_YEN;
  } else if (strcmp(cmd, "<leftmeta>") == 0) {
    single_code = KEY_LEFTMETA;
  } else if (strcmp(cmd, "<rightmeta>") == 0) {
    single_code = KEY_RIGHTMETA;
  } else if (strcmp(cmd, "<compose>") == 0) {
    single_code = KEY_COMPOSE;
  } else if (strcmp(cmd, "<stop>") == 0) {
    single_code = KEY_STOP;
  } else if (strcmp(cmd, "<again>") == 0) {
    single_code = KEY_AGAIN;
  } else if (strcmp(cmd, "<props>") == 0) {
    single_code = KEY_PROPS;
  } else if (strcmp(cmd, "<undo>") == 0) {
    single_code = KEY_UNDO;
  } else if (strcmp(cmd, "<front>") == 0) {
    single_code = KEY_FRONT;
  } else if (strcmp(cmd, "<copy>") == 0) {
    single_code = KEY_COPY;
  } else if (strcmp(cmd, "<open>") == 0) {
    single_code = KEY_OPEN;
  } else if (strcmp(cmd, "<paste>") == 0) {
    single_code = KEY_PASTE;
  } else if (strcmp(cmd, "<find>") == 0) {
    single_code = KEY_FIND;
  } else if (strcmp(cmd, "<cut>") == 0) {
    single_code = KEY_CUT;
  } else if (strcmp(cmd, "<help>") == 0) {
    single_code = KEY_HELP;
  } else if (strcmp(cmd, "<menu>") == 0) {
    single_code = KEY_MENU;
  } else if (strcmp(cmd, "<calc>") == 0) {
    single_code = KEY_CALC;
  } else if (strcmp(cmd, "<setup>") == 0) {
    single_code = KEY_SETUP;
  } else if (strcmp(cmd, "<sleep>") == 0) {
    single_code = KEY_SLEEP;
  } else if (strcmp(cmd, "<wakeup>") == 0) {
    single_code = KEY_WAKEUP;
  } else if (strcmp(cmd, "<file>") == 0) {
    single_code = KEY_FILE;
  } else if (strcmp(cmd, "<sendfile>") == 0) {
    single_code = KEY_SENDFILE;
  } else if (strcmp(cmd, "<deletefile>") == 0) {
    single_code = KEY_DELETEFILE;
  } else if (strcmp(cmd, "<xfer>") == 0) {
    single_code = KEY_XFER;
  } else if (strcmp(cmd, "<prog1>") == 0) {
    single_code = KEY_PROG1;
  } else if (strcmp(cmd, "<prog2>") == 0) {
    single_code = KEY_PROG2;
  } else if (strcmp(cmd, "<www>") == 0) {
    single_code = KEY_WWW;
  } else if (strcmp(cmd, "<msdos>") == 0) {
    single_code = KEY_MSDOS;
  } else if (strcmp(cmd, "<coffee>") == 0) {
    single_code = KEY_COFFEE;
  } else if (strcmp(cmd, "<screenlock>") == 0) {
    single_code = KEY_SCREENLOCK;
  } else if (strcmp(cmd, "<rotate_display>") == 0) {
    single_code = KEY_ROTATE_DISPLAY;
  } else if (strcmp(cmd, "<direction>") == 0) {
    single_code = KEY_DIRECTION;
  } else if (strcmp(cmd, "<cyclewindows>") == 0) {
    single_code = KEY_CYCLEWINDOWS;
  } else if (strcmp(cmd, "<mail>") == 0) {
    single_code = KEY_MAIL;
  } else if (strcmp(cmd, "<bookmarks>") == 0) {
    single_code = KEY_BOOKMARKS;
  } else if (strcmp(cmd, "<computer>") == 0) {
    single_code = KEY_COMPUTER;
  } else if (strcmp(cmd, "<back>") == 0) {
    single_code = KEY_BACK;
  } else if (strcmp(cmd, "<forward>") == 0) {
    single_code = KEY_FORWARD;
  } else if (strcmp(cmd, "<closecd>") == 0) {
    single_code = KEY_CLOSECD;
  } else if (strcmp(cmd, "<ejectcd>") == 0) {
    single_code = KEY_EJECTCD;
  } else if (strcmp(cmd, "<ejectclosecd>") == 0) {
    single_code = KEY_EJECTCLOSECD;
  } else if (strcmp(cmd, "<nextsong>") == 0) {
    single_code = KEY_NEXTSONG;
  } else if (strcmp(cmd, "<playpause>") == 0) {
    single_code = KEY_PLAYPAUSE;
  } else if (strcmp(cmd, "<previoussong>") == 0) {
    single_code = KEY_PREVIOUSSONG;
  } else if (strcmp(cmd, "<stopcd>") == 0) {
    single_code = KEY_STOPCD;
  } else if (strcmp(cmd, "<record>") == 0) {
    single_code = KEY_RECORD;
  } else if (strcmp(cmd, "<rewind>") == 0) {
    single_code = KEY_REWIND;
  } else if (strcmp(cmd, "<phone>") == 0) {
    single_code = KEY_PHONE;
  } else if (strcmp(cmd, "<iso>") == 0) {
    single_code = KEY_ISO;
  } else if (strcmp(cmd, "<config>") == 0) {
    single_code = KEY_CONFIG;
  } else if (strcmp(cmd, "<homepage>") == 0) {
    single_code = KEY_HOMEPAGE;
  } else if (strcmp(cmd, "<refresh>") == 0) {
    single_code = KEY_REFRESH;
  } else if (strcmp(cmd, "<exit>") == 0) {
    single_code = KEY_EXIT;
  } else if (strcmp(cmd, "<move>") == 0) {
    single_code = KEY_MOVE;
  } else if (strcmp(cmd, "<edit>") == 0) {
    single_code = KEY_EDIT;
  } else if (strcmp(cmd, "<scrollup>") == 0) {
    single_code = KEY_SCROLLUP;
  } else if (strcmp(cmd, "<scrolldown>") == 0) {
    single_code = KEY_SCROLLDOWN;
  } else if (strcmp(cmd, "<kpleftparen>") == 0) {
    single_code = KEY_KPLEFTPAREN;
  } else if (strcmp(cmd, "<kprightparen>") == 0) {
    single_code = KEY_KPRIGHTPAREN;
  } else if (strcmp(cmd, "<new>") == 0) {
    single_code = KEY_NEW;
  } else if (strcmp(cmd, "<redo>") == 0) {
    single_code = KEY_REDO;
  } else if (strcmp(cmd, "<playcd>") == 0) {
    single_code = KEY_PLAYCD;
  } else if (strcmp(cmd, "<pausecd>") == 0) {
    single_code = KEY_PAUSECD;
  } else if (strcmp(cmd, "<prog3>") == 0) {
    single_code = KEY_PROG3;
  } else if (strcmp(cmd, "<prog4>") == 0) {
    single_code = KEY_PROG4;
  } else if (strcmp(cmd, "<dashboard>") == 0) {
    single_code = KEY_DASHBOARD;
  } else if (strcmp(cmd, "<suspend>") == 0) {
    single_code = KEY_SUSPEND;
  } else if (strcmp(cmd, "<close>") == 0) {
    single_code = KEY_CLOSE;
  } else if (strcmp(cmd, "<play>") == 0) {
    single_code = KEY_PLAY;
  } else if (strcmp(cmd, "<fastforward>") == 0) {
    single_code = KEY_FASTFORWARD;
  } else if (strcmp(cmd, "<bassboost>") == 0) {
    single_code = KEY_BASSBOOST;
  } else if (strcmp(cmd, "<print>") == 0) {
    single_code = KEY_PRINT;
  } else if (strcmp(cmd, "<hp>") == 0) {
    single_code = KEY_HP;
  } else if (strcmp(cmd, "<camera>") == 0) {
    single_code = KEY_CAMERA;
  } else if (strcmp(cmd, "<sound>") == 0) {
    single_code = KEY_SOUND;
  } else if (strcmp(cmd, "<question>") == 0) {
    single_code = KEY_QUESTION;
  } else if (strcmp(cmd, "<email>") == 0) {
    single_code = KEY_EMAIL;
  } else if (strcmp(cmd, "<chat>") == 0) {
    single_code = KEY_CHAT;
  } else if (strcmp(cmd, "<search>") == 0) {
    single_code = KEY_SEARCH;
  } else if (strcmp(cmd, "<connect>") == 0) {
    single_code = KEY_CONNECT;
  } else if (strcmp(cmd, "<finance>") == 0) {
    single_code = KEY_FINANCE;
  } else if (strcmp(cmd, "<sport>") == 0) {
    single_code = KEY_SPORT;
  } else if (strcmp(cmd, "<shop>") == 0) {
    single_code = KEY_SHOP;
  } else if (strcmp(cmd, "<alterase>") == 0) {
    single_code = KEY_ALTERASE;
  } else if (strcmp(cmd, "<cancel>") == 0) {
    single_code = KEY_CANCEL;
  } else if (strcmp(cmd, "<brightnessdown>") == 0) {
    single_code = KEY_BRIGHTNESSDOWN;
  } else if (strcmp(cmd, "<brightnessup>") == 0) {
    single_code = KEY_BRIGHTNESSUP;
  } else if (strcmp(cmd, "<media>") == 0) {
    single_code = KEY_MEDIA;
  } else if (strcmp(cmd, "<switchvideomode>") == 0) {
    single_code = KEY_SWITCHVIDEOMODE;
  } else if (strcmp(cmd, "<kbdillumtoggle>") == 0) {
    single_code = KEY_KBDILLUMTOGGLE;
  } else if (strcmp(cmd, "<kbdillumdown>") == 0) {
    single_code = KEY_KBDILLUMDOWN;
  } else if (strcmp(cmd, "<kbdillumup>") == 0) {
    single_code = KEY_KBDILLUMUP;
  } else if (strcmp(cmd, "<send>") == 0) {
    single_code = KEY_SEND;
  } else if (strcmp(cmd, "<reply>") == 0) {
    single_code = KEY_REPLY;
  } else if (strcmp(cmd, "<forwardmail>") == 0) {
    single_code = KEY_FORWARDMAIL;
  } else if (strcmp(cmd, "<save>") == 0) {
    single_code = KEY_SAVE;
  } else if (strcmp(cmd, "<documents>") == 0) {
    single_code = KEY_DOCUMENTS;
  } else if (strcmp(cmd, "<battery>") == 0) {
    single_code = KEY_BATTERY;
  } else if (strcmp(cmd, "<bluetooth>") == 0) {
    single_code = KEY_BLUETOOTH;
  } else if (strcmp(cmd, "<wlan>") == 0) {
    single_code = KEY_WLAN;
  } else if (strcmp(cmd, "<uwb>") == 0) {
    single_code = KEY_UWB;
  } else if (strcmp(cmd, "<unknown>") == 0) {
    single_code = KEY_UNKNOWN;
  } else if (strcmp(cmd, "<video_next>") == 0) {
    single_code = KEY_VIDEO_NEXT;
  } else if (strcmp(cmd, "<video_prev>") == 0) {
    single_code = KEY_VIDEO_PREV;
  } else if (strcmp(cmd, "<brightness_cycle>") == 0) {
    single_code = KEY_BRIGHTNESS_CYCLE;
  } else if (strcmp(cmd, "<brightness_auto>") == 0) {
    single_code = KEY_BRIGHTNESS_AUTO;
  } else if (strcmp(cmd, "<brightness_zero>") == 0) {
    single_code = KEY_BRIGHTNESS_ZERO;
  } else if (strcmp(cmd, "<display_off>") == 0) {
    single_code = KEY_DISPLAY_OFF;
  } else if (strcmp(cmd, "<wwan>") == 0) {
    single_code = KEY_WWAN;
  } else if (strcmp(cmd, "<wimax>") == 0) {
    single_code = KEY_WIMAX;
  } else if (strcmp(cmd, "<rfkill>") == 0) {
    single_code = KEY_RFKILL;
  } else if (strcmp(cmd, "<micmute>") == 0) {
    single_code = KEY_MICMUTE;
  }

  return single_code;
}

void emit_cmd(int fd, char *cmd) {
  control_set_t cset;
  memset(&cset, 0, sizeof(control_set_t));
  int single_code = 0;
  if (strlen(cmd) > 1) {
    cset = meta_codes(&cmd);
    if (strlen(cmd) > 1) {
      single_code = parse_special_code(cmd);
      if (single_code < 0) {
        printf("Failed to parse code: %s", cmd);
        return;
      }
    }
  }

  switch (cmd[0]) {
    case 'A':
      cset.shift = true;
    case 'a':
      single_code = KEY_A;
      break;
    case 'B':
      cset.shift = true;
    case 'b':
      single_code = KEY_B;
      break;
    case 'C':
      cset.shift = true;
    case 'c':
      single_code = KEY_C;
      break;
    case 'D':
      cset.shift = true;
    case 'd':
      single_code = KEY_D;
      break;
    case 'E':
      cset.shift = true;
    case 'e':
      single_code = KEY_E;
      break;
    case 'F':
      cset.shift = true;
    case 'f':
      single_code = KEY_F;
      break;
    case 'G':
      cset.shift = true;
    case 'g':
      single_code = KEY_G;
      break;
    case 'H':
      cset.shift = true;
    case 'h':
      single_code = KEY_H;
      break;
    case 'I':
      cset.shift = true;
    case 'i':
      single_code = KEY_I;
      break;
    case 'J':
      cset.shift = true;
    case 'j':
      single_code = KEY_J;
      break;
    case 'K':
      cset.shift = true;
    case 'k':
      single_code = KEY_K;
      break;
    case 'L':
      cset.shift = true;
    case 'l':
      single_code = KEY_L;
      break;
    case 'M':
      cset.shift = true;
    case 'm':
      single_code = KEY_M;
      break;
    case 'N':
      cset.shift = true;
    case 'n':
      single_code = KEY_N;
      break;
    case 'O':
      cset.shift = true;
    case 'o':
      single_code = KEY_O;
      break;
    case 'P':
      cset.shift = true;
    case 'p':
      single_code = KEY_P;
      break;
    case 'Q':
      cset.shift = true;
    case 'q':
      single_code = KEY_Q;
      break;
    case 'R':
      cset.shift = true;
    case 'r':
      single_code = KEY_R;
      break;
    case 'S':
      cset.shift = true;
    case 's':
      single_code = KEY_S;
      break;
    case 'T':
      cset.shift = true;
    case 't':
      single_code = KEY_T;
      break;
    case 'U':
      cset.shift = true;
    case 'u':
      single_code = KEY_U;
      break;
    case 'V':
      cset.shift = true;
    case 'v':
      single_code = KEY_V;
      break;
    case 'W':
      cset.shift = true;
    case 'w':
      single_code = KEY_W;
      break;
    case 'X':
      cset.shift = true;
    case 'x':
      single_code = KEY_X;
      break;
    case 'Y':
      cset.shift = true;
    case 'y':
      single_code = KEY_Y;
      break;
    case 'Z':
      cset.shift = true;
    case 'z':
      single_code = KEY_Z;
      break;
    case ' ':
      single_code = KEY_SPACE;
      break;
    case '1':
      single_code = KEY_1;
      break;
    case '2':
      single_code = KEY_2;
      break;
    case '3':
      single_code = KEY_3;
      break;
    case '4':
      single_code = KEY_4;
      break;
    case '5':
      single_code = KEY_5;
      break;
    case '6':
      single_code = KEY_6;
      break;
    case '7':
      single_code = KEY_7;
      break;
    case '8':
      single_code = KEY_8;
      break;
    case '9':
      single_code = KEY_9;
      break;
    case '0':
      single_code = KEY_0;
      break;
    case '-':
      single_code = KEY_MINUS;
      break;
    case '=':
      single_code = KEY_EQUAL;
      break;
    case '/':
      single_code = KEY_SLASH;
      break;
    case '[':
      single_code = KEY_LEFTBRACE;
      break;
    case ']':
      single_code = KEY_RIGHTBRACE;
      break;
    case ';':
      single_code = KEY_SEMICOLON;
      break;
    case '\'':
      single_code = KEY_APOSTROPHE;
      break;
    case '`':
      single_code = KEY_GRAVE;
      break;
    case '\\':
      single_code = KEY_BACKSLASH;
      break;
    case '<':
      cset.shift = true;
    case ',':
      single_code = KEY_COMMA;
      break;
    case '>':
      cset.shift = true;
    case '.':
      single_code = KEY_DOT;
      break;
  }

  emit_cset(fd, cset, 1);
  emit_key(fd, single_code);
  emit_cset(fd, cset, 0);
}

static int is_event_device(const struct dirent *dent) {
        return strncmp("event", dent->d_name, 5) == 0;
}

static char *fetch_device_node(const char *path) {
  char *devnode = calloc(1, 128);
  bool devnode_set = false;
  struct dirent **namelist;
  int ndev, i;

  ndev = scandir(path, &namelist, is_event_device, alphasort);
  if (ndev <= 0)
    return NULL;

  /* ndev should only ever be 1 */
  for (i = 0; i < ndev; i++) {
    if (!devnode_set && sprintf(devnode, "/dev/input/%s", namelist[i]->d_name) == -1) {
      devnode_set = false;
    } else {
      devnode_set = true;
    }
    free(namelist[i]);
  }

  free(namelist);

  return devnode;
}

static int fetch_syspath_and_devnode(int fd, char **syspath, char **devnode) {
  int rc;
  char buf[sizeof(SYS_INPUT_DIR) + 64] = SYS_INPUT_DIR;

  rc = ioctl(fd, UI_GET_SYSNAME(sizeof(buf) - strlen(SYS_INPUT_DIR)),
             &buf[strlen(SYS_INPUT_DIR)]);
  if (rc != -1) {
    *syspath = strdup(buf);
    *devnode = fetch_device_node(buf);
  }

  return *devnode ? 0 : -1;
}

static void ensure_sys_device(int fd) {
  struct uinput_setup usetup;
  char *devnode = NULL;
  char *syspath = NULL;

  int rc = fetch_syspath_and_devnode(fd, &syspath, &devnode);
  while (rc < 0) {
    ioctl(fd, UI_SET_EVBIT, EV_KEY);

    // 0 and 255 are reserved, highest I know of is KEY_MICMUTE
    for (int i = 1; i < KEY_MICMUTE; i++) {
      ioctl(fd, UI_SET_KEYBIT, i);
    }

    memset(&usetup, 0, sizeof(usetup));
    usetup.id.bustype = BUS_USB;
    usetup.id.vendor = 0x1234;
    usetup.id.product = 0x5678;
    strcpy(usetup.name, "youinput device");

    ioctl(fd, UI_DEV_SETUP, &usetup);
    ioctl(fd, UI_DEV_CREATE);
    rc = fetch_syspath_and_devnode(fd, &syspath, &devnode);
  }
}

static void usage(void) {
  printf("youniput <cmd>...\n");
}

int main(int argc, char **argv)
{
  int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
  if (fd == -1) {
    perror("/dev/uinput failed to open");
    return fd;
  }

  ensure_sys_device(fd);

  sleep(1);

  if (argc <= 1) {
    usage();
  }

  for (int i = 1; i < argc; i++) {
    emit_cmd(fd, argv[i]);
  }

  ioctl(fd, UI_DEV_DESTROY);
  close(fd);

  return 0;
}
