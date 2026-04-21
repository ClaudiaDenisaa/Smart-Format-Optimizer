/**
 * IR3 2026
 * image processor
 *
 * Programul se ocupa de procesarea imaginilor primite de server.
 * Sunt acceptate fisiere JPEG si PNG.
 *
 * Pentru JPEG:
 *  - imaginea este decomprimata
 *  - apoi este recomprimata cu o calitate mai mica
 *
 * Pentru PNG:
 *  - imaginea este citita din memorie cu libpng
 *  - apoi este rescrisa cu nivel maxim de compresie
 *  - optional, daca exista optipng in sistem, se incearca o optimizare suplimentara
 *
 * La final se actualizeaza fisierul stats.txt cu rezultatul procesarii.
 */

#include "image_processor.h"
#include "proto.h"

#include <errno.h>    /* utilizat pentru: errno */
#include <limits.h>   /* utilizat pentru: PATH_MAX */
#include <png.h>      /* utilizat pentru procesarea PNG */
#include <setjmp.h>   /* utilizat de libpng pentru tratarea erorilor */
#include <stdio.h>    /* utilizat pentru: FILE, fopen, fclose, fprintf */
#include <stdlib.h>   /* utilizat pentru: malloc, free, atoi */
#include <string.h>   /* utilizat pentru: strlen, strcmp, strncpy, memcpy */
#include <strings.h>  /* utilizat pentru: strcasecmp */
#include <sys/wait.h> /* utilizat pentru: waitpid */
#include <unistd.h>   /* utilizat pentru: access, fork, execl */
#include <zlib.h>     /* utilizat pentru compresia PNG */
#include <turbojpeg.h>/* utilizat pentru compresia JPEG */

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* calitatea folosita la recomprimarea JPEG */
#define JPEG_QUALITY 75

/*
 * Structura ajutatoare pentru citirea unui PNG direct din memorie,
 * nu din fisier.
 *
 * data   = adresa bufferului cu imaginea
 * size   = dimensiunea totala a bufferului
 * offset = pozitia curenta de citire
 */
typedef struct {
    const unsigned char *data;
    size_t size;
    size_t offset;
} PngMemoryReader;

/*
 * Intoarce doar numele fisierului, fara calea completa.
 *
 * Exemplu:
 *   /tmp/test/imagine.png -> imagine.png
 *
 * Daca numele lipseste, intoarce un nume implicit.
 */
static const char *safe_filename(const char *name) {
    const char *p;

    if (name == NULL || *name == '\0') {
        return "image";
    }

    p = strrchr(name, '/');
    if (p != NULL && *(p + 1) != '\0') {
        return p + 1;
    }

    return name;
}

/*
 * Verifica daca fisierul are extensia ceruta.
 * Comparatia nu tine cont de litere mari/mici.
 */
static int has_extension(const char *name, const char *ext) {
    size_t name_len;
    size_t ext_len;

    if (name == NULL || ext == NULL) {
        return 0;
    }

    name_len = strlen(name);
    ext_len = strlen(ext);

    if (name_len < ext_len) {
        return 0;
    }

    return strcasecmp(name + name_len - ext_len, ext) == 0;
}

/*
 * Detecteaza foarte simplu daca bufferul pare sa fie JPEG,
 * pe baza semnaturii de inceput si sfarsit.
 */
static int is_jpeg_data(const unsigned char *data, size_t size) {
    if (data == NULL || size < 3) {
        return 0;
    }

    return data[0] == 0xFF && data[1] == 0xD8 && data[size - 2] == 0xFF && data[size - 1] == 0xD9;
}

/*
 * Verifica semnatura standard PNG din primii 8 bytes.
 */
static int is_png_data(const unsigned char *data, size_t size) {
    static const unsigned char png_sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};

    if (data == NULL || size < sizeof(png_sig)) {
        return 0;
    }

    return memcmp(data, png_sig, sizeof(png_sig)) == 0;
}

/*
 * Construieste numele fisierului de iesire.
 *
 * Pentru PNG:
 *   optimized_nume.png
 *
 * Pentru JPEG:
 *   optimized_nume_q75.jpg
 */
static void build_output_name(const char *input_name,
                              int is_png,
                              char *out,
                              size_t out_size) {
    const char *base;
    char tmp[256];
    char *dot;

    base = safe_filename(input_name);

    memset(tmp, 0, sizeof(tmp));
    strncpy(tmp, base, sizeof(tmp) - 1);

    /* scoatem extensia veche, daca exista */
    dot = strrchr(tmp, '.');
    if (dot != NULL) {
        *dot = '\0';
    }

    if (is_png) {
        snprintf(out, out_size, "optimized_%s.png", tmp);
    } else {
        snprintf(out, out_size, "optimized_%s_q75.jpg", tmp);
    }
}

/*
 * Citeste din stats.txt valoarea curenta pentru images_processed.
 * Daca fisierul nu exista sau campul nu este gasit, intoarce 0.
 */
static int read_images_processed_count(void) {
    FILE *f;
    char line[256];
    int count = 0;

    f = fopen("stats.txt", "r");
    if (f == NULL) {
        return 0;
    }

    while (fgets(line, sizeof(line), f) != NULL) {
        if (strncmp(line, "images_processed=", 17) == 0) {
            count = atoi(line + 17);
            break;
        }
    }

    fclose(f);
    return count;
}

/*
 * Rescrie complet fisierul stats.txt dupa o procesare.
 *
 * Daca statusul este OPTIMIZED, contorul de imagini procesate creste.
 * Se salveaza si ultimele informatii despre fisierul lucrat.
 */
static void update_stats_after_processing(int client_id,
                                          const char *filename,
                                          size_t initial_size,
                                          size_t final_size,
                                          const char *status) {
    FILE *f;
    int count;

    count = read_images_processed_count();

    if (status != NULL && strcmp(status, "OPTIMIZED") == 0) {
        count++;
    }

    f = fopen("stats.txt", "w");
    if (f == NULL) {
        fprintf(stderr, "[PROCESSOR] nu pot actualiza stats.txt\n");
        return;
    }

    fprintf(f, "images_processed=%d\n", count);
    fprintf(f, "last_filename=%s\n", filename != NULL ? filename : "N/A");
    fprintf(f, "last_filesize_initial=%zu\n", initial_size);
    fprintf(f, "last_filesize_final=%zu\n", final_size);
    fprintf(f, "last_client_id=%d\n", client_id);
    fprintf(f, "last_status=%s\n", status != NULL ? status : "N/A");

    fclose(f);
}

/*
 * Comprima o imagine JPEG si o salveaza in fisier.
 *
 * Pasii sunt:
 *  1. se initializeaza decomprimarea
 *  2. se citesc dimensiunile imaginii
 *  3. se aloca buffer RGB
 *  4. se decomprima imaginea in RGB
 *  5. se initializeaza compresia
 *  6. se recomprima la calitatea definita
 *  7. se scrie rezultatul in fisier
 *
 * Intoarce:
 *  - 0 la succes
 *  - -1 la eroare
 */
static int compress_jpeg_to_file(const unsigned char *input_data,
                                 size_t input_size,
                                 const char *output_path,
                                 size_t *final_size) {
    tjhandle dec_handle = NULL;
    tjhandle enc_handle = NULL;
    unsigned char *rgb_buf = NULL;
    unsigned char *jpeg_buf = NULL;
    unsigned long jpeg_size = 0;
    int width = 0;
    int height = 0;
    int subsamp = 0;
    int colorspace = 0;
    FILE *f = NULL;
    int ret = -1;

    if (input_data == NULL || input_size == 0 || output_path == NULL) {
        fprintf(stderr, "[PROCESSOR] date invalide pentru compresie JPEG\n");
        return -1;
    }

    dec_handle = tjInitDecompress();
    if (dec_handle == NULL) {
        fprintf(stderr, "[PROCESSOR] tjInitDecompress a esuat: %s\n", tjGetErrorStr());
        goto cleanup;
    }

    if (tjDecompressHeader3(dec_handle,
                            input_data,
                            (unsigned long)input_size,
                            &width,
                            &height,
                            &subsamp,
                            &colorspace) != 0) {
        fprintf(stderr, "[PROCESSOR] tjDecompressHeader3 a esuat: %s\n", tjGetErrorStr());
        goto cleanup;
    }

    /* alocam memorie pentru imaginea decomprimata in format RGB */
    rgb_buf = (unsigned char *)malloc((size_t)width * (size_t)height * 3u);
    if (rgb_buf == NULL) {
        fprintf(stderr, "[PROCESSOR] malloc a esuat pentru bufferul RGB\n");
        goto cleanup;
    }

    if (tjDecompress2(dec_handle,
                      input_data,
                      (unsigned long)input_size,
                      rgb_buf,
                      width,
                      0,
                      height,
                      TJPF_RGB,
                      0) != 0) {
        fprintf(stderr, "[PROCESSOR] tjDecompress2 a esuat: %s\n", tjGetErrorStr());
        goto cleanup;
    }

    enc_handle = tjInitCompress();
    if (enc_handle == NULL) {
        fprintf(stderr, "[PROCESSOR] tjInitCompress a esuat: %s\n", tjGetErrorStr());
        goto cleanup;
    }

    if (tjCompress2(enc_handle,
                    rgb_buf,
                    width,
                    0,
                    height,
                    TJPF_RGB,
                    &jpeg_buf,
                    &jpeg_size,
                    TJSAMP_420,
                    JPEG_QUALITY,
                    TJFLAG_FASTDCT) != 0) {
        fprintf(stderr, "[PROCESSOR] tjCompress2 a esuat: %s\n", tjGetErrorStr());
        goto cleanup;
    }

    f = fopen(output_path, "wb");
    if (f == NULL) {
        fprintf(stderr, "[PROCESSOR] nu pot crea fisierul %s: %s\n", output_path, strerror(errno));
        goto cleanup;
    }

    if (fwrite(jpeg_buf, 1, jpeg_size, f) != jpeg_size) {
        fprintf(stderr, "[PROCESSOR] scriere incompleta in %s\n", output_path);
        goto cleanup;
    }

    fprintf(stderr,
            "[PROCESSOR] fisier optimizat salvat: %s (dimensiune noua: %lu bytes)\n",
            output_path,
            jpeg_size);

    if (final_size != NULL) {
        *final_size = (size_t)jpeg_size;
    }

    ret = 0;

cleanup:
    /* eliberam toate resursele folosite */
    if (f != NULL) {
        fclose(f);
    }
    if (jpeg_buf != NULL) {
        tjFree(jpeg_buf);
    }
    if (rgb_buf != NULL) {
        free(rgb_buf);
    }
    if (dec_handle != NULL) {
        tjDestroy(dec_handle);
    }
    if (enc_handle != NULL) {
        tjDestroy(enc_handle);
    }

    return ret;
}

/*
 * Callback folosit de libpng pentru a citi datele PNG direct din memorie.
 * In felul acesta nu mai este nevoie de un fisier intermediar pe disc.
 */
static void png_memory_read_callback(png_structp png_ptr,
                                     png_bytep out_bytes,
                                     png_size_t byte_count_to_read) {
    PngMemoryReader *reader;

    reader = (PngMemoryReader *)png_get_io_ptr(png_ptr);
    if (reader == NULL || out_bytes == NULL) {
        png_error(png_ptr, "reader PNG invalid");
    }

    if (reader->offset + byte_count_to_read > reader->size) {
        png_error(png_ptr, "date PNG insuficiente");
    }

    memcpy(out_bytes, reader->data + reader->offset, byte_count_to_read);
    reader->offset += byte_count_to_read;
}

/*
 * Incearca sa ruleze optipng -o7, daca programul exista in sistem.
 * Daca nu exista sau nu merge, fisierul ramane asa cum a fost scris de libpng.
 */
static void try_run_optipng(const char *output_path) {
    pid_t pid;
    int status;

    if (output_path == NULL) {
        return;
    }

    /* verificam cateva locatii posibile unde ar putea exista optipng */
    if (access("/opt/homebrew/bin/optipng", X_OK) != 0 &&
        access("/usr/local/bin/optipng", X_OK) != 0 &&
        access("/usr/bin/optipng", X_OK) != 0) {
        return;
    }

    pid = fork();
    if (pid < 0) {
        fprintf(stderr, "[PROCESSOR] nu pot porni optipng\n");
        return;
    }

    if (pid == 0) {
        if (access("/opt/homebrew/bin/optipng", X_OK) == 0) {
            execl("/opt/homebrew/bin/optipng", "optipng", "-o7", (char *)output_path, (char *)NULL);
        } else if (access("/usr/local/bin/optipng", X_OK) == 0) {
            execl("/usr/local/bin/optipng", "optipng", "-o7", (char *)output_path, (char *)NULL);
        } else {
            execl("/usr/bin/optipng", "optipng", "-o7", (char *)output_path, (char *)NULL);
        }

        _exit(127);
    }

    if (waitpid(pid, &status, 0) < 0) {
        fprintf(stderr, "[PROCESSOR] waitpid pentru optipng a esuat\n");
        return;
    }

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        fprintf(stderr, "[PROCESSOR] optipng -o7 a rulat cu succes pe %s\n", output_path);
    } else {
        fprintf(stderr, "[PROCESSOR] optipng nu a reusit pentru %s, pastrez fisierul rescris de libpng\n", output_path);
    }
}

/*
 * Intoarce dimensiunea unui fisier in bytes.
 * Daca apare o eroare, intoarce 0.
 */
static size_t get_file_size(const char *path) {
    FILE *f;
    long size;

    if (path == NULL) {
        return 0;
    }

    f = fopen(path, "rb");
    if (f == NULL) {
        return 0;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return 0;
    }

    size = ftell(f);
    fclose(f);

    if (size < 0) {
        return 0;
    }

    return (size_t)size;
}

/*
 * Comprima o imagine PNG si o rescrie in fisier.
 *
 * Pasii principali:
 *  1. citeste imaginea PNG din memorie
 *  2. transforma datele intr-un format uniform
 *  3. rescrie imaginea cu nivel maxim de compresie
 *  4. optional, ruleaza optipng
 *
 * Intoarce:
 *  - 0 la succes
 *  - -1 la eroare
 */
static int compress_png_to_file(const unsigned char *input_data,
                                size_t input_size,
                                const char *output_path,
                                size_t *final_size) {
    PngMemoryReader reader;
    png_structp read_ptr = NULL;
    png_infop read_info = NULL;
    png_structp write_ptr = NULL;
    png_infop write_info = NULL;
    png_bytep image_data = NULL;
    png_bytep *row_pointers = NULL;
    FILE *f = NULL;
    png_uint_32 width = 0;
    png_uint_32 height = 0;
    int bit_depth = 0;
    int color_type = 0;
    int interlace_type = 0;
    int compression_type = 0;
    int filter_method = 0;
    int channels = 0;
    size_t rowbytes = 0;
    size_t i;
    int ret = -1;

    if (input_data == NULL || input_size == 0 || output_path == NULL) {
        fprintf(stderr, "[PROCESSOR] date invalide pentru compresie PNG\n");
        return -1;
    }

    memset(&reader, 0, sizeof(reader));
    reader.data = input_data;
    reader.size = input_size;
    reader.offset = 0;

    read_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (read_ptr == NULL) {
        fprintf(stderr, "[PROCESSOR] png_create_read_struct a esuat\n");
        goto cleanup;
    }

    read_info = png_create_info_struct(read_ptr);
    if (read_info == NULL) {
        fprintf(stderr, "[PROCESSOR] png_create_info_struct pentru citire a esuat\n");
        goto cleanup;
    }

    if (setjmp(png_jmpbuf(read_ptr))) {
        fprintf(stderr, "[PROCESSOR] eroare la citirea PNG\n");
        goto cleanup;
    }

    png_set_read_fn(read_ptr, &reader, png_memory_read_callback);
    png_read_info(read_ptr, read_info);

    png_get_IHDR(read_ptr,
                 read_info,
                 &width,
                 &height,
                 &bit_depth,
                 &color_type,
                 &interlace_type,
                 &compression_type,
                 &filter_method);

    /* transformam datele astfel incat scrierea sa fie mai simpla */
    if (bit_depth == 16) {
        png_set_strip_16(read_ptr);
    }

    if (color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(read_ptr);
    }

    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
        png_set_expand_gray_1_2_4_to_8(read_ptr);
    }

    if (png_get_valid(read_ptr, read_info, PNG_INFO_tRNS)) {
        png_set_tRNS_to_alpha(read_ptr);
    }

    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_gray_to_rgb(read_ptr);
    }

    png_read_update_info(read_ptr, read_info);

    rowbytes = png_get_rowbytes(read_ptr, read_info);
    channels = png_get_channels(read_ptr, read_info);

    image_data = (png_bytep)malloc(rowbytes * (size_t)height);
    if (image_data == NULL) {
        fprintf(stderr, "[PROCESSOR] malloc a esuat pentru datele PNG\n");
        goto cleanup;
    }

    row_pointers = (png_bytep *)malloc(sizeof(png_bytep) * (size_t)height);
    if (row_pointers == NULL) {
        fprintf(stderr, "[PROCESSOR] malloc a esuat pentru liniile PNG\n");
        goto cleanup;
    }

    /* fiecare element indica inceputul unei linii din imagine */
    for (i = 0; i < (size_t)height; i++) {
        row_pointers[i] = image_data + i * rowbytes;
    }

    png_read_image(read_ptr, row_pointers);
    png_read_end(read_ptr, NULL);

    f = fopen(output_path, "wb");
    if (f == NULL) {
        fprintf(stderr, "[PROCESSOR] nu pot crea fisierul %s: %s\n", output_path, strerror(errno));
        goto cleanup;
    }

    write_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (write_ptr == NULL) {
        fprintf(stderr, "[PROCESSOR] png_create_write_struct a esuat\n");
        goto cleanup;
    }

    write_info = png_create_info_struct(write_ptr);
    if (write_info == NULL) {
        fprintf(stderr, "[PROCESSOR] png_create_info_struct pentru scriere a esuat\n");
        goto cleanup;
    }

    if (setjmp(png_jmpbuf(write_ptr))) {
        fprintf(stderr, "[PROCESSOR] eroare la scrierea PNG\n");
        goto cleanup;
    }

    png_init_io(write_ptr, f);

    /* setam compresia maxima pentru fisierul nou */
    png_set_compression_level(write_ptr, Z_BEST_COMPRESSION);
    png_set_compression_mem_level(write_ptr, 9);
    png_set_compression_strategy(write_ptr, Z_DEFAULT_STRATEGY);
    png_set_filter(write_ptr, PNG_FILTER_TYPE_BASE, PNG_ALL_FILTERS);

    png_set_IHDR(write_ptr,
                 write_info,
                 width,
                 height,
                 8,
                 channels == 4 ? PNG_COLOR_TYPE_RGBA : PNG_COLOR_TYPE_RGB,
                 PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_BASE,
                 PNG_FILTER_TYPE_BASE);

    png_write_info(write_ptr, write_info);
    png_write_image(write_ptr, row_pointers);
    png_write_end(write_ptr, NULL);

    fclose(f);
    f = NULL;

    /* daca exista optipng, incercam o optimizare suplimentara */
    try_run_optipng(output_path);

    if (final_size != NULL) {
        *final_size = get_file_size(output_path);
    }

    fprintf(stderr,
            "[PROCESSOR] fisier optimizat salvat: %s (dimensiune noua: %zu bytes)\n",
            output_path,
            final_size != NULL ? *final_size : get_file_size(output_path));

    ret = 0;

cleanup:
    /* eliberam memoria si structurile libpng */
    if (f != NULL) {
        fclose(f);
    }
    if (row_pointers != NULL) {
        free(row_pointers);
    }
    if (image_data != NULL) {
        free(image_data);
    }
    if (read_ptr != NULL && read_info != NULL) {
        png_destroy_read_struct(&read_ptr, &read_info, NULL);
    } else if (read_ptr != NULL) {
        png_destroy_read_struct(&read_ptr, NULL, NULL);
    }

    if (write_ptr != NULL && write_info != NULL) {
        png_destroy_write_struct(&write_ptr, &write_info);
    } else if (write_ptr != NULL) {
        png_destroy_write_struct(&write_ptr, NULL);
    }

    return ret;
}

/*
 * Functia principala de procesare pentru o sarcina primita.
 *
 * Pasii sunt:
 *  1. verifica daca task-ul este valid
 *  2. detecteaza tipul imaginii
 *  3. construieste numele fisierului de iesire
 *  4. apeleaza functia potrivita pentru JPEG sau PNG
 *  5. actualizeaza stats.txt
 *
 * Intoarce:
 *  - 0 la succes
 *  - -1 la eroare
 */
int process_image_task(const ImageTask *task) {
    char output_name[PATH_MAX];
    int rc;
    size_t final_size = 0;
    int is_jpeg;
    int is_png;

    if (task == NULL) {
        return -1;
    }

    if (task->data == NULL || task->size == 0) {
        fprintf(stderr, "[PROCESSOR] task invalid: lipsesc datele imaginii\n");
        return -1;
    }

    /* mai intai incercam detectia dupa semnatura reala a fisierului */
    is_jpeg = is_jpeg_data(task->data, task->size);
    is_png = is_png_data(task->data, task->size);

    /* daca nu reuseste, incercam si dupa extensie */
    if (!is_jpeg && !is_png && task->filename != NULL) {
        if (has_extension(task->filename, ".jpg") || has_extension(task->filename, ".jpeg")) {
            is_jpeg = 1;
        } else if (has_extension(task->filename, ".png")) {
            is_png = 1;
        }
    }

    if (!is_jpeg && !is_png) {
        fprintf(stderr,
                "[PROCESSOR] format nesuportat pentru %s. Sunt acceptate doar JPEG si PNG.\n",
                task->filename != NULL ? task->filename : "(fara_nume)");

        update_stats_after_processing(task->client_id,
                                      task->filename,
                                      task->size,
                                      0,
                                      "UNSUPPORTED_FORMAT");
        return -1;
    }

    build_output_name(task->filename, is_png, output_name, sizeof(output_name));

    fprintf(stderr,
            "[PROCESSOR] procesez imaginea %s pentru clientul %d\n",
            task->filename != NULL ? task->filename : "(fara_nume)",
            task->client_id);

    /* alegem functia potrivita in functie de tipul imaginii */
    if (is_jpeg) {
        rc = compress_jpeg_to_file(task->data, task->size, output_name, &final_size);
    } else {
        rc = compress_png_to_file(task->data, task->size, output_name, &final_size);
    }

    if (rc == 0) {
        update_stats_after_processing(task->client_id,
                                      task->filename,
                                      task->size,
                                      final_size,
                                      "OPTIMIZED");

        fprintf(stderr,
                "[PROCESSOR] optimizarea s-a terminat cu succes pentru %s\n",
                task->filename != NULL ? task->filename : "(fara_nume)");
        return 0;
    }

    update_stats_after_processing(task->client_id,
                                  task->filename,
                                  task->size,
                                  0,
                                  "OPTIMIZATION_FAILED");

    fprintf(stderr,
            "[PROCESSOR] optimizarea a esuat pentru %s\n",
            task->filename != NULL ? task->filename : "(fara_nume)");
    return -1;
}

/*
Exemple de rulare:

Fisierul este folosit din cadrul proiectului, nu se ruleaza direct separat.

Comportament:
- verifica daca imaginea primita este JPEG sau PNG
- pentru JPEG face recomprimare cu turbojpeg
- pentru PNG face rescriere cu libpng si compresie maxima
- optional incearca optimizare suplimentara cu optipng
- salveaza fisierul rezultat cu un nume nou
- actualizeaza stats.txt dupa fiecare procesare
*/