#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BATTERY_POLL_INTERVAL 8
#define URGENT_INTERVAL 1

static int get_battery_lights(int*);
static void set_wifi_light(int);
static void set_num_and_caps_lights(int);

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

    sleep(urgent? URGENT_INTERVAL : BATTERY_POLL_INTERVAL);
  }
}

int get_battery_lights(int* urgent) {
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
void set_wifi_light(int on) {
  FILE* out = fopen("/sys/class/leds/asus::wlan/brightness", "w");
  fprintf(out, "%d\n", on);
  fclose(out);
}

void set_num_and_caps_lights(int lights) {
  char command[32];
  sprintf(command, "xset %cled 2 %cled 1",
          (lights & 2)? ' ' : '-',
          (lights & 1)? ' ' : '-');
  system(command);
}
