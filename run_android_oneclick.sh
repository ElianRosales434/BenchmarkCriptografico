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
    CANDIDATE=$(find "$SCRIPT_DIR" -maxdepth 3 -type f -name "orquestador.c" 2>/dev/null | head -n 1 || true)
    if [ -n "$CANDIDATE" ]; then
        PROJECT_ROOT="$(cd "$(dirname "$CANDIDATE")/../.." && pwd)"
    else
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
    echo " El script 'run_android_oneclick.sh' se ejecuto en:"
    echo "   $SCRIPT_DIR"
    echo " pero no se localizo 'scripts/c/orquestador.c'."
    echo ""
    echo " Causa:"
    echo "   Se descargo o copio unicamente este archivo '.sh' de forma aislada"
    echo "   sin la carpeta completa del repositorio."
    echo ""
    echo " Solucion:"
    echo "   Asegurese de clonar o copiar la carpeta completa del repositorio en Termux:"
    echo "     git clone <URL_DEL_REPOSITORIO>"
    echo "     cd bold-salk && ./run_android_oneclick.sh"
    echo "======================================================================"
    exit 1
fi

cd "$PROJECT_ROOT"

echo "======================================================================"
echo "   SUITE DE BENCHMARKING CRIPTOGRAFICO (RSA vs. ML-KEM / Kyber)"
echo "   Plataforma: Android (Termux ARM64) - Arquitectura 100% C Nativa"
echo "   Directorio de trabajo: $PROJECT_ROOT"
echo "======================================================================"
echo ""

# 1. Gestion de Wake-Lock en Android
echo "[1/5] Adquiriendo Wake-Lock para evitar Doze Mode en Android..."
if command -v termux-wake-lock &> /dev/null; then
    termux-wake-lock
    echo "    Wake-lock adquirido exitosamente (la CPU se mantendra activa)."
else
    echo "    [AVISO] Comando termux-wake-lock no encontrado. Mantenga la pantalla encendida."
fi

cleanup() {
    echo ""
    if command -v termux-wake-unlock &> /dev/null; then
        echo "Liberando Wake-Lock de Android..."
        termux-wake-unlock || true
    fi
}
trap cleanup EXIT

# 2. Verificar herramientas base del sistema en Termux
echo ""
echo "[2/5] Verificando herramientas de compilacion en Termux..."
MISSING_PKGS=""
for pkg in git cmake clang make openssl; do
    if ! command -v $pkg &> /dev/null; then
        MISSING_PKGS="$MISSING_PKGS $pkg"
    fi
done

if [ -n "$MISSING_PKGS" ]; then
    echo "Instalando paquetes faltantes:$MISSING_PKGS..."
    pkg update -y -qq
    pkg install -y -qq $MISSING_PKGS
fi

# Verificar e instalar liboqs en Termux si no existe
HAS_LIBOQS=false
if [ -f "$PREFIX/lib/liboqs.so" ] || [ -f "/data/data/com.termux/files/usr/lib/liboqs.so" ]; then
    HAS_LIBOQS=true
fi

if [ "$HAS_LIBOQS" = false ]; then
    echo ""
    echo "======================================================================"
    echo " [AUTO-CONFIGURACION] Compilando e instalando liboqs en Termux..."
    echo " Requerido para la evaluacion de ML-KEM / Kyber en arquitectura ARM"
    echo "======================================================================"
    CURRENT_PWD="$(pwd)"
    cd "$HOME"
    if [ ! -d "liboqs" ]; then
        echo "Clonando repositorio oficial de liboqs..."
        git clone --depth 1 https://github.com/open-quantum-safe/liboqs.git
    fi
    cd liboqs
    mkdir -p build && cd build
    cmake -DCMAKE_INSTALL_PREFIX="$PREFIX" -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Release ..
    make -j$(nproc)
    make install
    cd "$CURRENT_PWD"
    echo "[OK] liboqs instalado exitosamente en Termux."
else
    echo "    Biblioteca post-cuantica liboqs detectada en Termux [OK]."
fi

# Precompilar suite nativa en C si no existe
if [ ! -f "bin/rsa_benchmark" ] || [ ! -f "bin/kyber_benchmark" ] || [ ! -f "bin/orquestador" ] || [ ! -f "bin/inventario" ]; then
    echo ""
    echo "==> Compilando binarios de la suite nativa en C con make..."
    make
fi

# 3. Estabilización y consideraciones de procesos en Android
echo ""
echo "======================================================================"
echo "[3/5] ESTABILIZACION DEL ENTORNO EN ANDROID"
echo "======================================================================"
echo "INFORMACION TECNICA:"
echo "En Android (sin root), el sandbox de seguridad del sistema operativo"
echo "impide que una aplicacion finalice los procesos de otras aplicaciones."
echo ""
echo "Para lograr la maxima estabilidad experimental, aplique manualmente:"
echo " 1. Conecte el telefono al cargador de energia."
echo " 2. Active el MODO AVION (evita interrupciones de red 4G/5G y Wi-Fi)."
echo " 3. En la pantalla de aplicaciones recientes de Android, presione"
echo "    'Cerrar todas las aplicaciones' dejando unicamente Termux."
echo "======================================================================"
read -p "Presione [ENTER] cuando haya preparado el dispositivo para continuar..." DUMMY

# 4. Generar ficha de inventario con C nativo
echo ""
echo "[4/5] Registrando inventario del dispositivo en C nativo..."
./bin/inventario

# 5. Menu interactivo
echo ""
echo "======================================================================"
echo "[5/5] CONFIGURACION DE LA SERIE DE EJECUCIONES (SUITE C PURA)"
echo "======================================================================"
echo "Seleccione el modo que desea ejecutar:"
echo "  [1] Prueba Piloto (200 repeticiones por nivel - Recomendado)"
echo "  [2] Serie Formal Completa (ejecucion_01 y ejecucion_02 - 5,000 rep.)"
echo "  [3] Solo Ejecucion Formal 01 (5,000 repeticiones)"
echo "  [4] Solo Ejecucion Formal 02 de Replicacion (5,000 repeticiones)"
echo ""
read -p "Ingrese opcion [1-4] (defecto 1): " MODO_ELEGIDO
MODO_ELEGIDO=${MODO_ELEGIDO:-1}

read -p "Identificador de este equipo [defecto android_termux]: " DISPOSITIVO_ID
DISPOSITIVO_ID=${DISPOSITIVO_ID:-android_termux}

echo ""
echo "Reposo termico de 5 segundos previo al inicio..."
sleep 5

case $MODO_ELEGIDO in
    1)
        echo "Iniciando Prueba Piloto (200 ejecuciones por nivel)..."
        ./bin/orquestador --mode pilot --device "$DISPOSITIVO_ID" --ejecucion piloto
        ;;
    2)
        echo "Iniciando Serie Formal Completa: Ejecucion 01 y 02 (5,000 ejecuciones)..."
        ./bin/orquestador --mode full --device "$DISPOSITIVO_ID" --ejecucion ejecucion_01
        
        echo ""
        echo "=================================================================="
        echo "Pausa de enfriamiento termico de 60 segundos antes de replicar..."
        echo "=================================================================="
        sleep 60
        
        echo "Iniciando Ejecucion Formal 02 de Replicacion (5,000 ejecuciones)..."
        ./bin/orquestador --mode full --device "$DISPOSITIVO_ID" --ejecucion ejecucion_02
        ;;
    3)
        echo "Iniciando Ejecucion Formal 01 (5,000 ejecuciones)..."
        ./bin/orquestador --mode full --device "$DISPOSITIVO_ID" --ejecucion ejecucion_01
        ;;
    4)
        echo "Iniciando Ejecucion Formal 02 (5,000 ejecuciones)..."
        ./bin/orquestador --mode full --device "$DISPOSITIVO_ID" --ejecucion ejecucion_02
        ;;
esac

echo ""
echo "======================================================================"
echo "   PROCESO COMPLETADO EXITOSAMENTE"
echo "   Resultados almacenados en: $(pwd)/resultados"
echo ""
echo "   Para consolidar tablas y graficos de tesis, ejecute en su PC:"
echo "   python scripts/analisis/graficar_tesis.py"
echo "======================================================================"
