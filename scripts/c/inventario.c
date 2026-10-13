#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>
    #include <direct.h>
    #define mkdir_cross(p) _mkdir(p)
    #define PLATFORM_WIN 1
#else
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <sys/utsname.h>
    #include <unistd.h>
    #define mkdir_cross(p) mkdir(p, 0755)
    #define PLATFORM_POSIX 1
#endif

static void generar_hardware_txt(const char* filepath) {
    FILE* fp = fopen(filepath, "w");
    if (!fp) {
        fprintf(stderr, "[ERROR] No se pudo crear: %s\n", filepath);
        return;
    }

    fprintf(fp, "======================================================================\n");
    fprintf(fp, " UNIVERSIDAD DEL VALLE DE GUATEMALA - TRABAJO DE GRADUACION\n");
    fprintf(fp, " FICHA TECNICA DE HARDWARE Y SISTEMA OPERATIVO (NATIVO C)\n");
    fprintf(fp, "======================================================================\n\n");

#if defined(PLATFORM_WIN)
    SYSTEM_INFO si;
    GetSystemInfo(&si);

    MEMORYSTATUSEX mem;
    mem.dwLength = sizeof(mem);
    GlobalMemoryStatusEx(&mem);

    const char* arch = "Desconocida";
    switch (si.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64: arch = "x64 (AMD64 / Intel 64)"; break;
        case PROCESSOR_ARCHITECTURE_ARM:   arch = "ARM (32-bit)"; break;
        case PROCESSOR_ARCHITECTURE_ARM64: arch = "ARM64 (AArch64)"; break;
        case PROCESSOR_ARCHITECTURE_INTEL: arch = "x86 (32-bit)"; break;
    }

    fprintf(fp, "SISTEMA OPERATIVO:\n");
    fprintf(fp, "  Plataforma: Windows (x64 / NT)\n");

    const char* proc_id = getenv("PROCESSOR_IDENTIFIER");
    fprintf(fp, "\nPROCESADOR:\n");
    fprintf(fp, "  Modelo/Identificador: %s\n", proc_id ? proc_id : "No disponible");
    fprintf(fp, "  Arquitectura: %s\n", arch);
    fprintf(fp, "  Nucleos logicos: %u\n", si.dwNumberOfProcessors);

    fprintf(fp, "\nMEMORIA RAM:\n");
    fprintf(fp, "  Total instalada: %.2f GB (%llu bytes)\n",
            (double)mem.ullTotalPhys / (1024.0 * 1024.0 * 1024.0), mem.ullTotalPhys);
    fprintf(fp, "  Disponible: %.2f GB\n",
            (double)mem.ullAvailPhys / (1024.0 * 1024.0 * 1024.0));

#else
    struct utsname un;
    if (uname(&un) == 0) {
        fprintf(fp, "SISTEMA OPERATIVO:\n");
        fprintf(fp, "  Sistema: %s\n", un.sysname);
        fprintf(fp, "  Release de Kernel: %s\n", un.release);
        fprintf(fp, "  Version de Kernel: %s\n", un.version);
        fprintf(fp, "  Arquitectura: %s\n", un.machine);
        fprintf(fp, "  Hostname: %s\n", un.nodename);
    }

    /* Leer modelo de CPU desde /proc/cpuinfo */
    fprintf(fp, "\nPROCESADOR:\n");
    FILE* cpuinfo = fopen("/proc/cpuinfo", "r");
    if (cpuinfo) {
        char line[256];
        int cores = 0;
        char model[256] = "ARM / Linux SoC";
        char hardware[256] = "";
        while (fgets(line, sizeof(line), cpuinfo)) {
            if (strncmp(line, "model name", 10) == 0 || strncmp(line, "Model", 5) == 0) {
                char* colon = strchr(line, ':');
                if (colon) {
                    snprintf(model, sizeof(model), "%s", colon + 2);
                    /* Quitar salto de línea */
                    model[strcspn(model, "\r\n")] = 0;
                }
            } else if (strncmp(line, "Hardware", 8) == 0) {
                char* colon = strchr(line, ':');
                if (colon) {
                    snprintf(hardware, sizeof(hardware), "%s", colon + 2);
                    hardware[strcspn(hardware, "\r\n")] = 0;
                }
            } else if (strncmp(line, "processor", 9) == 0) {
                cores++;
            }
        }
        fclose(cpuinfo);
        fprintf(fp, "  Modelo: %s\n", model);
        if (hardware[0] != '\0') {
            fprintf(fp, "  SoC/Hardware: %s\n", hardware);
        }
        fprintf(fp, "  Nucleos detectados: %d\n", cores > 0 ? cores : (int)sysconf(_SC_NPROCESSORS_ONLN));
    }

    /* Leer memoria desde /proc/meminfo */
    fprintf(fp, "\nMEMORIA RAM:\n");
    FILE* meminfo = fopen("/proc/meminfo", "r");
    if (meminfo) {
        char line[256];
        long mem_total_kb = 0;
        long mem_avail_kb = 0;
        while (fgets(line, sizeof(line), meminfo)) {
            if (strncmp(line, "MemTotal:", 9) == 0) {
                sscanf(line + 9, "%ld", &mem_total_kb);
            } else if (strncmp(line, "MemAvailable:", 13) == 0) {
                sscanf(line + 13, "%ld", &mem_avail_kb);
            }
        }
        fclose(meminfo);
        fprintf(fp, "  Total instalada: %.2f GB (%ld KB)\n", (double)mem_total_kb / (1024.0 * 1024.0), mem_total_kb);
        fprintf(fp, "  Disponible: %.2f GB (%ld KB)\n", (double)mem_avail_kb / (1024.0 * 1024.0), mem_avail_kb);
    }
#endif

    fprintf(fp, "\n======================================================================\n");
    fclose(fp);
}

static void generar_versiones_txt(const char* filepath) {
    FILE* fp = fopen(filepath, "w");
    if (!fp) {
        fprintf(stderr, "[ERROR] No se pudo crear: %s\n", filepath);
        return;
    }

    fprintf(fp, "======================================================================\n");
    fprintf(fp, " UNIVERSIDAD DEL VALLE DE GUATEMALA - TRABAJO DE GRADUACION\n");
    fprintf(fp, " FICHA TECNICA DE VERSIONES DE SOFTWARE Y BIBLIOTECAS (C NATIVO)\n");
    fprintf(fp, "======================================================================\n\n");

    fprintf(fp, "COMPILADOR C:\n");
#if defined(__clang__)
    fprintf(fp, "  Compilador: Clang / LLVM\n");
    fprintf(fp, "  Version: %s\n", __clang_version__);
#elif defined(__GNUC__)
    fprintf(fp, "  Compilador: GNU GCC\n");
    fprintf(fp, "  Version: %s\n", __VERSION__);
#elif defined(_MSC_VER)
    fprintf(fp, "  Compilador: Microsoft Visual C++ (MSVC)\n");
    fprintf(fp, "  Version interna (_MSC_VER): %d\n", _MSC_VER);
#else
    fprintf(fp, "  Compilador: Desconocido (C11 compatible)\n");
#endif

    fprintf(fp, "\nBIBLIOTECA CRIPTOGRAFICA CLASICA (RSA):\n");
    fprintf(fp, "  Biblioteca: OpenSSL\n");
#if defined(PLATFORM_WIN)
    HMODULE hOssl = LoadLibraryA("bin\\libcrypto-4-x64.dll");
    if (!hOssl) hOssl = LoadLibraryA("libcrypto-4-x64.dll");
    if (!hOssl) hOssl = LoadLibraryA("libcrypto-3-x64.dll");
    if (!hOssl) hOssl = LoadLibraryA("libcrypto-1_1-x64.dll");
    if (hOssl) {
        typedef const char* (*fn_ossl_version)(int);
        fn_ossl_version p_version = (fn_ossl_version)GetProcAddress(hOssl, "OpenSSL_version");
        if (p_version) {
            fprintf(fp, "  Version: %s\n", p_version(0));
        } else {
            fprintf(fp, "  Version: OpenSSL (DLL cargada)\n");
        }
        FreeLibrary(hOssl);
    } else {
        fprintf(fp, "  Version: OpenSSL 3.x / 4.x (binario independiente)\n");
    }
#else
    FILE* cmd_fp = popen("openssl version 2>/dev/null", "r");
    if (cmd_fp) {
        char ossl_buf[128] = {0};
        if (fgets(ossl_buf, sizeof(ossl_buf) - 1, cmd_fp)) {
            char* nl = strchr(ossl_buf, '\n');
            if (nl) *nl = '\0';
            fprintf(fp, "  Version: %s\n", ossl_buf);
        } else {
            fprintf(fp, "  Version: OpenSSL nativo POSIX\n");
        }
        pclose(cmd_fp);
    } else {
        fprintf(fp, "  Version: OpenSSL nativo POSIX\n");
    }
#endif

    fprintf(fp, "\nBIBLIOTECA CRIPTOGRAFICA POST-CUANTICA (ML-KEM / KYBER):\n");
    fprintf(fp, "  Biblioteca: Open Quantum Safe (liboqs)\n");
    fprintf(fp, "  Estandar NIST: FIPS 203 (ML-KEM-512, ML-KEM-768, ML-KEM-1024)\n");

    fprintf(fp, "\n======================================================================\n");
    fclose(fp);
}

int main(void) {
    mkdir_cross("entorno");

#if defined(PLATFORM_WIN)
    const char* hw_path = "entorno\\hardware.txt";
    const char* ver_path = "entorno\\versiones.txt";
#else
    const char* hw_path = "entorno/hardware.txt";
    const char* ver_path = "entorno/versiones.txt";
#endif

    generar_hardware_txt(hw_path);
    generar_versiones_txt(ver_path);

    printf("[EXITO] Fichas tecnicas generadas correctamente en C nativo:\n");
    printf(" -> %s\n", hw_path);
    printf(" -> %s\n", ver_path);

    return 0;
}
