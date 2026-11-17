#ifndef SVC_H
#define SVC_H
/* Anggota 2 */
void summary_start(void);  /* thread 'summary' permanen, SILENT (tulis ke ring buffer, jangan kprintf) */
int  purge_start(void);    /* thread 'purge' sekali jalan -> todo_purge_done(); 0 = ok, <0 = gagal */
void synclog_dump(void);   /* cetak ring buffer log (dipakai sysinfo) */
/* Uji untuk 'diag <nama>' */
void test_threads(void);   /* A2 */
void test_preempt(void);   /* A2 */
void test_race(void);      /* A3 */
void test_deadlock(void);  /* A3 (sudah ada) */
void test_protect(void);   /* A3 (sudah ada, perlu diperbaiki) */
void test_mem(void);       /* A4 */
#endif