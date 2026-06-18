#ifndef __PRINTK_H__
#define __PRINTK_H__

#include "types.h"

void printk(const char *fmt, ...);
void printk_info(const char *fmt, ...);
void printk_warn(const char *fmt, ...);
void printk_error(const char *fmt, ...);
void printk_panic(const char *fmt, ...);
void early_printk(const char *str);
void printk_init(void);

#endif
