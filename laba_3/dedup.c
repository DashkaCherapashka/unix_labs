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

static void add_entry(const char *path, const char *hash) {
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
    (void)sb;
    (void)ftwbuf;
    if (typeflag != FTW_F) {
        return 0;
    }

    char hash[HASH_HEX_LEN + 1];
    if (compute_sha1(fpath, hash) != 0) {
        fprintf(stderr, "Не удалось вычислить хэш для %s\n", fpath);
        return 0;
    }

    add_entry(fpath, hash);
    return 0;
}

static int compare_entries(const void *a, const void *b) {
    const FileEntry *ea = *(const FileEntry **)a;
    const FileEntry *eb = *(const FileEntry **)b;
    return strcmp(ea->hash, eb->hash);
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
        while (j < count && strcmp(arr[i]->hash, arr[j]->hash) == 0) j++;

        if (j - i > 1) {
            const char *keep = arr[i]->path;
            printf("Группа дубликатов (хэш %s):\n", arr[i]->hash);
            printf("  Оставляем: %s\n", keep);

            for (size_t k = i + 1; k < j; k++) {
                const char *dup = arr[k]->path;

                struct stat st_keep, st_dup;
                if (stat(keep, &st_keep) != 0 || stat(dup, &st_dup) != 0) {
                    perror("stat");
                    continue;
                }
                if (st_keep.st_size != st_dup.st_size) {
                    fprintf(stderr,
                            "Предупреждение: размеры не совпадают для %s и %s\n",
                            keep, dup);
                    continue;
                }

                char temp_path[4096];
                snprintf(temp_path, sizeof(temp_path), "%s.tmp_link_%d",
                         dup, (int)getpid());

                if (link(keep, temp_path) != 0) {
                    perror("link (temp)");
                    continue;
                }
                if (unlink(dup) != 0) {
                    perror("unlink (dup)");
                    unlink(temp_path);
                    continue;
                }
                if (rename(temp_path, dup) != 0) {
                    perror("rename");
                    continue;
                }
                printf("  Заменён: %s -> ссылка на %s\n", dup, keep);
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