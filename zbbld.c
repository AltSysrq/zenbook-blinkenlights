#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NORMAL_INTERVAL 4
#define URGENT_INTERVAL 1
#define NUM_CPUS 8
#define NUM_KBD_BL_LEVELS 4

static unsigned long prev_idle_ticks, prev_total_ticks;

static int get_battery_lights(int*);
static void set_wifi_light(int);
static void set_num_and_caps_lights(int);
static void update_keyboard_backlight(void);
static unsigned get_cpu_utilisation(void);
static unsigned get_kb_bl_level(unsigned);
static void set_kb_bl_level(unsigned);

int main(void) {
  int battery_lights;
  int urgent;

  /* Need to explicitly toggle the WiFi light before it will respond. */
  set_wifi_light(0);
  set_wifi_light(1);

  /* Init CPU counts */
  get_cpu_utilisation();

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
  if (!in) goto blink;

  fscanf(in, "%ld", &max);
  fclose(in);

  in = fopen("/sys/class/power_supply/BAT0/energy_now", "r");
  if (!in) goto blink;
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
    blink:
    /* Special case: Very low power; blink */
    *urgent = 1;
    blink = !blink;
    return blink? 0x7 : 0x0;
  }
}
static void set_wifi_light(int on) {
  FILE* out = fopen("/sys/class/leds/asus::wlan/brightness", "w");
  if (!out) return;
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

static unsigned get_cpu_utilisation(void) {
  unsigned long user, nice, system, idle, io, irq, softirq, total;
  unsigned long idle_elapsed, total_elapsed;
  unsigned ret;
  FILE* in = fopen("/proc/stat", "r");
  if (!in) return 0;

  fscanf(in, "cpu %ld %ld %ld %ld %ld %ld %ld",
         &user, &nice, &system, &idle, &io, &irq, &softirq);
  fclose(in);

  total = user + nice + system + idle + io + irq + softirq;

  idle_elapsed = idle - prev_idle_ticks;
  total_elapsed = total - prev_total_ticks;

  prev_idle_ticks = idle;
  prev_total_ticks = total;

  ret = 256 - idle_elapsed * 256 / total_elapsed;
  ret *= NUM_CPUS;
  return ret >= 256? 255 : ret;
}

static void update_keyboard_backlight(void) {
  unsigned per256 = get_cpu_utilisation();
  unsigned level = get_kb_bl_level(per256);
  set_kb_bl_level(level);
}

static unsigned get_kb_bl_level(unsigned per256) {
  return per256 * NUM_KBD_BL_LEVELS / 256;
}

static void set_kb_bl_level(unsigned level) {
  FILE* out = fopen("/sys/class/leds/asus::kbd_backlight/brightness", "w");
  fprintf(out, "%d\n", level);
  fclose(out);
}
