#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NORMAL_INTERVAL 4
#define URGENT_INTERVAL 1
#define NUM_CPUS 8
#define NUM_KBD_BL_LEVELS 4

static const unsigned kbd_bl_thresh[NUM_KBD_BL_LEVELS] = {
  0,
  2000000, /* 2.0 GHz, generally indicative of meaningful work */
  2500000, /* 2.5 GHz */
  3000000, /* 3.0 GHz */
};

static int get_battery_lights(int*);
static void set_wifi_light(int);
static void set_num_and_caps_lights(int);
static void update_keyboard_backlight(void);
static unsigned get_max_cpu_freq(void);
static unsigned get_kb_bl_level(unsigned);
static void set_kb_bl_level(unsigned);

int main(void) {
  int battery_lights;
  int urgent;

  /* Need to explicitly toggle the WiFi light before it will respond. */
  set_wifi_light(0);
  set_wifi_light(1);

  while (1) {
    battery_lights = get_battery_lights(&urgent);
    set_wifi_light(!!(battery_lights & 4));
    set_num_and_caps_lights(battery_lights & 0x3);

    update_keyboard_backlight();

    sleep(urgent? URGENT_INTERVAL : NORMAL_INTERVAL);
  }
}

static int get_battery_lights(int* urgent) {
  FILE* in;
  unsigned long current, max;
  unsigned per256;
  static int blink = 0;

  in = fopen("/sys/class/power_supply/BAT0/energy_full", "r");
  fscanf(in, "%ld", &max);
  fclose(in);

  in = fopen("/sys/class/power_supply/BAT0/energy_now", "r");
  fscanf(in, "%ld", &current);
  fclose(in);

  per256 = current / (max >> 8);
  *urgent = 0;
  if (per256 == 256)
    /* Special case: full */
    return 0x7;
  else if (per256 > 16)
    /* General case: Binary indicator */
    return per256 >> 5;
  else {
    /* Special case: Very low power; blink */
    *urgent = 1;
    blink = !blink;
    return blink? 0x7 : 0x0;
  }
}
static void set_wifi_light(int on) {
  FILE* out = fopen("/sys/class/leds/asus::wlan/brightness", "w");
  fprintf(out, "%d\n", on);
  fclose(out);
}

static void set_num_and_caps_lights(int lights) {
  char command[32];
  sprintf(command, "xset %cled 2 %cled 1",
          (lights & 2)? ' ' : '-',
          (lights & 1)? ' ' : '-');
  system(command);
}

static void update_keyboard_backlight(void) {
  unsigned max_freq = get_max_cpu_freq();
  unsigned level = get_kb_bl_level(max_freq);
  set_kb_bl_level(level);
}

static unsigned get_max_cpu_freq(void) {
  char filename[64];
  unsigned max = 0;
  unsigned freq, i;
  FILE* in;

  for (i = 0; i < NUM_CPUS; ++i) {
    sprintf(filename, "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_cur_freq", i);
    in = fopen(filename, "r");
    fscanf(in, "%d", &freq);
    fclose(in);

    if (freq > max)
      max = freq;
  }

  return max;
}

static unsigned get_kb_bl_level(unsigned freq) {
  unsigned i;

  for (i = 0; i < NUM_KBD_BL_LEVELS; ++i)
    if (freq < kbd_bl_thresh[i])
      return i - 1;

  return i - 1;
}

static void set_kb_bl_level(unsigned level) {
  FILE* out = fopen("/sys/class/leds/asus::kbd_backlight/brightness", "w");
  fprintf(out, "%d\n", level);
  fclose(out);
}
