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

struct control_set {
  bool ctrl;
  bool shift;
  bool meta;
  bool alt;
};

typedef struct control_set control_set_t;

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

void emit_cmd(int fd, char *cmd) {
  control_set_t cset;
  memset(&cset, 0, sizeof(control_set_t));
  int single_code;
  if (strlen(cmd) > 1) {
    cset = meta_codes(&cmd);
    if (strlen(cmd) > 1) {
      // then it's a <special> key, or invalid
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

static void ensure_device(int fd) {
  struct uinput_setup usetup;
  char *devnode = NULL;
  char *syspath = NULL;
  int rc = fetch_syspath_and_devnode(fd, &syspath, &devnode);
  if (rc < 0) {

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

    // I'm not sure how to know when the creation is done?
    sleep(1);
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

  ensure_device(fd);

  if (argc <= 1) {
    usage();
  }

  for (int i = 0; i < arg, i++) {
    emit_cmd(fd, argv[i]);
  }

  return 0;
}
