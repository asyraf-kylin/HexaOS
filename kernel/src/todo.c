#include "todo.h"
#include "vfs.h"
#include "sync.h"
#include "memory.h"
#include "kernel.h"

static mutex_t mutex_vfs;
static mutex_t mutex_counter;
static int global_id_counter = 0;
static int cached_active = 0;
static int cached_done = 0;

// Deklarasi helper vfs_name_at dari vfs.c
int vfs_name_at(int index, char *out, size_t max);

void todo_init(void) {
    mutex_init(&mutex_vfs, "vfs_lock");       // Berikan nama mutex
    mutex_init(&mutex_counter, "counter_lock"); // Berikan nama mutex
    global_id_counter = 0;
    todo_recount(&cached_active, &cached_done);
}

static void make_todo_filename(int id, char *buf, int max) {
    int temp = id;
    int len = 0;
    char rev[16];
    const char *p = "todo_";
    int i = 0;
    while (p[i] && i < max - 1) {
        buf[i] = p[i];
        i++;
    }
    if (temp == 0) {
        rev[len++] = '0';
    } else {
        while (temp > 0) {
            rev[len++] = '0' + (temp % 10);
            temp /= 10;
        }
    }
    while (len > 0 && i < max - 5) {
        buf[i++] = rev[--len];
    }
    const char *ext = ".txt";
    int j = 0;
    while (ext[j] && i < max - 1) {
        buf[i++] = ext[j++];
    }
    buf[i] = '\0';
}

// Sesuai dengan todo.h: menerima 2 parameter pointer
void todo_recount(int *active_out, int *done_out) {
    mutex_lock(&mutex_vfs);
    mutex_lock(&mutex_counter);
    
    int active = 0;
    int done = 0;
    char fname[32];
    
    int idx = 0;
    while (vfs_name_at(idx, fname, sizeof(fname)) == 0) {
        char *content = vfs_read(fname);
        if (content) {
            if (content[1] == 'x' || content[1] == 'X') {
                done++;
            } else {
                active++;
            }
        }
        idx++;
    }
    
    cached_active = active;
    cached_done = done;
    
    if (active_out) *active_out = active;
    if (done_out) *done_out = done;
    
    mutex_unlock(&mutex_counter);
    mutex_unlock(&mutex_vfs);
}

int todo_add(const char *text) {
    if (!text || text[0] == '\0') return TODO_ERR_EMPTY;

    mutex_lock(&mutex_vfs);
    mutex_lock(&mutex_counter);
    global_id_counter++;
    int new_id = global_id_counter;
    mutex_unlock(&mutex_counter);
    
    char filename[32];
    make_todo_filename(new_id, filename, sizeof(filename));
    
    int res_create = vfs_create(filename);
    if (res_create < 0) {
        mutex_unlock(&mutex_vfs);
        return TODO_ERR_FULL;
    }
    
    char write_buf[TODO_TEXT_MAX + 16];
    write_buf[0] = '[';
    write_buf[1] = ' ';
    write_buf[2] = ']';
    write_buf[3] = ' ';
    
    int idx = 4;
    int t = 0;
    while (text[t] && idx < (int)sizeof(write_buf) - 2) {
        write_buf[idx++] = text[t++];
    }
    write_buf[idx] = '\0';
    
    int res_write = vfs_write(filename, write_buf);
    if (res_write == -3) {
        vfs_delete(filename);
        mutex_unlock(&mutex_vfs);
        return TODO_ERR_OOM;
    }
    
    mutex_unlock(&mutex_vfs);
    todo_recount(&cached_active, &cached_done);
    return new_id;
}

int todo_done(int id) {
    mutex_lock(&mutex_vfs);
    char fname[32];
    make_todo_filename(id, fname, sizeof(fname));
    
    char *content = vfs_read(fname);
    if (!content) {
        mutex_unlock(&mutex_vfs);
        return TODO_ERR_NOID;
    }
    
    char buf[256];
    int i = 0;
    while (content[i] && i < (int)sizeof(buf) - 1) {
        buf[i] = content[i];
        i++;
    }
    buf[i] = '\0';
    
    if (buf[1] == ' ') {
        buf[1] = 'x';
        vfs_write(fname, buf);
    }
    
    mutex_unlock(&mutex_vfs);
    todo_recount(&cached_active, &cached_done);
    return TODO_OK;
}

int todo_edit(int id, const char *text) {
    if (!text || text[0] == '\0') return TODO_ERR_EMPTY;

    mutex_lock(&mutex_vfs);
    char fname[32];
    make_todo_filename(id, fname, sizeof(fname));
    
    char *content = vfs_read(fname);
    if (!content) {
        mutex_unlock(&mutex_vfs);
        return TODO_ERR_NOID;
    }
    
    char status = content[1];
    
    char buf[256];
    buf[0] = '[';
    buf[1] = status;
    buf[2] = ']';
    buf[3] = ' ';
    
    int idx = 4;
    int t = 0;
    while (text[t] && idx < (int)sizeof(buf) - 2) {
        buf[idx++] = text[t++];
    }
    buf[idx] = '\0';
    
    vfs_write(fname, buf);
    mutex_unlock(&mutex_vfs);
    return TODO_OK;
}

int todo_del(int id) {
    mutex_lock(&mutex_vfs);
    char fname[32];
    make_todo_filename(id, fname, sizeof(fname));
    
    int res = vfs_delete(fname);
    mutex_unlock(&mutex_vfs);
    
    if (res < 0) return TODO_ERR_NOID;
    
    todo_recount(&cached_active, &cached_done);
    return TODO_OK;
}

int todo_purge_done(void) {
    mutex_lock(&mutex_vfs);
    char fname[32];
    int purged = 0;
    int idx = 0;
    
    while (vfs_name_at(idx, fname, sizeof(fname)) == 0) {
        char *content = vfs_read(fname);
        if (content) {
            if (content[1] == 'x' || content[1] == 'X') {
                vfs_delete(fname);
                purged++;
                continue; 
            }
        }
        idx++;
    }
    mutex_unlock(&mutex_vfs);
    
    todo_recount(&cached_active, &cached_done);
    return purged;
}

int todo_count(void) {
    int active, done;
    todo_get_counts(&active, &done);
    return active + done;
}

int todo_at(int index, int *id_out, int *done_out, char *text_out, int max) {
    mutex_lock(&mutex_vfs);
    
    char fname[32];
    int found_idx = 0;
    char found_fname[32];
    int found = 0;
    
    int i = 0;
    while (vfs_name_at(i, fname, sizeof(fname)) == 0) {
        if (found_idx == index) {
            int k = 0;
            while (fname[k] && k < (int)sizeof(found_fname) - 1) {
                found_fname[k] = fname[k];
                k++;
            }
            found_fname[k] = '\0';
            found = 1;
            break;
        }
        found_idx++;
        i++;
    }
    
    if (!found) {
        mutex_unlock(&mutex_vfs);
        return TODO_ERR_NOID;
    }
    
    char *content = vfs_read(found_fname);
    if (!content) {
        mutex_unlock(&mutex_vfs);
        return TODO_ERR_NOID;
    }
    
    char buf[256];
    int sz = 0;
    while (content[sz] && sz < (int)sizeof(buf) - 1) {
        buf[sz] = content[sz];
        sz++;
    }
    buf[sz] = '\0';
    
    mutex_unlock(&mutex_vfs);
    
    int parsed_id = 0;
    const char *p = found_fname + 5;
    while (*p >= '0' && *p <= '9') {
        parsed_id = parsed_id * 10 + (*p - '0');
        p++;
    }
    
    if (id_out) *id_out = parsed_id;
    if (done_out) *done_out = (buf[1] == 'x' || buf[1] == 'X') ? 1 : 0;
    
    int src_idx = 4;
    int dst_idx = 0;
    while (buf[src_idx] && dst_idx < max - 1) {
        text_out[dst_idx++] = buf[src_idx++];
    }
    text_out[dst_idx] = '\0';
    
    return TODO_OK;
}

void todo_get_counts(int *active, int *done) {
    mutex_lock(&mutex_counter);
    if (active) *active = cached_active;
    if (done) *done = cached_done;
    mutex_unlock(&mutex_counter);
}