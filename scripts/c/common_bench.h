#ifndef COMMON_BENCH_H
#define COMMON_BENCH_H

#if !defined(_WIN32) && !defined(_WIN64)
    #ifndef _GNU_SOURCE
        #define _GNU_SOURCE
    #endif
    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 200809L
    #endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
    #define PLATFORM_WINDOWS 1
    #include <windows.h>
    #include <psapi.h>
    #pragma comment(lib, "psapi.lib")
#else
    #define PLATFORM_POSIX 1
    #include <time.h>
    #include <unistd.h>
    #include <sys/resource.h>
    #include <sys/time.h>
#endif

/* Estructura para almacenar tiempos y métricas de una iteración */
typedef struct {
    double tiempo_keygen_s;
    double tiempo_enc_s;
    double tiempo_dec_s;
    double tiempo_total_s;
    double cpu_percent;
    long memoria_kb;
    size_t tamano_pk;
    size_t tamano_sk;
    size_t tamano_ct;
    size_t tamano_ss;
    const char* estado; /* "OK" o "ERROR" */
} IterationMetric;

/* ============================================================================
 * MEDICIÓN DE TIEMPO DE ALTA RESOLUCIÓN
 * ============================================================================ */

#if defined(PLATFORM_WINDOWS)
static LARGE_INTEGER g_win_freq;
static bool g_win_freq_init = false;

static inline void bench_timer_init(void) {
    if (!g_win_freq_init) {
        QueryPerformanceFrequency(&g_win_freq);
        g_win_freq_init = true;
    }
}

static inline double bench_get_time_sec(void) {
    bench_timer_init();
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)g_win_freq.QuadPart;
}
#else
static inline void bench_timer_init(void) {
    /* POSIX CLOCK_MONOTONIC_RAW no requiere inicialización global */
}

static inline double bench_get_time_sec(void) {
    struct timespec ts;
#if defined(CLOCK_MONOTONIC_RAW)
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
#else
    clock_gettime(CLOCK_MONOTONIC, &ts);
#endif
    return (double)ts.tv_sec + ((double)ts.tv_nsec * 1e-9);
}
#endif

static inline void bench_sleep_ms(long ms) {
#if defined(PLATFORM_WINDOWS)
    Sleep((DWORD)ms);
#else
    struct timespec req;
    req.tv_sec = ms / 1000;
    req.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&req, NULL);
#endif
}

/* ============================================================================
 * MEDICIÓN DE MEMORIA RESIDENTE (RSS en KB)
 * ============================================================================ */

static inline long bench_get_memory_kb(void) {
#if defined(PLATFORM_WINDOWS)
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return (long)(pmc.WorkingSetSize / 1024);
    }
    return 0;
#else
    /* Intentar primero leyendo /proc/self/statm para RSS exacto y dinámico */
    FILE* fp = fopen("/proc/self/statm", "r");
    if (fp) {
        long pages = 0;
        long resident = 0;
        if (fscanf(fp, "%ld %ld", &pages, &resident) == 2) {
            fclose(fp);
            long page_size_kb = sysconf(_SC_PAGESIZE) / 1024;
            return resident * page_size_kb;
        }
        fclose(fp);
    }
    /* Fallback a getrusage */
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        return usage.ru_maxrss; /* En Linux ru_maxrss está en KB */
    }
    return 0;
#endif
}

/* ============================================================================
 * MEDICIÓN DE TIEMPO DE CPU DE PROCESO (User + Kernel)
 * ============================================================================ */

typedef struct {
    double wall_time_s;
    double cpu_time_s;
} ProcessCpuSnapshot;

static inline ProcessCpuSnapshot bench_get_cpu_snapshot(void) {
    ProcessCpuSnapshot snap;
    snap.wall_time_s = bench_get_time_sec();
#if defined(PLATFORM_WINDOWS)
    FILETIME ftCreate, ftExit, ftKernel, ftUser;
    if (GetProcessTimes(GetCurrentProcess(), &ftCreate, &ftExit, &ftKernel, &ftUser)) {
        ULARGE_INTEGER k, u;
        k.LowPart = ftKernel.dwLowDateTime;
        k.HighPart = ftKernel.dwHighDateTime;
        u.LowPart = ftUser.dwLowDateTime;
        u.HighPart = ftUser.dwHighDateTime;
        /* Cada unidad de FILETIME representa 100 nanosegundos (1e-7 s) */
        snap.cpu_time_s = (double)(k.QuadPart + u.QuadPart) * 1e-7;
    } else {
        snap.cpu_time_s = 0.0;
    }
#else
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        double utime = (double)usage.ru_utime.tv_sec + ((double)usage.ru_utime.tv_usec * 1e-6);
        double stime = (double)usage.ru_stime.tv_sec + ((double)usage.ru_stime.tv_usec * 1e-6);
        snap.cpu_time_s = utime + stime;
    } else {
        snap.cpu_time_s = 0.0;
    }
#endif
    return snap;
}

static inline double bench_calculate_cpu_percent(ProcessCpuSnapshot start, ProcessCpuSnapshot end) {
    double wall_delta = end.wall_time_s - start.wall_time_s;
    double cpu_delta = end.cpu_time_s - start.cpu_time_s;
    if (wall_delta <= 0.0) return 0.0;
    double pct = (cpu_delta / wall_delta) * 100.0;
    if (pct < 0.0) pct = 0.0;
    return pct;
}

/* ============================================================================
 * LECTURA DE TEMPERATURA DE CPU (Linux / Raspberry Pi / Android Termux)
 * ============================================================================ */

static inline double bench_get_temperature_celsius(void) {
#if defined(PLATFORM_WINDOWS)
    return -1.0; /* No disponible vía syscall estándar sin elevación WMI */
#else
    const char* thermal_paths[] = {
        "/sys/class/thermal/thermal_zone0/temp",
        "/sys/devices/virtual/thermal/thermal_zone0/temp",
        "/sys/class/thermal/thermal_zone1/temp",
        NULL
    };
    for (int i = 0; thermal_paths[i] != NULL; ++i) {
        FILE* fp = fopen(thermal_paths[i], "r");
        if (fp) {
            long temp_raw = 0;
            if (fscanf(fp, "%ld", &temp_raw) == 1) {
                fclose(fp);
                if (temp_raw > 1000) {
                    return (double)temp_raw / 1000.0;
                }
                return (double)temp_raw;
            }
            fclose(fp);
        }
    }
    return -1.0;
#endif
}

/* ============================================================================
 * MANEJO DE ARCHIVOS CSV
 * ============================================================================ */

static inline void bench_write_csv_header(FILE* fp) {
    fprintf(fp, "ejecucion,tiempo_keygen_s,tiempo_enc_s,tiempo_dec_s,tiempo_total_s,"
                "cpu_percent,memoria_kb,tamano_pk_bytes,tamano_sk_bytes,tamano_ct_bytes,tamano_ss_bytes,"
                "dispositivo,algoritmo,nivel,corrida,estado\n");
    fflush(fp);
}

static inline void bench_write_csv_row(FILE* fp, int ejecucion, const IterationMetric* m,
                                      const char* dispositivo, const char* algoritmo,
                                      const char* nivel, const char* id_ejecucion) {
    fprintf(fp, "%d,%.9f,%.9f,%.9f,%.9f,%.2f,%ld,%zu,%zu,%zu,%zu,%s,%s,%s,%s,%s\n",
            ejecucion,
            m->tiempo_keygen_s,
            m->tiempo_enc_s,
            m->tiempo_dec_s,
            m->tiempo_total_s,
            m->cpu_percent,
            m->memoria_kb,
            m->tamano_pk,
            m->tamano_sk,
            m->tamano_ct,
            m->tamano_ss,
            dispositivo,
            algoritmo,
            nivel,
            id_ejecucion,
            m->estado);
}

#endif /* COMMON_BENCH_H */
