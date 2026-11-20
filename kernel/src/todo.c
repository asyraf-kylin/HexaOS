#include "todo.h"
#include "vfs.h"
#include "sync.h"
#include "memory.h"
#include "kernel.h"

#define TODO_MAX_STATUS_ID 256

static mutex_t mutex_vfs;
static mutex_t mutex_counter;
static int global_id_counter = 0;
static int cached_active = 0;
static int cached_done = 0;
static uint8_t todo_done_flags[TODO_MAX_STATUS_ID];

int vfs_name_at(int index, char *out, size_t max);

static int todo_raw_len(const char *s) {
    int n = 0;
    while (s && s[n] != '\0') {
        n++;
    }
    return n;
}

static int parse_todo_id_from_name(const char *fname) {
    if (!fname) return -1;
    const char *prefix = "todo_";
    int i = 0;
    while (prefix[i]) {
        if (fname[i] != prefix[i]) return -1;
        i++;
    }
    int id = 0;
    int has_digit = 0;
    while (fname[i] >= '0' && fname[i] <= '9') {
        id = id * 10 + (fname[i] - '0');
        has_digit = 1;
        i++;
    }
    return has_digit ? id : -1;
}

static void reset_id_if_empty(void) {
    char fname[32];
    if (vfs_name_at(0, fname, sizeof(fname)) != 0) {
        mutex_lock(&mutex_counter);
        global_id_counter = 0;
        mutex_unlock(&mutex_counter);
    }
}

void todo_init(void) {
    mutex_init(&mutex_vfs, "vfs_lock");
    mutex_init(&mutex_counter, "counter_lock");
    global_id_counter = 0;
    for (int i = 0; i < TODO_MAX_STATUS_ID; i++) {
        todo_done_flags[i] = 0;
    }
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
            int id = parse_todo_id_from_name(fname);
            if (id >= 0 && id < TODO_MAX_STATUS_ID && todo_done_flags[id]) {
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

    int len = todo_raw_len(text);
    if (len > TODO_TEXT_MAX) len = TODO_TEXT_MAX;

    char clean_buf[TODO_TEXT_MAX + 1];
    int k = 0;
    while (k < len) {
        clean_buf[k] = text[k];
        k++;
    }
    clean_buf[k] = '\0';

    mutex_lock(&mutex_vfs);
    mutex_lock(&mutex_counter);
    int new_id = global_id_counter;
    global_id_counter++;
    mutex_unlock(&mutex_counter);

    char filename[32];
    make_todo_filename(new_id, filename, sizeof(filename));

    int res_create = vfs_create(filename);
    if (res_create < 0) {
        mutex_unlock(&mutex_vfs);
        return TODO_ERR_FULL;
    }

    int res_write = vfs_write(filename, clean_buf);
    if (res_write == -3) {
        vfs_delete(filename);
        mutex_unlock(&mutex_vfs);
        return TODO_ERR_OOM;
    }

    if (new_id >= 0 && new_id < TODO_MAX_STATUS_ID) {
        todo_done_flags[new_id] = 0;
    }

    mutex_unlock(&mutex_vfs);
    todo_recount(&cached_active, &cached_done);
    return new_id;
}

int todo_done(int id) {
    mutex_lock(&mutex_vfs);
    char fname[32];
    make_todo_filename(id, fname, sizeof(fname));

    if (!vfs_read(fname)) {
        mutex_unlock(&mutex_vfs);
        return TODO_ERR_NOID;
    }

    if (id >= 0 && id < TODO_MAX_STATUS_ID) {
        todo_done_flags[id] = 1;
    }

    mutex_unlock(&mutex_vfs);
    todo_recount(&cached_active, &cached_done);
    return TODO_OK;
}

int todo_undone(int id) {
    mutex_lock(&mutex_vfs);
    char fname[32];
    make_todo_filename(id, fname, sizeof(fname));

    if (!vfs_read(fname)) {
        mutex_unlock(&mutex_vfs);
        return TODO_ERR_NOID;
    }

    if (id >= 0 && id < TODO_MAX_STATUS_ID) {
        todo_done_flags[id] = 0;
    }

    mutex_unlock(&mutex_vfs);
    todo_recount(&cached_active, &cached_done);
    return TODO_OK;
}

int todo_done_all(void) {
    mutex_lock(&mutex_vfs);
    char fname[32];
    int changed = 0;
    int idx = 0;

    while (vfs_name_at(idx, fname, sizeof(fname)) == 0) {
        int id = parse_todo_id_from_name(fname);
        if (id >= 0 && id < TODO_MAX_STATUS_ID && todo_done_flags[id] == 0) {
            todo_done_flags[id] = 1;
            changed++;
        }
        idx++;
    }

    mutex_unlock(&mutex_vfs);
    todo_recount(&cached_active, &cached_done);
    return changed;
}

int todo_undone_all(void) {
    mutex_lock(&mutex_vfs);
    char fname[32];
    int changed = 0;
    int idx = 0;

    while (vfs_name_at(idx, fname, sizeof(fname)) == 0) {
        int id = parse_todo_id_from_name(fname);
        if (id >= 0 && id < TODO_MAX_STATUS_ID && todo_done_flags[id] != 0) {
            todo_done_flags[id] = 0;
            changed++;
        }
        idx++;
    }

    mutex_unlock(&mutex_vfs);
    todo_recount(&cached_active, &cached_done);
    return changed;
}

int todo_edit(int id, const char *text) {
    if (!text || text[0] == '\0') return TODO_ERR_EMPTY;

    mutex_lock(&mutex_vfs);
    char fname[32];
    make_todo_filename(id, fname, sizeof(fname));

    if (!vfs_read(fname)) {
        mutex_unlock(&mutex_vfs);
        return TODO_ERR_NOID;
    }

    int len = todo_raw_len(text);
    if (len > TODO_TEXT_MAX) len = TODO_TEXT_MAX;

    char clean_buf[TODO_TEXT_MAX + 1];
    int k = 0;
    while (k < len) {
        clean_buf[k] = text[k];
        k++;
    }
    clean_buf[k] = '\0';

    int rc = vfs_write(fname, clean_buf);
    mutex_unlock(&mutex_vfs);
    return (rc < 0) ? TODO_ERR_OOM : TODO_OK;
}

int todo_del(int id) {
    mutex_lock(&mutex_vfs);
    char fname[32];
    make_todo_filename(id, fname, sizeof(fname));

    int res = vfs_delete(fname);
    if (res == 0) {
        if (id >= 0 && id < TODO_MAX_STATUS_ID) {
            todo_done_flags[id] = 0;
        }
        reset_id_if_empty();
    }
    mutex_unlock(&mutex_vfs);

    if (res < 0) return TODO_ERR_NOID;

    todo_recount(&cached_active, &cached_done);
    return TODO_OK;
}

int todo_del_all(void) {
    mutex_lock(&mutex_vfs);
    char fname[32];
    int removed = 0;
    int idx = 0;

    while (vfs_name_at(idx, fname, sizeof(fname)) == 0) {
        int id = parse_todo_id_from_name(fname);
        if (vfs_delete(fname) == 0) {
            if (id >= 0 && id < TODO_MAX_STATUS_ID) {
                todo_done_flags[id] = 0;
            }
            removed++;
            continue;
        }
        idx++;
    }
    if (removed > 0) reset_id_if_empty();
    mutex_unlock(&mutex_vfs);

    todo_recount(&cached_active, &cached_done);
    return removed;
}

int todo_purge_done(void) {
    mutex_lock(&mutex_vfs);
    char fname[32];
    int purged = 0;
    int idx = 0;

    while (vfs_name_at(idx, fname, sizeof(fname)) == 0) {
        int id = parse_todo_id_from_name(fname);
        if (id >= 0 && id < TODO_MAX_STATUS_ID && todo_done_flags[id]) {
            if (vfs_delete(fname) == 0) {
                todo_done_flags[id] = 0;
                purged++;
                continue;
            }
        }
        idx++;
    }
    if (purged > 0) reset_id_if_empty();
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

    int parsed_id = parse_todo_id_from_name(found_fname);
    if (parsed_id < 0) parsed_id = 0;

    int is_done = (parsed_id >= 0 && parsed_id < TODO_MAX_STATUS_ID && todo_done_flags[parsed_id]) ? 1 : 0;

    /* Salin teks murni dan batasi sesuai panjang buffer */
    int dst_idx = 0;
    while (content[dst_idx] && dst_idx < max - 1) {
        text_out[dst_idx] = content[dst_idx];
        dst_idx++;
    }
    text_out[dst_idx] = '\0';

    mutex_unlock(&mutex_vfs);

    if (id_out) *id_out = parsed_id;
    if (done_out) *done_out = is_done;

    return TODO_OK;
}

void todo_get_counts(int *active, int *done) {
    mutex_lock(&mutex_counter);
    if (active) *active = cached_active;
    if (done) *done = cached_done;
    mutex_unlock(&mutex_counter);
}