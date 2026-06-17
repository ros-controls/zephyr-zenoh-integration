#include <zenoh-pico.h>
#include <zephyr/kernel.h>
#include <zephyr/posix/pthread.h>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>

int main()
{
  printk("\n=============================\n");
  printk("  Hello, World! (Zephyr C++17)\n");
  printk("  Board : %s\n", CONFIG_BOARD);
  printk("=============================\n\n");

  return 0;
}
