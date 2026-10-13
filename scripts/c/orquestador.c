#include "common_bench.h"

#if defined(PLATFORM_WINDOWS)
    #include <direct.h>
    #define mkdir_cross(p) _mkdir(p)
    #define PATH_SEP "\\"
    #define EXE_EXT ".exe"
#else
    #include <sys/stat.h>
    #define mkdir_cross(p) mkdir(p, 0755)
    #define PATH_SEP "/"
    #define EXE_EXT ""
#endif

typedef struct {
    char mode[16];          /* "pilot" o "full" */
    int runs;
    int warmup;
    char device[64];
    char ejecucion[64];
    char output_dir[256];
    double target_temp;
    int max_cooldown;
    int min_cooldown;
    bool no_thermal_gate;
} OrquestadorConfig;

typedef struct {
    char algoritmo[32];
    int nivel;
    char archivo[512];
    int retorno;
    double tiempo_segundos;
    int filas_registradas;
    int esperadas;
} TaskResult;

typedef struct {
    const char* alg;
    int level;
    const char* bin_name;
} BenchmarkTask;

static const BenchmarkTask G_TASKS[] = {
    {"RSA",    2048, "rsa_benchmark"},
    {"RSA",    3072, "rsa_benchmark"},
    {"RSA",    4096, "rsa_benchmark"},
    {"ML-KEM", 512,  "kyber_benchmark"},
    {"ML-KEM", 768,  "kyber_benchmark"},
    {"ML-KEM", 1024, "kyber_benchmark"}
};
static const int G_NUM_TASKS = sizeof(G_TASKS) / sizeof(G_TASKS[0]);

static void print_usage(const char* prog) {
    printf("Uso: %s [opciones]\n\n", prog);
    printf("Opciones:\n");
    printf("  --mode <pilot|full>    Modo de ejecucion: 'pilot' (200 rep.) o 'full' (5,000 rep.)\n");
    printf("  --runs <N>             Sobrescribe el numero de ejecuciones formales\n");
    printf("  --warmup <N>           Numero de ejecuciones de calentamiento (defecto: 20)\n");
    printf("  --device <nombre>      Identificador del dispositivo (defecto: 'dispositivo')\n");
    printf("  --ejecucion <id>       Identificador de la serie (defecto: 'ejecucion_01' o 'piloto')\n");
    printf("  --corrida <id>         Alias de --ejecucion por compatibilidad\n");
    printf("  --output-dir <ruta>    Directorio donde se guardaran los CSV y el resumen JSON\n");
    printf("  --target-temp <grados> Temperatura de reposo objetivo en C (defecto: 50.0)\n");
    printf("  --max-cooldown <seg>   Tiempo maximo de espera por enfriamiento en segundos (defecto: 600)\n");
    printf("  --min-cooldown <seg>   Sobrescribe la pausa minima de reposo en segundos\n");
    printf("  --no-thermal-gate      Desactiva la compuerta termica automatizada\n");
    printf("  --help                 Muestra esta ayuda\n");
}

static void parse_arguments(int argc, char* argv[], OrquestadorConfig* cfg) {
    /* Valores predeterminados */
    snprintf(cfg->mode, sizeof(cfg->mode), "pilot");
    cfg->runs = 200;
    cfg->warmup = 20;
    snprintf(cfg->device, sizeof(cfg->device), "dispositivo");
    snprintf(cfg->ejecucion, sizeof(cfg->ejecucion), "piloto");
    cfg->output_dir[0] = '\0';
    cfg->target_temp = 50.0;
    cfg->max_cooldown = 600;
    cfg->min_cooldown = -1;
    cfg->no_thermal_gate = false;

    bool runs_explicit = false;
    bool ejec_explicit = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--mode") == 0 && i + 1 < argc) {
            snprintf(cfg->mode, sizeof(cfg->mode), "%s", argv[++i]);
            if (strcmp(cfg->mode, "full") == 0) {
                if (!runs_explicit) cfg->runs = 5000;
                if (!ejec_explicit) snprintf(cfg->ejecucion, sizeof(cfg->ejecucion), "ejecucion_01");
            } else {
                if (!runs_explicit) cfg->runs = 200;
                if (!ejec_explicit) snprintf(cfg->ejecucion, sizeof(cfg->ejecucion), "piloto");
            }
        } else if (strcmp(argv[i], "--runs") == 0 && i + 1 < argc) {
            cfg->runs = atoi(argv[++i]);
            runs_explicit = true;
        } else if (strcmp(argv[i], "--warmup") == 0 && i + 1 < argc) {
            cfg->warmup = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--device") == 0 && i + 1 < argc) {
            snprintf(cfg->device, sizeof(cfg->device), "%s", argv[++i]);
        } else if ((strcmp(argv[i], "--ejecucion") == 0 || strcmp(argv[i], "--corrida") == 0) && i + 1 < argc) {
            snprintf(cfg->ejecucion, sizeof(cfg->ejecucion), "%s", argv[++i]);
            ejec_explicit = true;
        } else if (strcmp(argv[i], "--output-dir") == 0 && i + 1 < argc) {
            snprintf(cfg->output_dir, sizeof(cfg->output_dir), "%s", argv[++i]);
        } else if (strcmp(argv[i], "--target-temp") == 0 && i + 1 < argc) {
            cfg->target_temp = atof(argv[++i]);
        } else if (strcmp(argv[i], "--max-cooldown") == 0 && i + 1 < argc) {
            cfg->max_cooldown = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--min-cooldown") == 0 && i + 1 < argc) {
            cfg->min_cooldown = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--no-thermal-gate") == 0) {
            cfg->no_thermal_gate = true;
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            exit(0);
        }
    }

    /* Si no se especifico output_dir, usar ruta por defecto */
    if (cfg->output_dir[0] == '\0') {
        snprintf(cfg->output_dir, sizeof(cfg->output_dir), "resultados" PATH_SEP "%s", cfg->ejecucion);
    }
}

static int count_csv_data_rows(const char* filepath) {
    FILE* fp = fopen(filepath, "r");
    if (!fp) return 0;
    int rows = 0;
    char buffer[1024];
    /* Descartar encabezado */
    if (fgets(buffer, sizeof(buffer), fp) != NULL) {
        while (fgets(buffer, sizeof(buffer), fp) != NULL) {
            if (buffer[0] != '\0' && buffer[0] != '\n' && buffer[0] != '\r') {
                rows++;
            }
        }
    }
    fclose(fp);
    return rows;
}

static void apply_thermal_gating(const char* prev_alg, int prev_lvl, const OrquestadorConfig* cfg) {
    if (cfg->no_thermal_gate) return;

    int min_wait = 20;
    if (cfg->min_cooldown >= 0) {
        min_wait = cfg->min_cooldown;
    } else {
        bool is_pilot = (strcmp(cfg->mode, "pilot") == 0);
        if (strcmp(prev_alg, "ML-KEM") == 0) {
            min_wait = 15;
        } else if (strcmp(prev_alg, "RSA") == 0) {
            if (prev_lvl == 2048) {
                min_wait = is_pilot ? 30 : 120;
            } else if (prev_lvl == 3072) {
                min_wait = is_pilot ? 60 : 180;
            } else { /* 4096 */
                min_wait = is_pilot ? 90 : 300;
            }
        }
    }

    printf("\n----------------------------------------------------------------------\n");
    printf(" [COMPUERTA TERMICA] Finalizado %s-%d.\n", prev_alg, prev_lvl);
    double cur_temp = bench_get_temperature_celsius();
    if (cur_temp > 0) {
        printf(" Temperatura de CPU: %.1f C | Umbral de reposo objetivo: <= %.1f C\n", cur_temp, cfg->target_temp);
    } else {
        printf(" Sensor termico no expuesto directamente (aplicando reposo temporizado).\n");
    }

    /* 1. Reposo base preventivo */
    if (min_wait > 0) {
        printf(" Aplicando reposo preventivo de %ds para estabilizacion del sistema...\n", min_wait);
        int rem = min_wait;
        while (rem > 0) {
            int step = (rem > 5) ? 5 : rem;
            bench_sleep_ms(step * 1000);
            rem -= step;
            double t_live = bench_get_temperature_celsius();
            if (t_live > 0) {
                printf("   -> Reposo base: quedan %ds | Temp CPU: %.1f C...   \r", rem, t_live);
            } else {
                printf("   -> Reposo base: quedan %ds...   \r", rem);
            }
            fflush(stdout);
        }
        printf("\n   [OK] Reposo preventivo completado.\n");
    }

    /* 2. Espera activa si la CPU supera el umbral target_temp */
    cur_temp = bench_get_temperature_celsius();
    if (cur_temp > 0 && cur_temp > cfg->target_temp) {
        printf(" [ENFRIAMIENTO ACTIVO] CPU a %.1f C (> %.1f C).\n", cur_temp, cfg->target_temp);
        printf(" Esperando retorno a temperatura base (maximo %ds)...\n", cfg->max_cooldown);
        double t_start = bench_get_time_sec();
        while (true) {
            double t_live = bench_get_temperature_celsius();
            double elapsed = bench_get_time_sec() - t_start;
            if (t_live <= cfg->target_temp) {
                printf("   [OK] Temperatura de reposo alcanzada: %.1f C (tras %.0fs de enfriamiento).\n", t_live, elapsed);
                break;
            }
            if ((int)elapsed >= cfg->max_cooldown) {
                printf("   [AVISO] Tiempo maximo de enfriamiento alcanzado (%ds). Temp: %.1f C. Continuando...\n",
                       cfg->max_cooldown, t_live);
                break;
            }
            printf("   -> Enfriando: %.1f C (objetivo <= %.1f C | %.0fs transcurridos)...   \r",
                   t_live, cfg->target_temp, elapsed);
            fflush(stdout);
            bench_sleep_ms(5000);
        }
    }

    printf(" [LISTO] Condiciones termicas optimas verificadas. Iniciando siguiente prueba.\n");
    printf("----------------------------------------------------------------------\n\n");
}

int main(int argc, char* argv[]) {
    OrquestadorConfig cfg;
    parse_arguments(argc, argv, &cfg);

    printf("======================================================================\n");
    printf(" ORQUESTADOR DE EXPERIMENTACION CRIPTOGRAFICA (SUITE C PURA)\n");
    printf(" Modo: %s (%d ejecuciones por nivel)\n", cfg.mode, cfg.runs);
    printf(" Dispositivo: %s | Serie de ejecuciones: %s\n", cfg.device, cfg.ejecucion);
    printf(" Directorio de salida: %s\n", cfg.output_dir);
    if (!cfg.no_thermal_gate) {
        printf(" Compuerta termica activa: Objetivo <= %.1f C | Max cooldown: %ds\n",
               cfg.target_temp, cfg.max_cooldown);
    } else {
        printf(" Compuerta termica: DESACTIVADA (--no-thermal-gate)\n");
    }
    printf("======================================================================\n\n");

    /* Crear directorios de salida */
    mkdir_cross("resultados");
    mkdir_cross(cfg.output_dir);

    /* Verificación térmica previa al arranque general */
    double temp_inicio_suite = bench_get_temperature_celsius();
    if (!cfg.no_thermal_gate && temp_inicio_suite > 0) {
        printf("[INFO] Temperatura inicial de CPU del sistema: %.1f C\n", temp_inicio_suite);
        if (temp_inicio_suite > cfg.target_temp) {
            printf("[AVISO] La CPU esta inicialmente por encima del objetivo (%.1f C).\n", cfg.target_temp);
            printf("Esperando reposo inicial de enfriamiento...\n");
            double t_wait = bench_get_time_sec();
            while (temp_inicio_suite > cfg.target_temp && (bench_get_time_sec() - t_wait) < 180.0) {
                bench_sleep_ms(5000);
                temp_inicio_suite = bench_get_temperature_celsius();
                printf("   -> Enfriando CPU: %.1f C...   \r", temp_inicio_suite);
                fflush(stdout);
            }
            printf("\n[OK] Temperatura de inicio estabilizada: %.1f C.\n\n", temp_inicio_suite);
        }
    }

    TaskResult results[6];
    double suite_start = bench_get_time_sec();

    for (int i = 0; i < G_NUM_TASKS; i++) {
        const BenchmarkTask* t = &G_TASKS[i];

        /* Aplicar compuerta térmica entre pruebas */
        if (i > 0) {
            apply_thermal_gating(G_TASKS[i - 1].alg, G_TASKS[i - 1].level, &cfg);
        }

        /* Determinar nombre de archivo CSV */
        char csv_path[512];
        if (strcmp(t->alg, "RSA") == 0) {
            snprintf(csv_path, sizeof(csv_path), "%s" PATH_SEP "rsa_%d_%s_%s.csv",
                     cfg.output_dir, t->level, cfg.device, cfg.ejecucion);
        } else {
            snprintf(csv_path, sizeof(csv_path), "%s" PATH_SEP "mlkem_%d_%s_%s.csv",
                     cfg.output_dir, t->level, cfg.device, cfg.ejecucion);
        }

        /* Construir comando de invocación al binario C nativo */
        char cmd[1024];
        char bin_path[256];
        snprintf(bin_path, sizeof(bin_path), "bin" PATH_SEP "%s" EXE_EXT, t->bin_name);

        snprintf(cmd, sizeof(cmd), "%s --runs %d --warmup %d --level %d --device %s --ejecucion %s --output \"%s\"",
                 bin_path, cfg.runs, cfg.warmup, t->level, cfg.device, cfg.ejecucion, csv_path);

        printf("======================================================================\n");
        printf("[EJECUTANDO %d/%d] %s-%d\n", i + 1, G_NUM_TASKS, t->alg, t->level);
        printf(" Comando: %s\n", cmd);
        printf("======================================================================\n");

        double t0 = bench_get_time_sec();
        int ret = system(cmd);
        double t1 = bench_get_time_sec();

        int rows = count_csv_data_rows(csv_path);

        snprintf(results[i].algoritmo, sizeof(results[i].algoritmo), "%s", t->alg);
        results[i].nivel = t->level;
        /* Guardar solo el basename para el resumen */
        const char* slash = strrchr(csv_path, PATH_SEP[0]);
        snprintf(results[i].archivo, sizeof(results[i].archivo), "%s", slash ? (slash + 1) : csv_path);
        results[i].retorno = ret;
        results[i].tiempo_segundos = t1 - t0;
        results[i].filas_registradas = rows;
        results[i].esperadas = cfg.runs;
    }

    double suite_end = bench_get_time_sec();
    double suite_total = suite_end - suite_start;
    double temp_fin_suite = bench_get_temperature_celsius();

    /* Generar archivo JSON de resumen */
    char json_path[512];
    snprintf(json_path, sizeof(json_path), "%s" PATH_SEP "resumen_%s.json", cfg.output_dir, cfg.ejecucion);
    FILE* jfp = fopen(json_path, "w");
    if (jfp) {
        fprintf(jfp, "{\n");
        fprintf(jfp, "  \"dispositivo\": \"%s\",\n", cfg.device);
        fprintf(jfp, "  \"ejecucion\": \"%s\",\n", cfg.ejecucion);
        fprintf(jfp, "  \"modo\": \"%s\",\n", cfg.mode);
        fprintf(jfp, "  \"backend\": \"c\",\n");
        fprintf(jfp, "  \"temperatura_inicial_c\": %.1f,\n", temp_inicio_suite);
        fprintf(jfp, "  \"temperatura_final_c\": %.1f,\n", temp_fin_suite);
        fprintf(jfp, "  \"compuerta_termica_activa\": %s,\n", cfg.no_thermal_gate ? "false" : "true");
        fprintf(jfp, "  \"umbral_termico_objetivo_c\": %.1f,\n", cfg.target_temp);
        fprintf(jfp, "  \"tiempo_total_suite_segundos\": %.2f,\n", suite_total);
        fprintf(jfp, "  \"pruebas\": [\n");
        for (int i = 0; i < G_NUM_TASKS; i++) {
            fprintf(jfp, "    {\n");
            fprintf(jfp, "      \"algoritmo\": \"%s\",\n", results[i].algoritmo);
            fprintf(jfp, "      \"nivel\": %d,\n", results[i].nivel);
            fprintf(jfp, "      \"archivo\": \"%s\",\n", results[i].archivo);
            fprintf(jfp, "      \"retorno\": %d,\n", results[i].retorno);
            fprintf(jfp, "      \"tiempo_segundos\": %.2f,\n", results[i].tiempo_segundos);
            fprintf(jfp, "      \"filas_registradas\": %d,\n", results[i].filas_registradas);
            fprintf(jfp, "      \"esperadas\": %d\n", results[i].esperadas);
            fprintf(jfp, "    }%s\n", (i < G_NUM_TASKS - 1) ? "," : "");
        }
        fprintf(jfp, "  ]\n");
        fprintf(jfp, "}\n");
        fclose(jfp);
    }

    /* Imprimir resumen final en consola */
    printf("\n======================================================================\n");
    printf(" SUITE NATIVA EN C COMPLETADA EXITOSAMENTE\n");
    printf(" Tiempo total de la suite: %.2f segundos\n", suite_total);
    if (temp_inicio_suite > 0 && temp_fin_suite > 0) {
        printf(" Temperatura CPU: Inicial = %.1f C | Final = %.1f C (Delta: %+.1f C)\n",
               temp_inicio_suite, temp_fin_suite, temp_fin_suite - temp_inicio_suite);
    }
    printf(" Resumen guardado en: %s\n", json_path);
    printf("======================================================================\n");
    for (int i = 0; i < G_NUM_TASKS; i++) {
        const char* est = (results[i].filas_registradas == results[i].esperadas && results[i].retorno == 0)
                          ? "OK" : "INCOMPLETO";
        printf(" * %-7s %-4d | %6.2fs | Filas: %d/%d [%s] -> %s\n",
               results[i].algoritmo, results[i].nivel, results[i].tiempo_segundos,
               results[i].filas_registradas, results[i].esperadas, est, results[i].archivo);
    }
    printf("======================================================================\n\n");

    return 0;
}
