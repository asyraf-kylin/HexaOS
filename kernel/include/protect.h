#ifndef HEXAOS_PROTECT_H
#define HEXAOS_PROTECT_H

#include <stdint.h>
#include <stdbool.h>

#define PERM_R     0x01
#define PERM_W     0x02
#define PERM_X     0x04
#define PERM_USER  0x08   /* region boleh diakses user mode */

#define E_OK       0
#define E_PERM    -1
#define E_FAULT   -2
#define E_NOSYS   -3

typedef enum { PRIV_KERNEL = 0, PRIV_USER = 1 } priv_t;

enum { SYS_GETPID = 1, SYS_WRITE = 2, SYS_EXIT = 3, SYS_REBOOT = 4,
       SYS_SU = 5,      /* a1 = ptr password  : minta mode root        */
       SYS_SETPW = 6,   /* a1 = ptr lama, a2 = ptr baru : ganti password (khusus root) */
       SYS_DROP = 7 };  /* kembali ke user mode                        */

void   protect_init(void);
bool   protect_add_region(uint32_t start, uint32_t size, uint32_t flags, const char *name);
bool   protect_check(priv_t who, uint32_t addr, uint32_t len, uint32_t access);

void   protect_set_priv(int32_t tid, priv_t p);
priv_t protect_get_priv(int32_t tid);

/* Gerbang syscall: SATU-SATUNYA jalur user mode meminta layanan kernel */
int32_t protect_syscall(uint32_t num, uint32_t a1, uint32_t a2);

/* Operasi khusus kernel (mis. reboot/shutdown) */
bool    protect_require_kernel(const char *op);

/* Mode root: naik ke PRIV_KERNEL dengan password (simulasi su/sudo) */
int32_t protect_request_root(const char *password);   /* E_OK / E_PERM */
void    protect_drop_root(void);                      /* kembali PRIV_USER */
/* Ganti password: hanya boleh saat thread sedang mode root */
int32_t protect_change_password(const char *old_pw, const char *new_pw);

#endif