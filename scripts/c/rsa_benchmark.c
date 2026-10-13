#include "common_bench.h"
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/rand.h>
#include <openssl/x509.h>
#include <openssl/err.h>

#define SECRET_SIZE 32 /* 256 bits para clave simétrica (AES-256 equiv) */

typedef struct {
    int runs;
    int warmup;
    int key_bits;
    int batch_size;
    char device[64];
    char id_ejecucion[64];
    char output_path[256];
} BenchmarkConfig;

static void print_usage(const char* prog_name) {
    printf("Uso: %s [opciones]\n", prog_name);
    printf("Opciones:\n");
    printf("  --runs <N>          Numero de ejecuciones formales (defecto: 5000)\n");
    printf("  --warmup <N>        Numero de ejecuciones de calentamiento (defecto: 20)\n");
    printf("  --level <bits>      Tamano de clave RSA: 2048, 3072 o 4096 (defecto: 2048)\n");
    printf("  --device <nombre>   Nombre del dispositivo (defecto: 'dispositivo')\n");
    printf("  --ejecucion <id>    Identificador de la serie de ejecuciones (defecto: 'ejecucion_01')\n");
    printf("  --corrida <id>      Alias de --ejecucion por compatibilidad\n");
    printf("  --output <ruta>     Ruta del archivo CSV de salida\n");
    printf("  --batch <N>         Tamano de bloque para medicion de CPU (defecto: 50)\n");
    printf("  --help              Muestra esta ayuda\n");
}

static void parse_arguments(int argc, char* argv[], BenchmarkConfig* cfg) {
    /* Valores por defecto */
    cfg->runs = 5000;
    cfg->warmup = 20;
    cfg->key_bits = 2048;
    cfg->batch_size = 50;
    snprintf(cfg->device, sizeof(cfg->device), "host");
    snprintf(cfg->id_ejecucion, sizeof(cfg->id_ejecucion), "ejecucion_01");
    snprintf(cfg->output_path, sizeof(cfg->output_path), "rsa_results_%d.csv", cfg->key_bits);

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--runs") == 0 && i + 1 < argc) {
            cfg->runs = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--warmup") == 0 && i + 1 < argc) {
            cfg->warmup = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--level") == 0 && i + 1 < argc) {
            cfg->key_bits = atoi(argv[++i]);
            snprintf(cfg->output_path, sizeof(cfg->output_path), "rsa_results_%d.csv", cfg->key_bits);
        } else if (strcmp(argv[i], "--device") == 0 && i + 1 < argc) {
            strncpy(cfg->device, argv[++i], sizeof(cfg->device) - 1);
        } else if ((strcmp(argv[i], "--ejecucion") == 0 || strcmp(argv[i], "--corrida") == 0) && i + 1 < argc) {
            strncpy(cfg->id_ejecucion, argv[++i], sizeof(cfg->id_ejecucion) - 1);
        } else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            strncpy(cfg->output_path, argv[++i], sizeof(cfg->output_path) - 1);
        } else if (strcmp(argv[i], "--batch") == 0 && i + 1 < argc) {
            cfg->batch_size = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            exit(0);
        }
    }
}

static bool run_single_cycle(int key_bits, IterationMetric* m) {
    bool success = false;
    EVP_PKEY_CTX* kgen_ctx = NULL;
    EVP_PKEY* pkey = NULL;
    EVP_PKEY_CTX* enc_ctx = NULL;
    EVP_PKEY_CTX* dec_ctx = NULL;

    unsigned char secret[SECRET_SIZE];
    unsigned char ct[1024];
    size_t ct_len = sizeof(ct);
    unsigned char dec_secret[1024];
    size_t dec_len = sizeof(dec_secret);

    /* Generar secreto aleatorio a transportar */
    if (RAND_bytes(secret, SECRET_SIZE) <= 0) {
        m->estado = "ERROR_RNG";
        return false;
    }

    /* -------------------------------------------------------------
     * FASE 1: KeyGen
     * ------------------------------------------------------------- */
    double t0 = bench_get_time_sec();

    kgen_ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
    if (!kgen_ctx || EVP_PKEY_keygen_init(kgen_ctx) <= 0 ||
        EVP_PKEY_CTX_set_rsa_keygen_bits(kgen_ctx, key_bits) <= 0 ||
        EVP_PKEY_keygen(kgen_ctx, &pkey) <= 0) {
        m->estado = "ERROR_KEYGEN";
        goto cleanup;
    }

    double t1 = bench_get_time_sec();
    m->tiempo_keygen_s = t1 - t0;

    /* Obtener tamaños reales en DER (SubjectPublicKeyInfo y PKCS#8) */
    int pk_len = i2d_PUBKEY(pkey, NULL);
    int sk_len = i2d_PrivateKey(pkey, NULL);
    m->tamano_pk = (pk_len > 0) ? (size_t)pk_len : 0;
    m->tamano_sk = (sk_len > 0) ? (size_t)sk_len : 0;
    m->tamano_ss = SECRET_SIZE;

    /* -------------------------------------------------------------
     * FASE 2: Encapsulation / Encryption (RSA-OAEP-SHA256)
     * ------------------------------------------------------------- */
    t0 = bench_get_time_sec();

    enc_ctx = EVP_PKEY_CTX_new(pkey, NULL);
    if (!enc_ctx || EVP_PKEY_encrypt_init(enc_ctx) <= 0 ||
        EVP_PKEY_CTX_set_rsa_padding(enc_ctx, RSA_PKCS1_OAEP_PADDING) <= 0 ||
        EVP_PKEY_CTX_set_rsa_oaep_md(enc_ctx, EVP_sha256()) <= 0 ||
        EVP_PKEY_CTX_set_rsa_mgf1_md(enc_ctx, EVP_sha256()) <= 0 ||
        EVP_PKEY_encrypt(enc_ctx, ct, &ct_len, secret, SECRET_SIZE) <= 0) {
        m->estado = "ERROR_ENCRYPT";
        goto cleanup;
    }

    t1 = bench_get_time_sec();
    m->tiempo_enc_s = t1 - t0;
    m->tamano_ct = ct_len;

    /* -------------------------------------------------------------
     * FASE 3: Decapsulation / Decryption (RSA-OAEP-SHA256)
     * ------------------------------------------------------------- */
    t0 = bench_get_time_sec();

    dec_ctx = EVP_PKEY_CTX_new(pkey, NULL);
    if (!dec_ctx || EVP_PKEY_decrypt_init(dec_ctx) <= 0 ||
        EVP_PKEY_CTX_set_rsa_padding(dec_ctx, RSA_PKCS1_OAEP_PADDING) <= 0 ||
        EVP_PKEY_CTX_set_rsa_oaep_md(dec_ctx, EVP_sha256()) <= 0 ||
        EVP_PKEY_CTX_set_rsa_mgf1_md(dec_ctx, EVP_sha256()) <= 0 ||
        EVP_PKEY_decrypt(dec_ctx, dec_secret, &dec_len, ct, ct_len) <= 0) {
        m->estado = "ERROR_DECRYPT";
        goto cleanup;
    }

    t1 = bench_get_time_sec();
    m->tiempo_dec_s = t1 - t0;
    m->tiempo_total_s = m->tiempo_keygen_s + m->tiempo_enc_s + m->tiempo_dec_s;

    /* Verificación criptográfica */
    if (dec_len == SECRET_SIZE && memcmp(secret, dec_secret, SECRET_SIZE) == 0) {
        m->estado = "OK";
        success = true;
    } else {
        m->estado = "ERROR_VERIFY_MISMATCH";
    }

cleanup:
    if (enc_ctx) EVP_PKEY_CTX_free(enc_ctx);
    if (dec_ctx) EVP_PKEY_CTX_free(dec_ctx);
    if (kgen_ctx) EVP_PKEY_CTX_free(kgen_ctx);
    if (pkey) EVP_PKEY_free(pkey);
    return success;
}

int main(int argc, char* argv[]) {
    BenchmarkConfig cfg;
    parse_arguments(argc, argv, &cfg);
    int success_count = 0;

    char level_str[32];
    snprintf(level_str, sizeof(level_str), "RSA-%d", cfg.key_bits);

    printf("===================================================================\n");
    printf(" INICIANDO BENCHMARK RSA C: %s\n", level_str);
    printf(" Dispositivo: %s | Ejecucion: %s\n", cfg.device, cfg.id_ejecucion);
    printf(" Warmup: %d | Ejecuciones formales: %d | Batch CPU: %d\n", cfg.warmup, cfg.runs, cfg.batch_size);
    printf(" Archivo de salida: %s\n", cfg.output_path);
    printf("===================================================================\n");

    /* Temperatura inicial */
    double temp_inicio = bench_get_temperature_celsius();
    if (temp_inicio > 0) {
        printf("[INFO] Temperatura inicial de CPU: %.1f C\n", temp_inicio);
    }

    /* Ajuste dinámico de batch_size si runs es menor */
    if (cfg.batch_size > cfg.runs) {
        cfg.batch_size = (cfg.runs >= 20) ? (cfg.runs / 10) : 1;
    }

    double current_cpu_pct = 0.0;

    /* Calentamiento */
    if (cfg.warmup > 0) {
        printf("[FASE 1/3] Realizando calentamiento (%d ejecuciones)... ", cfg.warmup);
        fflush(stdout);
        ProcessCpuSnapshot warmup_start = bench_get_cpu_snapshot();
        for (int w = 0; w < cfg.warmup; w++) {
            IterationMetric wm = {0};
            run_single_cycle(cfg.key_bits, &wm);
        }
        ProcessCpuSnapshot warmup_end = bench_get_cpu_snapshot();
        double w_cpu = bench_calculate_cpu_percent(warmup_start, warmup_end);
        if (w_cpu > 0.0) {
            current_cpu_pct = w_cpu;
        }
        printf("Listo (CPU estimada: %.1f%%).\n", current_cpu_pct);
    }

    /* Abrir archivo CSV */
    FILE* fp = fopen(cfg.output_path, "w");
    if (!fp) {
        fprintf(stderr, "[ERROR] No se pudo crear el archivo de salida: %s\n", cfg.output_path);
        return 1;
    }
    bench_write_csv_header(fp);

    /* Medición formal */
    printf("[FASE 2/3] Realizando serie de ejecuciones formal (%d iteraciones)...\n", cfg.runs);
    double global_start_time = bench_get_time_sec();

    ProcessCpuSnapshot batch_cpu_start = bench_get_cpu_snapshot();

    for (int i = 1; i <= cfg.runs; i++) {
        IterationMetric m = {0};
        bool ok = run_single_cycle(cfg.key_bits, &m);
        if (ok) success_count++;

        /* Memoria RSS actual */
        m.memoria_kb = bench_get_memory_kb();

        /* Actualizar métrica de CPU en intervalos de bloque para evitar ruido */
        if (i % cfg.batch_size == 0 || i == cfg.runs) {
            ProcessCpuSnapshot batch_cpu_end = bench_get_cpu_snapshot();
            current_cpu_pct = bench_calculate_cpu_percent(batch_cpu_start, batch_cpu_end);
            batch_cpu_start = batch_cpu_end;
        }
        m.cpu_percent = current_cpu_pct;

        bench_write_csv_row(fp, i, &m, cfg.device, "RSA", level_str, cfg.id_ejecucion);

        if (i % 500 == 0 || i == cfg.runs) {
            double elapsed = bench_get_time_sec() - global_start_time;
            double ops_sec = (double)i / elapsed;
            printf("  -> Progreso: %d/%d (%.1f%%) | %.2f ops/s | Mem: %ld KB | CPU: %.1f%%\n",
                   i, cfg.runs, ((double)i / cfg.runs) * 100.0, ops_sec, m.memoria_kb, m.cpu_percent);
        }
    }

    double global_end_time = bench_get_time_sec();
    fclose(fp);

    /* Temperatura final */
    double temp_fin = bench_get_temperature_celsius();
    double total_elapsed = global_end_time - global_start_time;
    double avg_throughput = (double)cfg.runs / total_elapsed;

    printf("[FASE 3/3] Finalizado exitosamente.\n");
    printf("-------------------------------------------------------------------\n");
    printf(" Resumen de la serie de ejecuciones:\n");
    printf("  * Iteraciones exitosas: %d / %d (%.2f%%)\n", success_count, cfg.runs, ((double)success_count / cfg.runs) * 100.0);
    printf("  * Tiempo total de ejecuciones: %.3f segundos\n", total_elapsed);
    printf("  * Rendimiento promedio: %.2f ops/segundo\n", avg_throughput);
    if (temp_inicio > 0 && temp_fin > 0) {
        printf("  * Temperatura CPU: Inicial = %.1f C | Final = %.1f C (Delta: %+.1f C)\n",
               temp_inicio, temp_fin, temp_fin - temp_inicio);
    }
    printf("  * Archivo guardado: %s\n", cfg.output_path);
    printf("===================================================================\n");

    return (success_count == cfg.runs) ? 0 : 2;
}
