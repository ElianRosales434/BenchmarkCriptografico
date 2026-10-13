#include "common_bench.h"
#include <oqs/oqs.h>

typedef struct {
    int runs;
    int warmup;
    int level; /* 512, 768 o 1024 */
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
    printf("  --level <512|768|1024> Nivel de seguridad de Kyber/ML-KEM (defecto: 512)\n");
    printf("  --device <nombre>   Nombre del dispositivo (defecto: 'host')\n");
    printf("  --ejecucion <id>    Identificador de la serie de ejecuciones (defecto: 'ejecucion_01')\n");
    printf("  --corrida <id>      Alias de --ejecucion por compatibilidad\n");
    printf("  --output <ruta>     Ruta del archivo CSV de salida\n");
    printf("  --batch <N>         Tamano de bloque para medicion de CPU (defecto: 500)\n");
    printf("  --help              Muestra esta ayuda\n");
}

static void parse_arguments(int argc, char* argv[], BenchmarkConfig* cfg) {
    /* Valores por defecto */
    cfg->runs = 5000;
    cfg->warmup = 20;
    cfg->level = 512;
    cfg->batch_size = 500;
    snprintf(cfg->device, sizeof(cfg->device), "host");
    snprintf(cfg->id_ejecucion, sizeof(cfg->id_ejecucion), "ejecucion_01");
    snprintf(cfg->output_path, sizeof(cfg->output_path), "mlkem_results_%d.csv", cfg->level);

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--runs") == 0 && i + 1 < argc) {
            cfg->runs = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--warmup") == 0 && i + 1 < argc) {
            cfg->warmup = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--level") == 0 && i + 1 < argc) {
            cfg->level = atoi(argv[++i]);
            snprintf(cfg->output_path, sizeof(cfg->output_path), "mlkem_results_%d.csv", cfg->level);
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

/* Resuelve el nombre del algoritmo según la versión de liboqs (soporta ML-KEM y Kyber legacy) */
static const char* resolve_algorithm_name(int level) {
    if (level == 512) {
        if (OQS_KEM_alg_is_enabled("ML-KEM-512")) return "ML-KEM-512";
        if (OQS_KEM_alg_is_enabled("Kyber512")) return "Kyber512";
    } else if (level == 768) {
        if (OQS_KEM_alg_is_enabled("ML-KEM-768")) return "ML-KEM-768";
        if (OQS_KEM_alg_is_enabled("Kyber768")) return "Kyber768";
    } else if (level == 1024) {
        if (OQS_KEM_alg_is_enabled("ML-KEM-1024")) return "ML-KEM-1024";
        if (OQS_KEM_alg_is_enabled("Kyber1024")) return "Kyber1024";
    }
    return NULL;
}

static bool run_single_kem_cycle(OQS_KEM* kem,
                                 uint8_t* public_key,
                                 uint8_t* secret_key,
                                 uint8_t* ciphertext,
                                 uint8_t* shared_secret_e,
                                 uint8_t* shared_secret_d,
                                 IterationMetric* m) {
    /* -------------------------------------------------------------
     * FASE 1: KeyGen
     * ------------------------------------------------------------- */
    double t0 = bench_get_time_sec();
    if (OQS_KEM_keypair(kem, public_key, secret_key) != OQS_SUCCESS) {
        m->estado = "ERROR_KEYPAIR";
        return false;
    }
    double t1 = bench_get_time_sec();
    m->tiempo_keygen_s = t1 - t0;

    /* -------------------------------------------------------------
     * FASE 2: Encapsulation
     * ------------------------------------------------------------- */
    t0 = bench_get_time_sec();
    if (OQS_KEM_encaps(kem, ciphertext, shared_secret_e, public_key) != OQS_SUCCESS) {
        m->estado = "ERROR_ENCAPS";
        return false;
    }
    t1 = bench_get_time_sec();
    m->tiempo_enc_s = t1 - t0;

    /* -------------------------------------------------------------
     * FASE 3: Decapsulation
     * ------------------------------------------------------------- */
    t0 = bench_get_time_sec();
    if (OQS_KEM_decaps(kem, shared_secret_d, ciphertext, secret_key) != OQS_SUCCESS) {
        m->estado = "ERROR_DECAPS";
        return false;
    }
    t1 = bench_get_time_sec();
    m->tiempo_dec_s = t1 - t0;
    m->tiempo_total_s = m->tiempo_keygen_s + m->tiempo_enc_s + m->tiempo_dec_s;

    /* Tamaños en bytes */
    m->tamano_pk = kem->length_public_key;
    m->tamano_sk = kem->length_secret_key;
    m->tamano_ct = kem->length_ciphertext;
    m->tamano_ss = kem->length_shared_secret;

    /* Verificación de concordancia criptográfica */
    if (memcmp(shared_secret_e, shared_secret_d, kem->length_shared_secret) == 0) {
        m->estado = "OK";
        return true;
    } else {
        m->estado = "ERROR_VERIFY_MISMATCH";
        return false;
    }
}

int main(int argc, char* argv[]) {
    BenchmarkConfig cfg;
    parse_arguments(argc, argv, &cfg);

    OQS_init();

    const char* alg_name = resolve_algorithm_name(cfg.level);
    if (!alg_name) {
        fprintf(stderr, "[ERROR] Nivel de seguridad %d no encontrado o no habilitado en liboqs.\n", cfg.level);
        OQS_destroy();
        return 1;
    }

    OQS_KEM* kem = OQS_KEM_new(alg_name);
    if (!kem) {
        fprintf(stderr, "[ERROR] No se pudo instanciar el KEM: %s\n", alg_name);
        OQS_destroy();
        return 1;
    }

    int success_count = 0;

    /* Buffers pre-asignados para evitar overhead de malloc/free en el loop */
    uint8_t* public_key = malloc(kem->length_public_key);
    uint8_t* secret_key = malloc(kem->length_secret_key);
    uint8_t* ciphertext = malloc(kem->length_ciphertext);
    uint8_t* shared_secret_e = malloc(kem->length_shared_secret);
    uint8_t* shared_secret_d = malloc(kem->length_shared_secret);

    if (!public_key || !secret_key || !ciphertext || !shared_secret_e || !shared_secret_d) {
        fprintf(stderr, "[ERROR] Falla de asignacion de memoria para buffers criptograficos.\n");
        OQS_KEM_free(kem);
        OQS_destroy();
        return 1;
    }

    printf("===================================================================\n");
    printf(" INICIANDO BENCHMARK POST-CUANTICO C: %s (Nivel %d)\n", alg_name, cfg.level);
    printf(" Dispositivo: %s | Ejecucion: %s\n", cfg.device, cfg.id_ejecucion);
    printf(" Parametros: PK=%zu B | SK=%zu B | CT=%zu B | SS=%zu B\n",
           kem->length_public_key, kem->length_secret_key, kem->length_ciphertext, kem->length_shared_secret);
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
            run_single_kem_cycle(kem, public_key, secret_key, ciphertext, shared_secret_e, shared_secret_d, &wm);
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
        goto cleanup;
    }
    bench_write_csv_header(fp);

    /* Medición formal */
    printf("[FASE 2/3] Realizando serie de ejecuciones formal (%d iteraciones)...\n", cfg.runs);
    double global_start_time = bench_get_time_sec();

    ProcessCpuSnapshot batch_cpu_start = bench_get_cpu_snapshot();

    for (int i = 1; i <= cfg.runs; i++) {
        IterationMetric m = {0};
        bool ok = run_single_kem_cycle(kem, public_key, secret_key, ciphertext, shared_secret_e, shared_secret_d, &m);
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

        char level_label[32];
        snprintf(level_label, sizeof(level_label), "%d", cfg.level);
        bench_write_csv_row(fp, i, &m, cfg.device, alg_name, level_label, cfg.id_ejecucion);

        if (i % 1000 == 0 || i == cfg.runs) {
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
    printf("  * Algoritmo evaluado: %s (Nivel NIST %d)\n", alg_name, cfg.level);
    printf("  * Iteraciones exitosas: %d / %d (%.2f%%)\n", success_count, cfg.runs, ((double)success_count / cfg.runs) * 100.0);
    printf("  * Tiempo total de ejecuciones: %.3f segundos\n", total_elapsed);
    printf("  * Rendimiento promedio: %.2f ops/segundo\n", avg_throughput);
    if (temp_inicio > 0 && temp_fin > 0) {
        printf("  * Temperatura CPU: Inicial = %.1f C | Final = %.1f C (Delta: %+.1f C)\n",
               temp_inicio, temp_fin, temp_fin - temp_inicio);
    }
    printf("  * Archivo guardado: %s\n", cfg.output_path);
    printf("===================================================================\n");

cleanup:
    free(public_key);
    free(secret_key);
    free(ciphertext);
    free(shared_secret_e);
    free(shared_secret_d);
    OQS_KEM_free(kem);
    OQS_destroy();

    return (success_count == cfg.runs) ? 0 : 2;
}
