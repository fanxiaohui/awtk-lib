#include "awtk.h"

/**
 * 初始化
 */
ret_t application_init(void) {

  widget_t* system_bar = window_open("system_bar");
//  widget_t* system_bar_b = window_open("system_bar_b");
  widget_t* win = window_open("start");

  return RET_OK;
}

/**
 * 退出
 */
ret_t application_exit(void) {
  log_debug("application_exit\n");
  return RET_OK;
}
