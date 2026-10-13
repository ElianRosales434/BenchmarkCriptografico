set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Búsqueda inteligente de la raíz del proyecto
PROJECT_ROOT=""

if [ -f "$SCRIPT_DIR/scripts/c/orquestador.c" ]; then
    PROJECT_ROOT="$SCRIPT_DIR"
elif [ -f "$SCRIPT_DIR/benchmark_criptografia/scripts/c/orquestador.c" ]; then
    PROJECT_ROOT="$SCRIPT_DIR/benchmark_criptografia"
elif [ -f "$(dirname "$SCRIPT_DIR")/scripts/c/orquestador.c" ]; then
    PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
elif [ -f "$(dirname "$SCRIPT_DIR")/benchmark_criptografia/scripts/c/orquestador.c" ]; then
    PROJECT_ROOT="$(dirname "$SCRIPT_DIR")/benchmark_criptografia"
else
    # Buscar si existe en subdirectorios de Downloads (ej. si se descomprimió un zip)
    CANDIDATE=$(find "$SCRIPT_DIR" -maxdepth 3 -type f -name "orquestador.c" 2>/dev/null | head -n 1 || true)
    if [ -n "$CANDIDATE" ]; then
        PROJECT_ROOT="$(cd "$(dirname "$CANDIDATE")/../.." && pwd)"
    else
        # Buscar en ubicaciones habituales de usuario
        for cdir in "$HOME/benchmark_criptografia" "$HOME/bold-salk/benchmark_criptografia" "$HOME/bold-salk"; do
            if [ -f "$cdir/scripts/c/orquestador.c" ]; then
                PROJECT_ROOT="$cdir"
                break
            fi
        done
    fi
fi

if [ -z "$PROJECT_ROOT" ] || [ ! -f "$PROJECT_ROOT/scripts/c/orquestador.c" ]; then
    echo "======================================================================"
    echo " [ERROR] No se encontraron los archivos del proyecto criptografico"
    echo "======================================================================"
    echo " El script 'run_linux_oneclick.sh' se ejecuto en:"
    echo "   $SCRIPT_DIR"
    echo " pero no se localizo 'scripts/c/orquestador.c'."
    echo ""
    echo " Causa:"
    echo "   Se copio o descargo unicamente este script '.sh' de forma aislada,"
    echo "   sin transferir la carpeta completa 'benchmark_criptografia/' o 'scripts/'."
    echo ""
    echo " Solucion:"
    echo "   1. Si tiene la carpeta completa descargada, ingrese a ella:"
    echo "      cd ~/Downloads/nombre_carpeta/benchmark_criptografia"
    echo "      ./run_linux_oneclick.sh"
    echo ""
    echo "   2. O copie la carpeta completa desde su PC a la Raspberry Pi:"
    echo "      scp -r benchmark_criptografia pi@<IP_RASPBERRY>:~/"
    echo "      cd ~/benchmark_criptografia && ./run_linux_oneclick.sh"
    echo "======================================================================"
    exit 1
fi

cd "$PROJECT_ROOT"

echo "======================================================================"
echo "   SUITE DE BENCHMARKING CRIPTOGRAFICO (RSA vs. ML-KEM / Kyber)"
echo "   Plataforma: Linux / Raspberry Pi OS"
echo "   Directorio de trabajo: $PROJECT_ROOT"
echo "======================================================================"
echo ""

# 1. Verificar herramientas base del sistema
echo "[1/5] Verificando dependencias del sistema..."
if command -v apt-get &> /dev/null; then
    MISSING_PKGS=""
    for pkg in git cmake build-essential libssl-dev; do
        if ! dpkg -s "$pkg" &> /dev/null; then
            MISSING_PKGS="$MISSING_PKGS $pkg"
        fi
    done
    if [ -n "$MISSING_PKGS" ]; then
        echo "Instalando paquetes requeridos:$MISSING_PKGS..."
        sudo apt-get update -qq
        sudo apt-get install -y -qq $MISSING_PKGS
    fi
fi

# 2. Verificar liboqs y compilar suite nativa en C
echo "[2/5] Verificando biblioteca liboqs y compilando suite C nativa..."

# Verificar e instalar liboqs (Open Quantum Safe) si no existe en el sistema
HAS_LIBOQS=false
if ldconfig -p 2>/dev/null | grep -q "liboqs"; then
    HAS_LIBOQS=true
elif [ -f "/usr/local/lib/liboqs.so" ] || [ -f "/usr/lib/liboqs.so" ] || [ -f "$HOME/liboqs/build/lib/liboqs.so" ]; then
    HAS_LIBOQS=true
elif ls /usr/local/lib/liboqs.so* 1> /dev/null 2>&1; then
    HAS_LIBOQS=true
fi

if [ "$HAS_LIBOQS" = false ]; then
    echo ""
    echo "======================================================================"
    echo " [AUTO-CONFIGURACION] Compilando e instalando liboqs (Open Quantum Safe)..."
    echo " Requerido para la evaluacion de ML-KEM / Kyber (~2-3 minutos en Raspberry Pi)"
    echo "======================================================================"
    CURRENT_PWD="$(pwd)"
    cd "$HOME"
    if [ ! -d "liboqs" ]; then
        echo "Clonando repositorio oficial de liboqs..."
        git clone --depth 1 https://github.com/open-quantum-safe/liboqs.git
    fi
    cd liboqs
    mkdir -p build && cd build
    echo "Configurando liboqs con CMake (Release, bibliotecas compartidas)..."
    cmake -DCMAKE_INSTALL_PREFIX=/usr/local -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Release ..
    echo "Compilando en paralelo con $(nproc) núcleos..."
    make -j$(nproc)
    echo "Instalando en /usr/local/lib..."
    sudo make install
    if [ -d "/etc/ld.so.conf.d" ]; then
        echo "/usr/local/lib" | sudo tee /etc/ld.so.conf.d/liboqs.conf > /dev/null 2>&1 || true
    fi
    sudo ldconfig
    cd "$CURRENT_PWD"
    echo "[OK] liboqs compilado e instalado exitosamente en el sistema."
else
    echo "    Biblioteca post-cuantica liboqs detectada en el sistema [OK]."
fi

export LD_LIBRARY_PATH="/usr/local/lib:${LD_LIBRARY_PATH:-}"

# Precompilar automáticamente los binarios de benchmark en C si no existen
if [ ! -f "bin/rsa_benchmark" ] || [ ! -f "bin/kyber_benchmark" ]; then
    echo ""
    echo "==> Compilando binarios de benchmark en C (RSA y Kyber)..."
    make
fi

# 3. Estabilización de CPU y cierre de procesos no esenciales
echo ""
echo "======================================================================"
echo "[3/5] ESTABILIZACION DEL ENTORNO DE PRUEBA (Linux / Raspberry Pi)"
echo "======================================================================"

# Cerrar aplicaciones pesadas de escritorio si estan activas
echo "Cerrando navegadores y reproductores multimedia..."
pkill -f chromium-browser 2>/dev/null || true
pkill -f firefox 2>/dev/null || true
pkill -f vlc 2>/dev/null || true

# Configurar gobernador de CPU a 'performance' si hay permisos sudo
RESTORE_GOVERNOR=""
if [ -d "/sys/devices/system/cpu/cpu0/cpufreq" ]; then
    CURRENT_GOV=$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null || echo "")
    if [ "$CURRENT_GOV" != "performance" ] && [ "$EUID" -eq 0 -o -n "$(command -v sudo)" ]; then
        echo "Ajustando gobernador de frecuencia de CPU a 'performance'..."
        echo "performance" | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor > /dev/null 2>&1 || true
        RESTORE_GOVERNOR="$CURRENT_GOV"
    fi
fi

# Funcion para restaurar gobernador al salir
cleanup() {
    if [ -n "$RESTORE_GOVERNOR" ]; then
        echo ""
        echo "Restaurando gobernador de CPU previo ($RESTORE_GOVERNOR)..."
        echo "$RESTORE_GOVERNOR" | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor > /dev/null 2>&1 || true
    fi
}
trap cleanup EXIT

# Verificar temperatura actual
if [ -f "/sys/class/thermal/thermal_zone0/temp" ]; then
    RAW_TEMP=$(cat /sys/class/thermal/thermal_zone0/temp)
    TEMP_C=$(echo "scale=1; $RAW_TEMP / 1000" | bc 2>/dev/null || echo "$((RAW_TEMP / 1000))")
    echo "    Temperatura actual de CPU: ${TEMP_C}°C"
    if [ "${RAW_TEMP%???}" -gt 65 ]; then
        echo "    [ALERTA] La CPU esta por encima de 65°C. Se recomienda esperar a que se enfrie."
    fi
fi

# 4. Generar ficha de inventario
echo ""
echo "[4/5] Registrando inventario del dispositivo en C nativo..."
./bin/inventario

# 5. Menu interactivo
echo ""
echo "======================================================================"
echo "[5/5] CONFIGURACION DE LA SERIE DE EJECUCIONES (SUITE C NATIVA)"
echo "======================================================================"
echo "Seleccione el modo que desea ejecutar:"
echo "  [1] Prueba Piloto (200 repeticiones por nivel - Recomendado)"
echo "  [2] Serie Formal Completa (ejecucion_01 y ejecucion_02 - 5,000 rep.)"
echo "  [3] Solo Ejecucion Formal 01 (5,000 repeticiones)"
echo "  [4] Solo Ejecucion Formal 02 de Replicacion (5,000 repeticiones)"
echo ""
read -p "Ingrese opcion [1-4] (defecto 1): " MODO_ELEGIDO
MODO_ELEGIDO=${MODO_ELEGIDO:-1}

DEFAULT_DEV="raspberry_pi"
if ! grep -qi "raspberry" /proc/cpuinfo 2>/dev/null; then
    DEFAULT_DEV="linux_pc"
fi

read -p "Identificador de este equipo [defecto $DEFAULT_DEV]: " DISPOSITIVO_ID
DISPOSITIVO_ID=${DISPOSITIVO_ID:-$DEFAULT_DEV}

NICE_PREFIX=""
if [ "$EUID" -eq 0 ]; then
    NICE_PREFIX="nice -n -20"
fi

echo ""
echo "Pausa termica de 5 segundos previo al inicio..."
sleep 5

case $MODO_ELEGIDO in
    1)
        echo "Iniciando Prueba Piloto (200 ejecuciones por nivel)..."
        $NICE_PREFIX ./bin/orquestador --mode pilot --device "$DISPOSITIVO_ID" --ejecucion piloto
        ;;
    2)
        echo "Iniciando Serie Formal Completa: Ejecucion 01 y 02 (5,000 ejecuciones)..."
        $NICE_PREFIX ./bin/orquestador --mode full --device "$DISPOSITIVO_ID" --ejecucion ejecucion_01
        
        echo ""
        echo "=================================================================="
        echo "Pausa de enfriamiento termico de 60 segundos antes de replicar..."
        echo "=================================================================="
        sleep 60
        
        echo "Iniciando Ejecucion Formal 02 de Replicacion (5,000 ejecuciones)..."
        $NICE_PREFIX ./bin/orquestador --mode full --device "$DISPOSITIVO_ID" --ejecucion ejecucion_02
        ;;
    3)
        echo "Iniciando Ejecucion Formal 01 (5,000 ejecuciones)..."
        $NICE_PREFIX ./bin/orquestador --mode full --device "$DISPOSITIVO_ID" --ejecucion ejecucion_01
        ;;
    4)
        echo "Iniciando Ejecucion Formal 02 (5,000 ejecuciones)..."
        $NICE_PREFIX ./bin/orquestador --mode full --device "$DISPOSITIVO_ID" --ejecucion ejecucion_02
        ;;
esac

echo ""
echo "======================================================================"
echo "   PROCESO COMPLETADO EXITOSAMENTE"
echo "   Resultados almacenados en: $(pwd)/resultados"
echo "======================================================================"
