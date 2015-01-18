
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <getopt.h>
#include <libudev.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/hidraw.h>

#define SUNKBD_LAYOUT_5_MASK 0x20

static const char *VENDOR = "23fd", *PRODUCT = "206a";
static bool find_sunkbd(char *device)
{
  struct udev *udev;
  struct udev_enumerate *enumerate;
  struct udev_list_entry *devices, *dev_list_entry;

  udev = udev_new();
  if (udev == NULL) {
    fprintf(stderr, "Cannot create udev.\n");
    return false;
  }

  enumerate = udev_enumerate_new(udev);
  udev_enumerate_add_match_subsystem(enumerate, "hidraw");
  udev_enumerate_scan_devices(enumerate);
  devices = udev_enumerate_get_list_entry(enumerate);

  udev_list_entry_foreach(dev_list_entry, devices) {
    const char *syspath, *devpath;
    struct udev_device *hiddev, *usbdev;

    syspath = udev_list_entry_get_name(dev_list_entry);
    hiddev = udev_device_new_from_syspath(udev, syspath);
    devpath = udev_device_get_devnode(hiddev);

    usbdev = udev_device_get_parent_with_subsystem_devtype(hiddev, "usb", "usb_device");
    if (usbdev == NULL) {
      fprintf(stderr, "Cannot find parent USB device.\n");
      return false;
    }

    if (!strcmp(VENDOR, udev_device_get_sysattr_value(usbdev, "idVendor")) &&
        !strcmp(PRODUCT, udev_device_get_sysattr_value(usbdev, "idProduct"))) {
      if (device[0] != '\0') {
        fprintf(stderr, "Found more than one keyboard. Need to specify one.\n");
        return false;
      }
      strncpy(device, devpath, PATH_MAX-1);
    }

    udev_device_unref(hiddev);
  }

  udev_enumerate_unref(enumerate);
  udev_unref(udev);

  if (device[0] == '\0') {
    fprintf(stderr, "Keyboard not found.\n");
    return false;
  }
  return true;
}

static char device[PATH_MAX] = { 0 };
static int click = -1;

static struct option long_options[] = {
  {"click", no_argument, &click, 1},
  {"no-click", no_argument, &click, 0},
  {NULL, 0, 0, 0}
};

#define countof(x) (sizeof(x)/sizeof(x[0]))

int main(int argc, char **argv)
{
  while (true) {
    int option_index = 0;
    int c = getopt_long(argc, argv, "d:cn",
                        long_options, &option_index);

    if (c < 0) break;

    if (c == 0) {
      if (long_options[option_index].flag != 0) continue;
      c = long_options[option_index].val;
    }

    switch (c) {
    case 'd':
      if (optarg[0] == '/') {
        strncpy(device, optarg, sizeof(device)-1);
      }
      else {
        snprintf(device, sizeof(device)-1, "/dev/hidraw%s", optarg);
      }
      break;

    case 'c':
      click = 1;
      break;

    case 'n':
      click = 0;
      break;

    case '?':
    default:
      printf("Usage: %s [--device num] [--click] [--no-click]\n", argv[0]);
      return 1;
    }
  }

  if (device[0] == '\0') {
    if (!find_sunkbd(device)) return 1;
  }

  int fd, rc;
  unsigned char buf[3];
  fd = open(device, O_RDWR|O_NONBLOCK);
  if (fd < 0) {
    perror("Unable to open device");
    return 1;
  }
  
  buf[0] = 0;
  rc = ioctl(fd, HIDIOCGFEATURE(3), buf);
  if (rc < 0) {
    perror("Error getting feature report");
    return 1;
  }
  if (rc != 3) {
    fprintf(stderr, "Incorrect feature report: %d", rc);
    return 1;
  }

  // http://docs.oracle.com/cd/E19253-01/817-2521/new-311/index.html#indexterm-82
  // Changing Between Keyboards on SPARC Systems

  const char *layout;
  switch (buf[1]) {
  case 0x00:
  case 0x01:
    layout = "Type 4 / United States";
    break;
  case 0x02:
    layout = "Type 4 / Belgium / French";
    break;
  case 0x03:
    layout = "Type 4 / Canada / French";
    break;
  case 0x04:
    layout = "Type 4 / Denmark";
    break;
  case 0x05:
    layout = "Type 4 / Germany";
    break;
  case 0x06:
    layout = "Type 4 / Italy";
    break;
  case 0x07:
    layout = "Type 4 / Netherlands";
    break;
  case 0x08:
    layout = "Type 4 / Norway";
    break;
  case 0x09:
    layout = "Type 4 / Portugal";
    break;
  case 0x0A:
    layout = "Type 4 / America / Spanish";
    break;
  case 0x0B:
    layout = "Type 4 / Sweden, Finland";
    break;
  case 0x0C:
    layout = "Type 4 / Switzerland / French";
    break;
  case 0x0D:
    layout = "Type 4 / Switzerland / German";
    break;
  case 0x0E:
    layout = "Type 4 / Great Britain";
    break;
  case 0x10:
    layout = "Type 4 / Korea";
    break;
  case 0x11:
    layout = "Type 4 / Taiwan";
    break;
  case 0x17:
    layout = "Type 4 / Russia";
    break;
  case 0x21:
    layout = "Type 5 / United States";
    break;
  case 0x22:
    layout = "Type 5 / United States / UNIX";
    break;
  case 0x23:
    layout = "Type 5 / France";
    break;
  case 0x24:
    layout = "Type 5 / Denmark";
    break;
  case 0x25:
    layout = "Type 5 / Germany";
    break;
  case 0x26:
    layout = "Type 5 / Italy";
    break;
  case 0x27:
    layout = "Type 5 / Netherlands";
    break;
  case 0x28:
    layout = "Type 5 / Norway";
    break;
  case 0x29:
    layout = "Type 5 / Portugal";
    break;
  case 0x2A:
    layout = "Type 5 / Spain";
    break;
  case 0x2B:
    layout = "Type 5 / Sweden";
    break;
  case 0x2C:
    layout = "Type 5 / Switzerland / French";
    break;
  case 0x2D:
    layout = "Type 5 / Switzerland / German";
    break;
  case 0x2E:
    layout = "Type 5 / Great Britain";
    break;
  case 0x2F:
    layout = "Type 5 / Korea";
    break;
  case 0x30:
    layout = "Type 5 / Taiwan";
    break;
  case 0x31:
    layout = "Type 5 / Japan";
    break;
  case 0x32:
    layout = "Type 5 / Canada / French";
    break;
  case 0x33:
    layout = "Type 5 / Hungary";
    break;
  case 0x34:
    layout = "Type 5 / Poland";
    break;
  case 0x35:
    layout = "Type 5 / Czech";
    break;
  case 0x36:
    layout = "Type 5 / Russia";
    break;
  case 0x37:
    layout = "Type 5 / Latvia";
    break;
  case 0x38:
    layout = "Type 5 / Turkey";
    break;
  case 0x39:
    layout = "Type 5 / Greece";
    break;
  case 0x3A:
    layout = "Type 5 / Arabic";
    break;
  case 0x3B:
    layout = "Type 5 / Lithuania";
    break;
  case 0x3C:
    layout = "Type 5 / Belgium";
    break;
  case 0x3E:
    layout = "Type 5 / Canada / French";
    break;
  default:
    layout = "Unknown";
    break;
  }
  printf("Layout = %02X (%s)\n", buf[1], layout);

  do {
    if (click != -1) {
      buf[2] = (unsigned char)click;
    }
    else {
      break;
    }

    rc = ioctl(fd, HIDIOCSFEATURE(3), buf);
    if (rc < 0) {
      perror("Error setting feature report");
      return 1;
    }
  } while(false);

  printf("Click = %s\n", buf[2] ? "on" : "off");

  return 0;
}
