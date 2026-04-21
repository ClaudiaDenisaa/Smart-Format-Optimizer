#include "image_processor.h"
#include "proto.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <turbojpeg.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define JPEG_QUALITY 75

static const char *safe_filename(const char *name) {
    const char *p;

    if (name == NULL || *name == '\0') {
        return "image.jpg";
    }

    p = strrchr(name, '/');
    if (p != NULL && *(p + 1) != '\0') {
        return p + 1;
    }

    return name;
}

static void build_output_name(const char *input_name, char *out, size_t out_size) {
    const char *base;
    char tmp[256];
    char *dot;

    base = safe_filename(input_name);

    memset(tmp, 0, sizeof(tmp));
    strncpy(tmp, base, sizeof(tmp) - 1);

    dot = strrchr(tmp, '.');
    if (dot != NULL) {
        *dot = '\0';
    }

    snprintf(out, out_size, "optimized_%s_q75.jpg", tmp);
}

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

static void update_stats_after_processing(int client_id,
                                          const char *filename,
                                          size_t initial_size,
                                          size_t final_size,
                                          const char *status) {
    FILE *f;
    int count;

    count = read_images_processed_count();

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
        fprintf(stderr, "[PROCESSOR] date invalide pentru compresie\n");
        return -1;
    }

    dec_handle = tjInitDecompress();
    if (dec_handle == NULL) {
        fprintf(stderr, "[PROCESSOR] tjInitDecompress a esuat: %s\n", tjGetErrorStr());
        goto cleanup;
    }

    if (tjDecompressHeader3(dec_handle, input_data, (unsigned long)input_size,
                            &width, &height, &subsamp, &colorspace) != 0) {
        fprintf(stderr, "[PROCESSOR] tjDecompressHeader3 a esuat: %s\n", tjGetErrorStr());
        goto cleanup;
    }

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

int process_image_task(const ImageTask *task) {
    int type;
    char output_name[PATH_MAX];
    int rc;
    size_t final_size = 0;

    if (task == NULL) {
        return -1;
    }

    if (task->data == NULL || task->size == 0) {
        fprintf(stderr, "[PROCESSOR] task invalid: lipsesc datele imaginii\n");
        return -1;
    }

    type = detect_image_format(task->data, (int)task->size);
    if (type != 1) {
        fprintf(stderr,
                "[PROCESSOR] imaginea %s nu este JPEG. Momentan optimizez doar JPEG.\n",
                task->filename != NULL ? task->filename : "(fara_nume)");
        return -1;
    }

    build_output_name(task->filename, output_name, sizeof(output_name));

    fprintf(stderr,
            "[PROCESSOR] procesez imaginea %s pentru clientul %d\n",
            task->filename != NULL ? task->filename : "(fara_nume)",
            task->client_id);

    rc = compress_jpeg_to_file(task->data, task->size, output_name, &final_size);
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
