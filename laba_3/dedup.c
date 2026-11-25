#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ftw.h>
#include <sys/stat.h>
#include <openssl/evp.h>

#define HASH_HEX_LEN 40

typedef struct FileEntry {
    char *path;
    char hash[HASH_HEX_LEN + 1];
    dev_t dev;
    ino_t ino;
    off_t size;
    struct FileEntry *next;
} FileEntry;

static FileEntry *head = NULL;
static FileEntry *tail = NULL;

static int compute_sha1(const char *path, char out_hex[HASH_HEX_LEN + 1]) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        perror("fopen");
        return -1;
    }

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) {
        fclose(fp);
        return -1;
    }

    const EVP_MD *md = EVP_sha1();
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len = 0;

    if (EVP_DigestInit_ex(ctx, md, NULL) != 1) {
        EVP_MD_CTX_free(ctx);
        fclose(fp);
        return -1;
    }

    unsigned char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        if (EVP_DigestUpdate(ctx, buf, n) != 1) {
            EVP_MD_CTX_free(ctx);
            fclose(fp);
            return -1;
        }
    }

    if (ferror(fp)) {
        EVP_MD_CTX_free(ctx);
        fclose(fp);
        return -1;
    }

    if (EVP_DigestFinal_ex(ctx, digest, &digest_len) != 1) {
        EVP_MD_CTX_free(ctx);
        fclose(fp);
        return -1;
    }

    EVP_MD_CTX_free(ctx);
    fclose(fp);

    for (unsigned int i = 0; i < digest_len; i++) {
        sprintf(out_hex + i * 2, "%02x", digest[i]);
    }
    out_hex[HASH_HEX_LEN] = '\0';
    return 0;
}

static void add_entry(const char *path, const char *hash,
                      const struct stat *sb) {
    FileEntry *e = malloc(sizeof(FileEntry));
    if (!e) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    e->path = strdup(path);
    if (!e->path) {
        perror("strdup");
        free(e);
        exit(EXIT_FAILURE);
    }
    strncpy(e->hash, hash, HASH_HEX_LEN);
    e->hash[HASH_HEX_LEN] = '\0';
    e->dev = sb->st_dev;
    e->ino = sb->st_ino;
    e->size = sb->st_size;
    e->next = NULL;
    if (!head) {
        head = tail = e;
    } else {
        tail->next = e;
        tail = e;
    }
}

static int process_file(const char *fpath, const struct stat *sb,
                        int typeflag, struct FTW *ftwbuf) {
    (void)ftwbuf;
    if (typeflag != FTW_F) {
        return 0;
    }

    char hash[HASH_HEX_LEN + 1];
    if (compute_sha1(fpath, hash) != 0) {
        fprintf(stderr, "Не удалось вычислить хэш для %s\n", fpath);
        return 0;
    }

    add_entry(fpath, hash, sb);
    return 0;
}

static int compare_entries(const void *a, const void *b) {
    const FileEntry *ea = *(const FileEntry **)a;
    const FileEntry *eb = *(const FileEntry **)b;
    int r = strcmp(ea->hash, eb->hash);
    if (r != 0) {
        return r;
    }
    if (ea->size != eb->size) {
        return (ea->size < eb->size) ? -1 : 1;
    }
    return 0;
}

static int replace_with_hardlink(const FileEntry *keep, const FileEntry *dup) {
    if (keep->dev != dup->dev) {
        fprintf(stderr,
                "Пропуск %s: другой раздел (dev %llu != %llu)\n",
                dup->path,
                (unsigned long long)keep->dev,
                (unsigned long long)dup->dev);
        return -1;
    }
    if (keep->ino == dup->ino) {
        fprintf(stderr,
                "Пропуск %s: уже жёсткая ссылка на %s\n",
                dup->path, keep->path);
        return -1;
    }

    size_t len = strlen(dup->path) + 32;
    char *temp_path = malloc(len);
    if (!temp_path) {
        perror("malloc");
        return -1;
    }
    int need = snprintf(temp_path, len, "%s.tmp_link_%d",
                        dup->path, (int)getpid());
    if (need < 0 || (size_t)need >= len) {
        fprintf(stderr,
                "Слишком длинный путь для временной ссылки: %s\n",
                dup->path);
        free(temp_path);
        return -1;
    }

    if (link(keep->path, temp_path) != 0) {
        perror("link (temp)");
        free(temp_path);
        return -1;
    }
    if (unlink(dup->path) != 0) {
        perror("unlink (dup)");
        unlink(temp_path);
        free(temp_path);
        return -1;
    }
    if (rename(temp_path, dup->path) != 0) {
        perror("rename");
        unlink(temp_path);
        free(temp_path);
        return -1;
    }

    free(temp_path);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Использование: %s <каталог>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *root = argv[1];
    struct stat st;
    if (stat(root, &st) != 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Ошибка: %s не является каталогом\n", root);
        return EXIT_FAILURE;
    }

    if (nftw(root, process_file, 20, FTW_PHYS) == -1) {
        perror("nftw");
        return EXIT_FAILURE;
    }

    size_t count = 0;
    for (FileEntry *e = head; e; e = e->next) count++;
    if (count == 0) {
        printf("Файлов не найдено.\n");
        return EXIT_SUCCESS;
    }

    FileEntry **arr = malloc(count * sizeof(FileEntry *));
    if (!arr) {
        perror("malloc");
        return EXIT_FAILURE;
    }
    size_t idx = 0;
    for (FileEntry *e = head; e; e = e->next) arr[idx++] = e;

    qsort(arr, count, sizeof(FileEntry *), compare_entries);

    size_t i = 0;
    while (i < count) {
        size_t j = i + 1;
        while (j < count &&
               strcmp(arr[i]->hash, arr[j]->hash) == 0 &&
               arr[i]->size == arr[j]->size) {
            j++;
        }

        if (j - i > 1) {
            if (arr[i]->size == 0) {
                printf("Пропуск группы пустых файлов (хэш %s)\n", arr[i]->hash);
                i = j;
                continue;
            }

            const FileEntry *keep = arr[i];
            printf("Группа дубликатов (хэш %s):\n", keep->hash);
            printf("  Оставляем: %s\n", keep->path);

            for (size_t k = i + 1; k < j; k++) {
                if (replace_with_hardlink(keep, arr[k]) == 0) {
                    printf("  Заменён: %s -> ссылка на %s\n",
                           arr[k]->path, keep->path);
                }
            }
        }
        i = j;
    }

    for (FileEntry *e = head; e; ) {
        FileEntry *next = e->next;
        free(e->path);
        free(e);
        e = next;
    }
    free(arr);
    return EXIT_SUCCESS;
}
