import os
import sys
import glob
import math
from collections import defaultdict

# Intentar importar matplotlib y numpy para gráficos
HAS_MATPLOTLIB = False
try:
    import matplotlib
    matplotlib.use('Agg')  # Modo no interactivo para servidores/consolas
    import matplotlib.pyplot as plt
    import numpy as np
    HAS_MATPLOTLIB = True
except ImportError:
    pass


def parse_csv_file(filepath):
    """Lee un CSV de resultados y extrae las filas de datos."""
    rows = []
    with open(filepath, 'r', encoding='utf-8') as f:
        header_line = f.readline()
        if not header_line:
            return []
        headers = [h.strip() for h in header_line.split(',')]
        for line in f:
            line = line.strip()
            if not line:
                continue
            parts = [p.strip() for p in line.split(',')]
            if len(parts) != len(headers):
                continue
            row = dict(zip(headers, parts))
            rows.append(row)
    return rows


def compute_stats(values):
    """Calcula media, desviación estándar, mediana, p95 y p99."""
    if not values:
        return {"n": 0, "mean": 0.0, "std": 0.0, "median": 0.0, "p95": 0.0, "p99": 0.0, "min": 0.0, "max": 0.0}
    
    n = len(values)
    mean_val = sum(values) / n
    variance = sum((x - mean_val) ** 2 for x in values) / (n - 1) if n > 1 else 0.0
    std_val = math.sqrt(variance)
    
    sorted_v = sorted(values)
    def percentile(p):
        k = (n - 1) * (p / 100.0)
        f = math.floor(k)
        c = math.ceil(k)
        if f == c:
            return sorted_v[int(k)]
        d0 = sorted_v[int(f)] * (c - k)
        d1 = sorted_v[int(c)] * (k - f)
        return d0 + d1

    return {
        "n": n,
        "mean": mean_val,
        "std": std_val,
        "median": percentile(50),
        "p95": percentile(95),
        "p99": percentile(99),
        "min": sorted_v[0],
        "max": sorted_v[-1]
    }


def analyze_results(results_dir):
    """Explora el directorio de resultados y agrupa métricas por algoritmo y nivel."""
    csv_files = glob.glob(os.path.join(results_dir, "**", "*.csv"), recursive=True)
    if not csv_files:
        print(f"[AVISO] No se encontraron archivos CSV en: {results_dir}")
        return None

    data = defaultdict(lambda: {
        "keygen_ms": [],
        "enc_ms": [],
        "dec_ms": [],
        "total_ms": [],
        "cpu": [],
        "mem_kb": [],
        "pk_bytes": 0,
        "sk_bytes": 0,
        "ct_bytes": 0,
        "ss_bytes": 0,
        "dispositivos": set()
    })

    for path in csv_files:
        rows = parse_csv_file(path)
        for r in rows:
            if r.get("estado") != "OK":
                continue
            
            alg = r.get("algoritmo", "")
            nivel_str = r.get("nivel", "")
            try:
                keygen_ms = float(r["tiempo_keygen_s"]) * 1000.0
                enc_ms = float(r["tiempo_enc_s"]) * 1000.0
                dec_ms = float(r["tiempo_dec_s"]) * 1000.0
                total_ms = float(r["tiempo_total_s"]) * 1000.0
                cpu = float(r["cpu_percent"])
                mem = float(r["memoria_kb"])
                pk = int(r.get("tamano_pk_bytes", 0))
                sk = int(r.get("tamano_sk_bytes", 0))
                ct = int(r.get("tamano_ct_bytes", 0))
                ss = int(r.get("tamano_ss_bytes", 0))
                disp = r.get("dispositivo", "desconocido")
            except (KeyError, ValueError):
                continue

            # Normalización de etiqueta (ej. ML-KEM-512, RSA-2048)
            if "ML-KEM" in alg or "KYBER" in alg.upper() or nivel_str in ("512", "768", "1024"):
                label = f"ML-KEM-{nivel_str}" if not nivel_str.startswith("ML-KEM") else nivel_str
            elif "RSA" in alg or nivel_str in ("2048", "3072", "4096"):
                label = f"RSA-{nivel_str}" if not nivel_str.startswith("RSA") else nivel_str
            else:
                label = nivel_str if nivel_str else alg

            group = data[label]
            group["keygen_ms"].append(keygen_ms)
            group["enc_ms"].append(enc_ms)
            group["dec_ms"].append(dec_ms)
            group["total_ms"].append(total_ms)
            group["cpu"].append(cpu)
            group["mem_kb"].append(mem)
            if pk > 0: group["pk_bytes"] = pk
            if sk > 0: group["sk_bytes"] = sk
            if ct > 0: group["ct_bytes"] = ct
            if ss > 0: group["ss_bytes"] = ss
            group["dispositivos"].add(disp)

    summary = {}
    for label, metrics in data.items():
        summary[label] = {
            "keygen": compute_stats(metrics["keygen_ms"]),
            "enc": compute_stats(metrics["enc_ms"]),
            "dec": compute_stats(metrics["dec_ms"]),
            "total": compute_stats(metrics["total_ms"]),
            "cpu": compute_stats(metrics["cpu"]),
            "mem_kb": compute_stats(metrics["mem_kb"]),
            "pk_bytes": metrics["pk_bytes"],
            "sk_bytes": metrics["sk_bytes"],
            "ct_bytes": metrics["ct_bytes"],
            "ss_bytes": metrics["ss_bytes"],
            "dispositivos": list(metrics["dispositivos"])
        }

    return summary


def export_markdown_table(summary, output_path):
    """Genera tabla formateada en Markdown."""
    orden = ["RSA-2048", "RSA-3072", "RSA-4096", "ML-KEM-512", "ML-KEM-768", "ML-KEM-1024"]
    keys = [k for k in orden if k in summary] + [k for k in summary if k not in orden]

    with open(output_path, "w", encoding="utf-8") as f:
        f.write("# Resumen Estadístico de Benchmarking Criptográfico\n\n")
        f.write("Tiempos reportados en milisegundos (ms). Medias ± desviación estándar y percentil 95 (P95).\n\n")
        f.write("| Algoritmo / Nivel | N | KeyGen (ms) | Encaps/Encrypt (ms) | Decaps/Decrypt (ms) | Total (ms) | CPU (%) | Memoria (KB) |\n")
        f.write("| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |\n")

        for k in keys:
            s = summary[k]
            kg = f"{s['keygen']['mean']:.3f} ± {s['keygen']['std']:.3f}"
            enc = f"{s['enc']['mean']:.4f} ± {s['enc']['std']:.4f}"
            dec = f"{s['dec']['mean']:.4f} ± {s['dec']['std']:.4f}"
            tot = f"{s['total']['mean']:.3f}"
            cpu = f"{s['cpu']['mean']:.1f}%"
            mem = f"{s['mem_kb']['mean']:.0f}"
            f.write(f"| **{k}** | {s['total']['n']} | {kg} | {enc} | {dec} | {tot} | {cpu} | {mem} |\n")

        f.write("\n\n### Tamaños de Artefactos Criptográficos (Bytes)\n\n")
        f.write("| Algoritmo / Nivel | Clave Pública (PK) | Clave Privada (SK) | Texto Cifrado (CT) | Clave Compartida (SS) |\n")
        f.write("| :--- | :---: | :---: | :---: | :---: |\n")
        for k in keys:
            s = summary[k]
            f.write(f"| **{k}** | {s['pk_bytes']:,} B | {s['sk_bytes']:,} B | {s['ct_bytes']:,} B | {s['ss_bytes']} B |\n")


def export_latex_table(summary, output_path):
    """Genera tabla formateada en LaTeX (booktabs) lista para incorporar a la tesis."""
    orden = ["RSA-2048", "RSA-3072", "RSA-4096", "ML-KEM-512", "ML-KEM-768", "ML-KEM-1024"]
    keys = [k for k in orden if k in summary] + [k for k in summary if k not in orden]

    with open(output_path, "w", encoding="utf-8") as f:
        f.write("% ====================================================================\n")
        f.write("% TABLA PARA TESIS UVG: RENDIMIENTO COMPUTACIONAL (RSA vs ML-KEM)\n")
        f.write("% Requiere: \\usepackage{booktabs}, \\usepackage{siunitx}\n")
        f.write("% ====================================================================\n")
        f.write("\\begin{table}[htbp]\n")
        f.write("  \\centering\n")
        f.write("  \\caption{Comparativa de rendimiento entre RSA y ML-KEM en milisegundos (ms)}\n")
        f.write("  \\label{tab:rendimiento_criptografia}\n")
        f.write("  \\begin{tabular}{l c r r r r r}\n")
        f.write("    \\toprule\n")
        f.write("    \\textbf{Algoritmo} & $N$ & \\textbf{KeyGen (ms)} & \\textbf{Enc/Encaps (ms)} & \\textbf{Dec/Decaps (ms)} & \\textbf{Total (ms)} & \\textbf{CPU (\\%)} \\\\\n")
        f.write("    \\midrule\n")

        for k in keys:
            s = summary[k]
            kg = f"{s['keygen']['mean']:.2f} \\pm {s['keygen']['std']:.2f}"
            enc = f"{s['enc']['mean']:.4f} \\pm {s['enc']['std']:.4f}"
            dec = f"{s['dec']['mean']:.4f} \\pm {s['dec']['std']:.4f}"
            tot = f"{s['total']['mean']:.2f}"
            cpu = f"{s['cpu']['mean']:.1f}"
            f.write(f"    {k} & {s['total']['n']} & ${kg}$ & ${enc}$ & ${dec}$ & {tot} & {cpu}\\% \\\\\n")

        f.write("    \\bottomrule\n")
        f.write("  \\end{tabular}\n")
        f.write("\\end{table}\n\n")

        f.write("% TABLA DE TAMAÑO DE CLAVES Y MENSAJES\n")
        f.write("\\begin{table}[htbp]\n")
        f.write("  \\centering\n")
        f.write("  \\caption{Tamaños de claves y texto cifrado en bytes}\n")
        f.write("  \\label{tab:tamanos_criptografia}\n")
        f.write("  \\begin{tabular}{l r r r r}\n")
        f.write("    \\toprule\n")
        f.write("    \\textbf{Algoritmo} & \\textbf{Clave Pública (B)} & \\textbf{Clave Privada (B)} & \\textbf{Texto Cifrado (B)} & \\textbf{Secreto (B)} \\\\\n")
        f.write("    \\midrule\n")
        for k in keys:
            s = summary[k]
            f.write(f"    {k} & {s['pk_bytes']} & {s['sk_bytes']} & {s['ct_bytes']} & {s['ss_bytes']} \\\\\n")
        f.write("    \\bottomrule\n")
        f.write("  \\end{tabular}\n")
        f.write("\\end{table}\n")


def generate_figures(summary, output_dir):
    """Genera figuras en formato PNG de 300 DPI si matplotlib está disponible."""
    if not HAS_MATPLOTLIB:
        print("[AVISO] matplotlib no está instalado. Omitiendo generación de gráficos PNG.")
        print("        (Las tablas Markdown y LaTeX se generaron correctamente).")
        return

    os.makedirs(output_dir, exist_ok=True)
    orden = ["RSA-2048", "RSA-3072", "RSA-4096", "ML-KEM-512", "ML-KEM-768", "ML-KEM-1024"]
    keys = [k for k in orden if k in summary]

    if not keys:
        return

    plt.rcParams.update({
        'font.size': 11,
        'figure.autolayout': True,
        'axes.grid': True,
        'grid.alpha': 0.3
    })

    # 1. Gráfico de Tiempos de Operación (KeyGen vs Encaps vs Decaps) en escala logarítmica
    fig, ax = plt.subplots(figsize=(10, 5))
    x = np.arange(len(keys))
    width = 0.25

    keygen_vals = [summary[k]["keygen"]["mean"] for k in keys]
    enc_vals = [summary[k]["enc"]["mean"] for k in keys]
    dec_vals = [summary[k]["dec"]["mean"] for k in keys]

    ax.bar(x - width, keygen_vals, width, label='Key Generation', color='#1f77b4')
    ax.bar(x, enc_vals, width, label='Encryption / Encapsulation', color='#2ca02c')
    ax.bar(x + width, dec_vals, width, label='Decryption / Decapsulation', color='#d62728')

    ax.set_yscale('log')
    ax.set_ylabel('Tiempo promedio (milisegundos) [Escala Logarítmica]')
    ax.set_title('Comparativa de Tiempos de Operación Criptográfica (RSA vs. ML-KEM)')
    ax.set_xticks(x)
    ax.set_xticklabels(keys, rotation=15)
    ax.legend()
    fig_path = os.path.join(output_dir, "figura_tiempos_operaciones.png")
    plt.savefig(fig_path, dpi=300)
    plt.close()
    print(f" -> Figura guardada: {fig_path}")

    # 2. Gráfico de Tamaños de Claves y Texto Cifrado (Bytes)
    fig, ax = plt.subplots(figsize=(10, 5))
    pk_vals = [summary[k]["pk_bytes"] for k in keys]
    ct_vals = [summary[k]["ct_bytes"] for k in keys]
    sk_vals = [summary[k]["sk_bytes"] for k in keys]

    width = 0.25
    ax.bar(x - width, pk_vals, width, label='Clave Pública (PK)', color='#3b528b')
    ax.bar(x, ct_vals, width, label='Texto Cifrado (CT)', color='#5ec962')
    ax.bar(x + width, sk_vals, width, label='Clave Privada (SK)', color='#440154')

    ax.set_ylabel('Tamaño en Bytes')
    ax.set_title('Comparativa de Sobrecarga de Comunicación y Memoria (Artefactos en Bytes)')
    ax.set_xticks(x)
    ax.set_xticklabels(keys, rotation=15)
    ax.legend()
    fig_path = os.path.join(output_dir, "figura_tamano_artefactos.png")
    plt.savefig(fig_path, dpi=300)
    plt.close()
    print(f" -> Figura guardada: {fig_path}")


def main():
    print("======================================================================")
    print(" GENERADOR DE ESTADÍSTICAS Y FIGURAS PARA TESIS (RSA vs. ML-KEM)")
    print(" Universidad del Valle de Guatemala")
    print("======================================================================")

    base_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
    results_dir = os.path.join(base_dir, "resultados")

    if len(sys.argv) > 1:
        results_dir = sys.argv[1]

    print(f"Analizando resultados en: {results_dir}")
    summary = analyze_results(results_dir)
    if not summary:
        print("[ERROR] No se encontraron datos válidos para procesar.")
        sys.exit(1)

    out_docs = os.path.join(base_dir, "resultados", "analisis_tesis")
    os.makedirs(out_docs, exist_ok=True)

    md_path = os.path.join(out_docs, "tabla_resumen_tesis.md")
    tex_path = os.path.join(out_docs, "tabla_resumen_tesis.tex")

    export_markdown_table(summary, md_path)
    export_latex_table(summary, tex_path)
    print(f"[OK] Tabla Markdown: {md_path}")
    print(f"[OK] Tabla LaTeX:    {tex_path}")

    generate_figures(summary, out_docs)

    print("======================================================================")
    print(" Proceso de análisis completado.")
    print("======================================================================")


if __name__ == "__main__":
    main()
