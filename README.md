# Manual Experimental: Evaluación de Desempeño Criptográfico (RSA vs. ML-KEM / Kyber)

**Universidad del Valle de Guatemala — Facultad de Ingeniería**  
**Trabajo de Graduación:** *Análisis comparativo del desempeño de algoritmos criptográficos clásicos y post-cuánticos en el intercambio de claves en dispositivos de uso común.*  
**Autor:** Elián Israel Rosales Barrios (2026)

---

## 1. Introducción y Fundamentos Metodológicos

Este repositorio contiene la suite experimental completa para medir, registrar y analizar el desempeño computacional y sobrecosto de comunicación entre la criptografía asimétrica clásica (**RSA**) y la criptografía post-cuántica basada en retículas (**ML-KEM / CRYSTALS-Kyber**) en cuatro dispositivos de uso común:
1. Computadora de escritorio (Windows x64).
2. Computadora portátil (Windows / Linux x64).
3. Raspberry Pi (Raspberry Pi OS - ARM).
4. Teléfono inteligente Android (mediante Termux - ARM64).

### Niveles de Seguridad Evaluados (NIST SP 800-57 & FIPS 203)

| Nivel de Seguridad NIST | Seguridad Clásica Equivalente | Algoritmo Clásico (RSA) | Algoritmo Post-Cuántica (KEM) | Tamaño Secreto Compartido |
| :---: | :---: | :---: | :---: | :---: |
| **Nivel 1** | AES-128 (112-128 bits) | RSA-2048 ($e=65537$) | ML-KEM-512 (Kyber512) | 32 bytes (256 bits) |
| **Nivel 3** | AES-192 (192 bits) | RSA-3072 ($e=65537$) | ML-KEM-768 (Kyber768) | 32 bytes (256 bits) |
| **Nivel 5** | AES-256 (256 bits) | RSA-4096 ($e=65537$) | ML-KEM-1024 (Kyber1024) | 32 bytes (256 bits) |

### Estándar Criptográfico y Operaciones Evaluadas
Siguiendo las directrices del **NIST PQC**, **eBACS/SUPERCOP** y RFCs de **IETF/TLS**, cada ciclo de intercambio de claves mide tres operaciones atómicas:
1. **KeyGen**: Generación de claves pública ($\text{pk}$) y privada ($\text{sk}$).
2. **Encaps / Encrypt**: 
   * En ML-KEM: Encapsulación de clave generando el secreto compartido y el texto cifrado ($ct$).
   * En RSA: Transporte de clave simétrica aleatoria de 32 bytes mediante RSA-OAEP con función hash SHA-256 y máscara MGF1-SHA256.
3. **Decaps / Decrypt**:
   * En ML-KEM: Decapsulación usando la clave secreta y el texto cifrado.
   * En RSA: Descifrado RSA-OAEP con la clave privada.
4. **Verificación**: Validación bit a bit de que el secreto recuperado es idéntico al original ($A == B$). Un error invalida la muestra.

---

## 2. Métricas Registradas y Estructura del CSV

Cada iteración genera una fila en el archivo CSV con las siguientes 16 columnas estandarizadas:

```csv
ejecucion,tiempo_keygen_s,tiempo_enc_s,tiempo_dec_s,tiempo_total_s,cpu_percent,memoria_kb,tamano_pk_bytes,tamano_sk_bytes,tamano_ct_bytes,tamano_ss_bytes,dispositivo,algoritmo,nivel,corrida,estado
```

* **`tiempo_keygen_s`**, **`tiempo_enc_s`**, **`tiempo_dec_s`**, **`tiempo_total_s`**: Medidos con temporizadores de precisión sub-microsegundo (`QueryPerformanceCounter` en Windows, `clock_gettime(CLOCK_MONOTONIC_RAW)` en POSIX, `time.perf_counter()` en Python).
* **`cpu_percent`**: Porcentaje acumulado de CPU en intervalos por bloque para mitigar el ruido de iteraciones en microsegundos.
* **`memoria_kb`**: Memoria física residente (RSS en KB) del proceso.
* **`tamano_pk_bytes`**, **`tamano_sk_bytes`**, **`tamano_ct_bytes`**, **`tamano_ss_bytes`**: Tamaños de las estructuras en el canal y en almacenamiento.
* **`estado`**: `"OK"` si la verificación criptográfica fue exitosa; código de error en caso contrario.

Adicionalmente, el orquestador y los scripts registran la **temperatura de la CPU** al inicio y al final de la serie de ejecuciones en plataformas Linux/Android para monitorear posible *thermal throttling*.

---

## 3. Estructura de Carpetas

```
benchmark_criptografia/
├── scripts/
│   ├── c/
│   │   ├── common_bench.h         # Utilidades comunes multiplataforma en C
│   │   ├── rsa_benchmark.c        # Benchmark RSA nativo C (OpenSSL EVP)
│   │   ├── kyber_benchmark.c      # Benchmark Kyber nativo C (liboqs)
│   │   ├── orquestador.c          # Orquestador nativo C con compuerta térmica automatizada
│   │   └── inventario.c           # Generador nativo C de fichas técnicas de hardware y versiones
│   └── analisis/
│       └── graficar_tesis.py      # Análisis estadístico y generador de tablas LaTeX / figuras para la tesis
├── bin/                           # Binarios ejecutables C compilados (.exe o ELF)
├── resultados/
│   ├── piloto/                    # Pruebas piloto (200 ejecuciones)
│   ├── ejecucion_01/              # Ejecución formal 1 (5,000 ejecuciones)
│   ├── ejecucion_02/              # Ejecución formal 2 de replicación (5,000 ejecuciones)
│   └── analisis_tesis/            # Tablas LaTeX y Markdown consolidadas
├── entorno/
│   ├── hardware.txt               # Ficha de características del equipo (C nativo)
│   └── versiones.txt              # Ficha de versiones de bibliotecas y compiladores (C nativo)
├── CMakeLists.txt                 # Configuración de compilación con CMake (RSA, Kyber, Orquestador, Inventario)
├── Makefile                       # Makefile para Linux y Termux (compila suite C completa)
├── run_windows_oneclick.bat       # Lanzador One-Click C nativo para Windows (estabiliza, compila y ejecuta)
├── run_linux_oneclick.sh          # Lanzador One-Click C nativo para Raspberry Pi y Linux
├── run_android_oneclick.sh        # Lanzador One-Click C nativo para Android Termux (wake-lock + make)
└── README.md                      # Este manual
```

---

## 4. Preparación de Dispositivos: Guía Paso a Paso

---

### Dispositivo 1 & 2: Windows x64 (PC de Escritorio / Laptop)

#### Requisitos Previos
* **Sistema Operativo:** Windows 10 o Windows 11 (64 bits).
* **Visual Studio:** Visual Studio Community 2022 con la carga de trabajo *"Desarrollo para el escritorio con C++"*.
* **Python:** Python 3.10+ (64 bits) instalado con la casilla *"Add Python to PATH"* activada.
* **CMake & Git:** Git para Windows y CMake instalados en el PATH.

#### Paso W1. Instalar OpenSSL para Windows x64
1. Descargue el instalador precompilado de OpenSSL v3.x para Windows x64 (por ejemplo, desde [FireDaemon OpenSSL](https://firedaemon.com/firedaemon-openssl) o [Slproweb OpenSSL Win64](https://slproweb.com/products/Win32OpenSSL.html) versión completa de 64 bits).
2. Instale en `C:\Program Files\OpenSSL-Win64` o `C:\OpenSSL-Win64`.
3. Verifique en PowerShell:
   ```powershell
   openssl version
   ```

#### Paso W2. Clonar y compilar `liboqs` en Windows x64
Abra **x64 Native Tools Command Prompt for VS 2022** (es fundamental que sea la consola nativa x64, no x86):

```cmd
cd C:\Users\TuUsuario\Documents
git clone https://github.com/open-quantum-safe/liboqs.git
cd liboqs
mkdir build
cd build

cmake -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release -DOQS_USE_OPENSSL=ON -DOPENSSL_ROOT_DIR="C:\Program Files\OpenSSL-Win64" ..
cmake --build . --config Release
```
*Verificación:* Confirme que se generó `C:\Users\TuUsuario\Documents\liboqs\build\bin\Release\oqs.dll` y `oqs.lib`.

#### Paso W3. Compilar los benchmarks nativos en C (Windows)
En la misma consola x64 Native Tools, diríjase a `benchmark_criptografia`:

```cmd
cd C:\ruta\al\proyecto\benchmark_criptografia
mkdir bin

:: Compilar RSA C (OpenSSL):
cl /O2 /MD scripts\c\rsa_benchmark.c /I"C:\Program Files\OpenSSL-Win64\include" /Iscripts\c "C:\Program Files\OpenSSL-Win64\lib\VC\x64\MD\libcrypto.lib" /Fe:bin\rsa_benchmark.exe /link psapi.lib

:: Compilar Kyber C (liboqs):
cl /O2 /MD scripts\c\kyber_benchmark.c /I"C:\Users\TuUsuario\Documents\liboqs\include" /I"C:\Users\TuUsuario\Documents\liboqs\build\include" /Iscripts\c "C:\Users\TuUsuario\Documents\liboqs\build\lib\Release\oqs.lib" "C:\Program Files\OpenSSL-Win64\lib\VC\x64\MD\libcrypto.lib" /Fe:bin\kyber_benchmark.exe /link psapi.lib

:: Copiar oqs.dll a la carpeta bin para que el ejecutable la encuentre:
copy "C:\Users\TuUsuario\Documents\liboqs\build\bin\Release\oqs.dll" bin\
```

#### Paso W4. Preparar entorno Python en Windows
En PowerShell o CMD:
```powershell
pip install cryptography psutil
```

---

### Dispositivo 3: Raspberry Pi (Raspberry Pi OS)

#### Paso R1. Actualizar sistema e instalar paquetes base
Abra una terminal en la Raspberry Pi:
```bash
sudo apt update && sudo apt upgrade -y
sudo apt install -y python3-full python3-venv git cmake build-essential libssl-dev
```

#### Paso R2. Preparar entorno virtual de Python (PEP 668)
Para no interferir con los paquetes del sistema operativo:
```bash
cd ~/benchmark_criptografia
python3 -m venv rsa_env
source rsa_env/bin/activate
pip install --upgrade pip
pip install cryptography psutil
```

#### Paso R3. Clonar y compilar `liboqs` en Raspberry Pi
```bash
cd ~
git clone --depth 1 https://github.com/open-quantum-safe/liboqs.git
cd liboqs
mkdir build && cd build
cmake -DCMAKE_INSTALL_PREFIX=/usr/local -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
sudo make install
sudo ldconfig
```

#### Paso R4. Compilar los ejecutables en C
Regrese a la carpeta del benchmark:
```bash
cd ~/benchmark_criptografia
make
```
*Verificación:* Se crearán los binarios `bin/rsa_benchmark` y `bin/kyber_benchmark`. Pruebe la ejecución de ayuda:
```bash
./bin/rsa_benchmark --help
./bin/kyber_benchmark --help
```

---

### Dispositivo 4: Teléfono Android (vía Termux)

#### Paso T1. Instalación y configuración de Termux
1. Instale Termux desde **F-Droid** (no use Google Play Store, ya que está desactualizada).
2. Abra Termux y configure los repositorios y almacenamiento:
   ```bash
   termux-change-repo
   # Seleccione "Main repository" y elija un mirror confiable (ej. mirrors.tuna.tsinghua.edu.cn o grimler.se)
   pkg update && pkg upgrade -y
   termux-setup-storage
   ```

#### Paso T2. Instalar herramientas de compilación y Python
```bash
pkg install -y python git cmake clang make openssl
pip install --upgrade pip
pip install cryptography psutil
```
*(Nota: Si `pip install psutil` solicita cabeceras, ejecute `pkg install -y python-dev libffi-dev`)*.

#### Paso T3. Compilar `liboqs` en Termux
```bash
cd ~
git clone --depth 1 https://github.com/open-quantum-safe/liboqs.git
cd liboqs
mkdir build && cd build
cmake -DCMAKE_INSTALL_PREFIX=$PREFIX -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
make install
```

#### Paso T4. Compilar los benchmarks en Termux
```bash
cd ~/benchmark_criptografia
make
```
Si prefiere compilar manualmente con clang:
```bash
mkdir -p bin
clang -O3 scripts/c/rsa_benchmark.c -I$PREFIX/include -Iscripts/c -L$PREFIX/lib -lssl -lcrypto -lm -o bin/rsa_benchmark
clang -O3 scripts/c/kyber_benchmark.c -I$PREFIX/include -Iscripts/c -L$PREFIX/lib -loqs -lssl -lcrypto -lm -o bin/kyber_benchmark
```

---

## 5. Protocolo de Ejecución del Experimento

### Reglas Críticas de Estabilización Previas a la Ejecución
1. **Alimentación Eléctrica:** Conecte el dispositivo a su cargador de corriente (evite operar solo con batería para evitar perfiles de ahorro de energía agresivos).
2. **Cierre de Procesos en Segundo Plano:** Cierre navegadores, reproductores de video, descargas de red y aplicaciones no esenciales.
3. **Condición Térmica:** Deje reposar el dispositivo 5 minutos antes de iniciar para que la temperatura de CPU se normalice a temperatura ambiente. En Raspberry Pi, asegúrese de contar con disipador pasivo o ventilador si está disponible.

---

### Fase 1: Generar Fichas de Entorno y Hardware
Ejecute el binario de inventario C nativo en el dispositivo:
```bash
# En Linux / Raspberry Pi / Android:
./bin/inventario

# En Windows:
bin\inventario.exe
```
Esto generará los archivos `entorno/hardware.txt` y `entorno/versiones.txt` con los metadatos exactos de la plataforma.

---

### Fase 2: Prueba Piloto (200 repeticiones)
La prueba piloto verifica la estabilidad, la validez criptográfica y la correcta estructura de los CSV sin contaminar los datos formales.

```bash
# En Linux / Raspberry Pi / Android:
./bin/orquestador --mode pilot --device mi_dispositivo --ejecucion piloto

# En Windows:
bin\orquestador.exe --mode pilot --device mi_dispositivo --ejecucion piloto
```

*Verificación de la prueba piloto:*
1. Revise los archivos en `resultados/piloto/`.
2. Confirme que cada CSV tenga exactamente 200 filas de datos (más la cabecera).
3. Verifique que la columna `estado` tenga valor `"OK"` en el 100% de las filas.

---

### Fase 3: Ejecución Formal 01 (5,000 repeticiones)
Una vez aprobado el piloto, los binarios y parámetros quedan estrictamente **congelados**.

```bash
# En Linux / Raspberry Pi / Android:
./bin/orquestador --mode full --device mi_dispositivo --ejecucion ejecucion_01

# En Windows:
bin\orquestador.exe --mode full --device mi_dispositivo --ejecucion ejecucion_01
```

Al finalizar, verifique que se hayan creado los 6 archivos CSV en `resultados/ejecucion_01/`:
* `rsa_2048_mi_dispositivo_ejecucion_01.csv`
* `rsa_3072_mi_dispositivo_ejecucion_01.csv`
* `rsa_4096_mi_dispositivo_ejecucion_01.csv`
* `mlkem_512_mi_dispositivo_ejecucion_01.csv`
* `mlkem_768_mi_dispositivo_ejecucion_01.csv`
* `mlkem_1024_mi_dispositivo_ejecucion_01.csv`
* `resumen_ejecucion_01.json`

---

### Fase 4: Ejecución Formal 02 de Replicación (5,000 repeticiones)
Repita el procedimiento tras un período de enfriamiento térmico para evaluar la replicabilidad estadística:
```bash
# En Linux / Raspberry Pi / Android:
./bin/orquestador --mode full --device mi_dispositivo --ejecucion ejecucion_02

# En Windows:
bin\orquestador.exe --mode full --device mi_dispositivo --ejecucion ejecucion_02
```

---

### Fase 5: Consolidación Estadística y Tablas para la Tesis
En la computadora de análisis, ejecute el script de post-procesamiento:
```bash
python scripts/analisis/graficar_tesis.py
```
Este script genera automáticamente:
* `resultados/analisis_tesis/tabla_resumen_tesis.md` (Markdown con medias, desviaciones estándar, percentiles P95 y tamaños de claves).
* `resultados/analisis_tesis/tabla_resumen_tesis.tex` (Código LaTeX listo para insertar en la tesis con paquetes `booktabs` y `siunitx`).
* Figuras comparativas de alta resolución (PNG a 300 DPI) para tiempos de operación y sobrecarga en memoria.

---

## 6. Ejecución Manual Individual de Scripts

Si desea ejecutar un algoritmo específico de forma aislada:

### Ejecución en Python:
```bash
# RSA-2048 con 5,000 ejecuciones:
python scripts/python/rsa_benchmark.py --runs 5000 --warmup 20 --level 2048 --device raspberry_pi --ejecucion ejecucion_01 --output resultados/ejecucion_01/rsa_2048.csv

# Kyber-512 (ML-KEM-512) con 5,000 ejecuciones:
python scripts/python/kyber_benchmark.py --runs 5000 --warmup 20 --level 512 --device raspberry_pi --ejecucion ejecucion_01 --output resultados/ejecucion_01/mlkem_512.csv
```

### Ejecución en C Nativo:
```bash
# RSA-2048:
./bin/rsa_benchmark --runs 5000 --warmup 20 --level 2048 --device windows_pc --ejecucion ejecucion_01 --output resultados/ejecucion_01/rsa_2048_c.csv

# Kyber-512:
./bin/kyber_benchmark --runs 5000 --warmup 20 --level 512 --device windows_pc --ejecucion ejecucion_01 --output resultados/ejecucion_01/mlkem_512_c.csv
```

---

## 7. Solución de Problemas Frecuentes

1. **Error LNK4272 (incompatibilidad de arquitectura x86 vs x64 en Windows):**
   * *Causa:* Se compiló usando el *Developer Command Prompt* estándar de 32 bits en lugar del nativo de 64 bits.
   * *Solución:* Abra exclusivamente **x64 Native Tools Command Prompt for VS 2022** o ejecute `"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"`. Ejecute `cl` y confirme que indique *"for x64"*.

2. **Error `PermissionError: [Errno 13] Permission denied: '/proc/stat'` en Termux (Android):**
   * *Causa:* Android 8.0+ bloquea el acceso de aplicaciones sin root a `/proc/stat`.
   * *Solución:* Los scripts implementados en esta suite no dependen de `/proc/stat`; utilizan `resource.getrusage` y tiempos de proceso propios que están plenamente autorizados en Android.

3. **Error PEP 668 *"externally-managed-environment"* en Raspberry Pi OS:**
   * *Causa:* Las versiones recientes de Debian/Raspberry Pi OS bloquean `pip install` global.
   * *Solución:* Use siempre el entorno virtual creado con `python3 -m venv rsa_env` y actívelo con `source rsa_env/bin/activate`.

4. **Advertencia de liboqs: *"Toolchain is not Clang/GCC"*:*
   * *Causa:* Aparece al compilar en Windows con MSVC.
   * *Impacto:* Es solo una advertencia informativa de la biblioteca sobre optimizaciones específicas de vectorización, no un error. La compilación genera el binario funcionalmente correcto.