#ifndef HEXAOS_SYNC_H
#define HEXAOS_SYNC_H

#include "kernel.h"
#include "mutex.h"
#include "semaphore.h"

/* Fungsi pengujian / demo */
void test_deadlock(void);
void test_protect(void);

/* Inisialisasi modul sync */
void sync_init(void);

#endif
