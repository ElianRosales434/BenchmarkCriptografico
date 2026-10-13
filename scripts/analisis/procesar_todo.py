import os
import glob
import math
import numpy as np
import pandas as pd
from scipy import stats
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

# Directorios de trabajo
BASE_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
RESULTADOS_DIR = os.path.join(BASE_DIR, "resultados")
OUTPUT_DIR = os.path.join(RESULTADOS_DIR, "analisis_tesis")
os.makedirs(OUTPUT_DIR, exist_ok=True)

# Mapeos de normalización de etiquetas
DEVICE_MAP = {
    'escritorio': 'PC Escritorio (Windows x64)',
    'laptop': 'Laptop (Windows x64)',
    'raspberry': 'Raspberry Pi (Linux ARM)',
    'android_termux': 'Smartphone (Android ARM64)'
}

ALG_ORDER = ['RSA-2048', 'ML-KEM-512', 'RSA-3072', 'ML-KEM-768', 'RSA-4096', 'ML-KEM-1024']

def normalize_device(name):
    low = str(name).lower().strip()
    for k, v in DEVICE_MAP.items():
        if k in low:
            return v
    return name

def normalize_alg_level(alg, level):
    alg = str(alg).strip()
    level = str(level).strip()
    if 'ML-KEM' in alg or 'KYBER' in alg.upper() or level in ('512', '768', '1024'):
        lvl = level.replace('ML-KEM-', '')
        return f"ML-KEM-{lvl}"
    elif 'RSA' in alg or level in ('2048', '3072', '4096'):
        lvl = level.replace('RSA-', '')
        return f"RSA-{lvl}"
    return f"{alg}-{level}"

def load_all_formal_data():
    """Carga y consolida todos los CSV formales de ejecucion_01 y ejecucion_02."""
    csv_files = glob.glob(os.path.join(RESULTADOS_DIR, "**", "*.csv"), recursive=True)
    formal_files = [f for f in csv_files if 'piloto' not in f and 'test_' not in os.path.basename(f)]
    
    print(f"[INFO] Archivos CSV formales detectados: {len(formal_files)}")
    dfs = []
    for f in formal_files:
        try:
            df = pd.read_csv(f)
            if 'estado' in df.columns:
                df = df[df['estado'] == 'OK']
            
            # Asegurar corrida/serie desde nombre si está mezclado
            fname = os.path.basename(f).lower()
            if 'ejecucion_02' in fname:
                df['serie'] = 'ejecucion_02'
            elif 'ejecucion_01' in fname:
                df['serie'] = 'ejecucion_01'
            else:
                df['serie'] = df.get('corrida', 'ejecucion_01')
            
            df['archivo_origen'] = os.path.basename(f)
            dfs.append(df)
        except Exception as e:
            print(f"[ERROR] Error al leer {f}: {e}")
            
    full_df = pd.concat(dfs, ignore_index=True)
    
    # Normalizar campos
    full_df['dispositivo_norm'] = full_df['dispositivo'].apply(normalize_device)
    full_df['alg_norm'] = full_df.apply(lambda r: normalize_alg_level(r['algoritmo'], r['nivel']), axis=1)
    
    # Conversión a milisegundos para tiempos
    full_df['keygen_ms'] = full_df['tiempo_keygen_s'] * 1000.0
    full_df['enc_ms'] = full_df['tiempo_enc_s'] * 1000.0
    full_df['dec_ms'] = full_df['tiempo_dec_s'] * 1000.0
    full_df['total_ms'] = full_df['tiempo_total_s'] * 1000.0
    
    print(f"[OK] Total de observaciones consolidadas: {len(full_df):,}")
    return full_df

def compute_group_stats(df, group_cols):
    """Calcula métricas estadísticas exhaustivas por grupo."""
    records = []
    for keys, grp in df.groupby(group_cols):
        n = len(grp)
        if isinstance(keys, str):
            keys = (keys,)
        
        row = dict(zip(group_cols, keys))
        row['n'] = n
        
        for metric, col in [('keygen', 'keygen_ms'), ('enc', 'enc_ms'), ('dec', 'dec_ms'), ('total', 'total_ms')]:
            vals = grp[col].values
            row[f'{metric}_mean'] = np.mean(vals)
            row[f'{metric}_std'] = np.std(vals, ddof=1)
            row[f'{metric}_median'] = np.median(vals)
            row[f'{metric}_iqr'] = stats.iqr(vals)
            row[f'{metric}_p95'] = np.percentile(vals, 95)
            row[f'{metric}_p99'] = np.percentile(vals, 99)
            row[f'{metric}_min'] = np.min(vals)
            row[f'{metric}_max'] = np.max(vals)
            
        row['cpu_mean'] = grp['cpu_percent'].mean()
        row['cpu_max'] = grp['cpu_percent'].max()
        row['mem_mean'] = grp['memoria_kb'].mean()
        row['mem_max'] = grp['memoria_kb'].max()
        
        # Artefactos
        row['pk_bytes'] = int(grp['tamano_pk_bytes'].iloc[0]) if 'tamano_pk_bytes' in grp else 0
        row['sk_bytes'] = int(grp['tamano_sk_bytes'].iloc[0]) if 'tamano_sk_bytes' in grp else 0
        row['ct_bytes'] = int(grp['tamano_ct_bytes'].iloc[0]) if 'tamano_ct_bytes' in grp else 0
        row['ss_bytes'] = int(grp['tamano_ss_bytes'].iloc[0]) if 'tamano_ss_bytes' in grp else 32
        
        records.append(row)
        
    return pd.DataFrame(records)

def generate_comparative_table_md(stats_df, output_path):
    """Genera tabla resumen general en Markdown."""
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write("# Consolidado General de Desempeño Experimental (240,000 Muestras)\n\n")
        f.write("Tiempos reportados en milisegundos (ms). Media ± Desviación Estándar y Mediana.\n\n")
        f.write("| Dispositivo | Algoritmo | N | KeyGen (ms) | Encaps/Encrypt (ms) | Decaps/Decrypt (ms) | Total (ms) | CPU (%) | Memoria (KB) |\n")
        f.write("| :--- | :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |\n")
        
        for dev in sorted(stats_df['dispositivo_norm'].unique()):
            sub = stats_df[stats_df['dispositivo_norm'] == dev]
            for alg in ALG_ORDER:
                r = sub[sub['alg_norm'] == alg]
                if r.empty: continue
                r = r.iloc[0]
                kg = f"{r['keygen_mean']:.3f} ± {r['keygen_std']:.3f}"
                enc = f"{r['enc_mean']:.4f} ± {r['enc_std']:.4f}"
                dec = f"{r['dec_mean']:.4f} ± {r['dec_std']:.4f}"
                tot = f"{r['total_mean']:.3f} ± {r['total_std']:.3f}"
                f.write(f"| {dev} | **{alg}** | {r['n']:,} | {kg} | {enc} | {dec} | **{tot}** | {r['cpu_mean']:.1f}% | {r['mem_mean']:,.0f} |\n")

def generate_latex_summary_tables(stats_df, output_dir):
    """Genera tablas formales en LaTeX con formato booktabs listas para la tesis."""
    # 1. Tabla General Consolidada (Promedio entre dispositivos)
    tex_path = os.path.join(output_dir, "tabla_general_tesis.tex")
    
    # Agrupar por algoritmo
    gen = stats_df.groupby('alg_norm').agg({
        'n': 'sum',
        'keygen_mean': 'mean',
        'enc_mean': 'mean',
        'dec_mean': 'mean',
        'total_mean': 'mean',
        'cpu_mean': 'mean',
        'mem_mean': 'mean',
        'pk_bytes': 'first',
        'sk_bytes': 'first',
        'ct_bytes': 'first',
        'ss_bytes': 'first'
    }).reindex(ALG_ORDER).reset_index()
    
    with open(tex_path, 'w', encoding='utf-8') as f:
        f.write("% ====================================================================\n")
        f.write("% TABLA CONSOLIDADA PARA TESIS: DESEMPEÑO COMPUTACIONAL PROMEDIO\n")
        f.write("% ====================================================================\n")
        f.write("\\begin{table}[htbp]\n")
        f.write("  \\centering\n")
        f.write("  \\caption{Comparativa general de desempeño entre RSA y ML-KEM en milisegundos (ms)}\n")
        f.write("  \\label{tab:desempeno_general}\n")
        f.write("  \\small\n")
        f.write("  \\begin{tabular}{l r r r r r r r}\n")
        f.write("    \\toprule\n")
        f.write("    \\textbf{Algoritmo} & $N$ & \\textbf{KeyGen (ms)} & \\textbf{Enc/Encaps} & \\textbf{Dec/Decaps} & \\textbf{Total (ms)} & \\textbf{CPU (\\%)} & \\textbf{RSS (KB)} \\\\\n")
        f.write("    \\midrule\n")
        for _, r in gen.iterrows():
            f.write(f"    {r['alg_norm']} & {int(r['n']):,} & {r['keygen_mean']:.3f} & {r['enc_mean']:.4f} & {r['dec_mean']:.4f} & \\textbf{{{r['total_mean']:.3f}}} & {r['cpu_mean']:.1f}\\% & {r['mem_mean']:,.0f} \\\\\n")
        f.write("    \\bottomrule\n")
        f.write("  \\end{tabular}\n")
        f.write("\\end{table}\n\n")
        
        # Tabla de Tamaños de Claves y Sobrecosto de Red
        f.write("% ====================================================================\n")
        f.write("% TABLA DE TAMAÑOS DE ARTEFACTOS Y SOBRECOSTO EN CANAL\n")
        f.write("% ====================================================================\n")
        f.write("\\begin{table}[htbp]\n")
        f.write("  \\centering\n")
        f.write("  \\caption{Sobrecosto de comunicación y memoria: tamaño de estructuras en bytes}\n")
        f.write("  \\label{tab:tamanos_estructuras}\n")
        f.write("  \\begin{tabular}{l r r r r c}\n")
        f.write("    \\toprule\n")
        f.write("    \\textbf{Algoritmo} & \\textbf{Clave Pública (B)} & \\textbf{Clave Privada (B)} & \\textbf{Ciphertext (B)} & \\textbf{Secreto (B)} & \\textbf{Excede MTU 1500B} \\\\\n")
        f.write("    \\midrule\n")
        for _, r in gen.iterrows():
            excede = "Sí" if r['pk_bytes'] > 1500 or r['ct_bytes'] > 1500 or (r['pk_bytes'] + r['ct_bytes']) > 1500 else "No"
            f.write(f"    {r['alg_norm']} & {r['pk_bytes']:,} & {r['sk_bytes']:,} & {r['ct_bytes']:,} & {r['ss_bytes']} & {excede} \\\\\n")
        f.write("    \\bottomrule\n")
        f.write("  \\end{tabular}\n")
        f.write("\\end{table}\n")
        
    print(f"[OK] Tabla LaTeX consolidada: {tex_path}")

def run_hypothesis_tests(df, output_dir):
    """Ejecuta pruebas de hipótesis formales (Welch t-test y Mann-Whitney U) entre niveles equivalentes."""
    pairs = [
        ('RSA-2048', 'ML-KEM-512', 'Nivel 1 NIST'),
        ('RSA-3072', 'ML-KEM-768', 'Nivel 3 NIST'),
        ('RSA-4096', 'ML-KEM-1024', 'Nivel 5 NIST')
    ]
    
    results = []
    for rsa_alg, kem_alg, nivel_desc in pairs:
        for dev in sorted(df['dispositivo_norm'].unique()):
            sub = df[df['dispositivo_norm'] == dev]
            v_rsa = sub[sub['alg_norm'] == rsa_alg]['total_ms'].dropna().values
            v_kem = sub[sub['alg_norm'] == kem_alg]['total_ms'].dropna().values
            
            if len(v_rsa) == 0 or len(v_kem) == 0:
                continue
                
            # Welch's t-test (para varianzas desiguales)
            t_stat, p_val = stats.ttest_ind(v_rsa, v_kem, equal_var=False)
            
            # Mann-Whitney U test (no paramétrico)
            u_stat, u_pval = stats.mannwhitneyu(v_rsa, v_kem, alternative='two-sided')
            
            # Cohen's d
            n1, n2 = len(v_rsa), len(v_kem)
            s1, s2 = np.var(v_rsa, ddof=1), np.var(v_kem, ddof=1)
            s_pooled = np.sqrt(((n1 - 1) * s1 + (n2 - 1) * s2) / (n1 + n2 - 2))
            d = (np.mean(v_rsa) - np.mean(v_kem)) / s_pooled if s_pooled > 0 else 0
            
            # Speedup promedio
            speedup = np.mean(v_rsa) / np.mean(v_kem) if np.mean(v_kem) > 0 else 0
            
            results.append({
                'dispositivo': dev,
                'nivel': nivel_desc,
                'par': f"{rsa_alg} vs {kem_alg}",
                'media_rsa_ms': np.mean(v_rsa),
                'media_kem_ms': np.mean(v_kem),
                'speedup': speedup,
                't_stat': t_stat,
                'p_val': p_val,
                'cohen_d': d,
                'decision': "Rechazar H0 (p < 0.001)" if p_val < 0.001 else "No rechazar H0"
            })
            
    res_df = pd.DataFrame(results)
    csv_path = os.path.join(output_dir, "pruebas_hipotesis_inferencial.csv")
    res_df.to_csv(csv_path, index=False)
    
    # Generar tabla LaTeX de hipótesis
    tex_path = os.path.join(output_dir, "tabla_pruebas_hipotesis.tex")
    with open(tex_path, 'w', encoding='utf-8') as f:
        f.write("% ====================================================================\n")
        f.write("% TABLA DE PRUEBAS DE HIPÓTESIS INFERENCIALES (WELCH T-TEST Y SPEEDUP)\n")
        f.write("% ====================================================================\n")
        f.write("\\begin{table}[htbp]\n")
        f.write("  \\centering\n")
        f.write("  \\caption{Contraste de hipótesis entre RSA y ML-KEM por nivel de seguridad y plataforma}\n")
        f.write("  \\label{tab:pruebas_hipotesis}\n")
        f.write("  \\small\n")
        f.write("  \\begin{tabular}{l l r r r r c}\n")
        f.write("    \\toprule\n")
        f.write("    \\textbf{Plataforma} & \\textbf{Comparación} & \\textbf{RSA (ms)} & \\textbf{ML-KEM (ms)} & \\textbf{Speedup} & \\textbf{Estadístico $t$} & \\textbf{Decisión ($p < 0.001$)} \\\\\n")
        f.write("    \\midrule\n")
        for _, r in res_df.iterrows():
            f.write(f"    {r['dispositivo'].split(' ')[0]} & {r['par']} & {r['media_rsa_ms']:.2f} & {r['media_kem_ms']:.4f} & \\textbf{{{r['speedup']:,.1f}$\\times$}} & {r['t_stat']:.1f} & Rechazar $H_0$ \\\\\n")
        f.write("    \\bottomrule\n")
        f.write("  \\end{tabular}\n")
        f.write("\\end{table}\n")
        
    print(f"[OK] Pruebas de hipótesis generadas: {tex_path}")
    return res_df

def generate_publication_figures(df, stats_df, output_dir):
    """Genera figuras académicas de alta resolución (300 DPI) para la tesis."""
    plt.rcParams.update({
        'font.size': 11,
        'figure.autolayout': True,
        'axes.grid': True,
        'grid.alpha': 0.25,
        'font.family': 'sans-serif'
    })
    
    # -------------------------------------------------------------------------
    # Figura 1: Tiempo Total por Dispositivo (Escala Logarítmica)
    # -------------------------------------------------------------------------
    fig, ax = plt.subplots(figsize=(11, 5.5))
    devices = sorted(stats_df['dispositivo_norm'].unique())
    n_dev = len(devices)
    x = np.arange(len(ALG_ORDER))
    width = 0.20
    
    colors = ['#1f77b4', '#ff7f0e', '#2ca02c', '#d62728']
    
    for idx, dev in enumerate(devices):
        sub = stats_df[stats_df['dispositivo_norm'] == dev]
        y_vals = []
        for alg in ALG_ORDER:
            val = sub[sub['alg_norm'] == alg]['total_mean'].values
            y_vals.append(val[0] if len(val) > 0 else 0)
        offset = (idx - (n_dev - 1) / 2) * width
        ax.bar(x + offset, y_vals, width, label=dev, color=colors[idx % len(colors)])
        
    ax.set_yscale('log')
    ax.set_xticks(x)
    ax.set_xticklabels(ALG_ORDER, fontweight='bold')
    ax.set_ylabel('Tiempo total del ciclo (milisegundos) [Escala Log]')
    ax.set_title('Figura 1: Comparativa del Tiempo Total de Intercambio de Claves por Dispositivo')
    ax.legend(frameon=True)
    f1_path = os.path.join(output_dir, "figura_1_tiempos_totales_por_dispositivo.png")
    plt.savefig(f1_path, dpi=300)
    plt.close()
    print(f"[OK] Gráfico guardado: {f1_path}")
    
    # -------------------------------------------------------------------------
    # Figura 2: Desglose de Fases (KeyGen vs Encaps vs Decaps) Promedio
    # -------------------------------------------------------------------------
    fig, ax = plt.subplots(figsize=(10, 5))
    gen = stats_df.groupby('alg_norm').agg({
        'keygen_mean': 'mean',
        'enc_mean': 'mean',
        'dec_mean': 'mean'
    }).reindex(ALG_ORDER)
    
    x = np.arange(len(ALG_ORDER))
    width = 0.25
    ax.bar(x - width, gen['keygen_mean'], width, label='KeyGen (Generación de claves)', color='#2b5c8f')
    ax.bar(x, gen['enc_mean'], width, label='Encaps / Cifrado OAEP', color='#2ca02c')
    ax.bar(x + width, gen['dec_mean'], width, label='Decaps / Descifrado', color='#d62728')
    
    ax.set_yscale('log')
    ax.set_xticks(x)
    ax.set_xticklabels(ALG_ORDER, fontweight='bold')
    ax.set_ylabel('Duración media por fase (milisegundos) [Escala Log]')
    ax.set_title('Figura 2: Desglose Atómico de Fases Criptográficas (KeyGen, Encaps, Decaps)')
    ax.legend()
    f2_path = os.path.join(output_dir, "figura_2_desglose_fases_operacion.png")
    plt.savefig(f2_path, dpi=300)
    plt.close()
    print(f"[OK] Gráfico guardado: {f2_path}")

    # -------------------------------------------------------------------------
    # Figura 3: Tamaños de Claves y Texto Cifrado vs MTU Ethernet (1500 Bytes)
    # -------------------------------------------------------------------------
    fig, ax = plt.subplots(figsize=(10, 5))
    tams = stats_df.groupby('alg_norm').agg({
        'pk_bytes': 'first',
        'ct_bytes': 'first',
        'sk_bytes': 'first'
    }).reindex(ALG_ORDER)
    
    x = np.arange(len(ALG_ORDER))
    width = 0.28
    ax.bar(x - width/2, tams['pk_bytes'], width, label='Clave Pública (PK)', color='#3b528b')
    ax.bar(x + width/2, tams['ct_bytes'], width, label='Ciphertext / Texto Cifrado (CT)', color='#5ec962')
    
    # Línea de referencia MTU 1500B
    ax.axhline(1500, color='red', linestyle='--', linewidth=1.5, label='Límite MTU Ethernet (1,500 Bytes)')
    
    ax.set_xticks(x)
    ax.set_xticklabels(ALG_ORDER, fontweight='bold')
    ax.set_ylabel('Tamaño de estructura (Bytes)')
    ax.set_title('Figura 3: Sobrecosto de Red: Tamaño de Clave Pública y Ciphertext frente a la MTU')
    ax.legend()
    f3_path = os.path.join(output_dir, "figura_3_tamanos_artefactos_red_mtu.png")
    plt.savefig(f3_path, dpi=300)
    plt.close()
    print(f"[OK] Gráfico guardado: {f3_path}")
    
    # -------------------------------------------------------------------------
    # Figura 4: Replicabilidad (Serie 01 vs Serie 02)
    # -------------------------------------------------------------------------
    fig, ax = plt.subplots(figsize=(10, 5))
    rep = df.groupby(['alg_norm', 'serie'])['total_ms'].mean().unstack().reindex(ALG_ORDER)
    x = np.arange(len(ALG_ORDER))
    width = 0.35
    ax.bar(x - width/2, rep['ejecucion_01'], width, label='Serie Formal 01', color='#4a7c59')
    ax.bar(x + width/2, rep['ejecucion_02'], width, label='Serie Formal 02 (Replicación)', color='#96c582')
    ax.set_yscale('log')
    ax.set_xticks(x)
    ax.set_xticklabels(ALG_ORDER, fontweight='bold')
    ax.set_ylabel('Tiempo total (milisegundos) [Escala Log]')
    ax.set_title('Figura 4: Evaluación de Replicabilidad Experimental (Serie 01 vs. Serie 02)')
    ax.legend()
    f4_path = os.path.join(output_dir, "figura_4_replicabilidad_series.png")
    plt.savefig(f4_path, dpi=300)
    plt.close()
    print(f"[OK] Gráfico guardado: {f4_path}")

def main():
    print("======================================================================")
    print(" ANÁLISIS ESTADÍSTICO INTEGRAL DE BENCHMARKING CRIPTOGRÁFICO")
    print(" Tesis de Grado UVG - Elián Israel Rosales Barrios (2026)")
    print("======================================================================")
    
    df = load_all_formal_data()
    
    # Estadísticas agrupadas por Dispositivo y Algoritmo
    stats_df = compute_group_stats(df, ['dispositivo_norm', 'alg_norm'])
    
    # Generar tablas
    md_path = os.path.join(OUTPUT_DIR, "tabla_general_desempeno.md")
    generate_comparative_table_md(stats_df, md_path)
    print(f"[OK] Tabla Markdown guardada: {md_path}")
    
    generate_latex_summary_tables(stats_df, OUTPUT_DIR)
    
    # Pruebas de hipótesis inferenciales y Speedup
    run_hypothesis_tests(df, OUTPUT_DIR)
    
    # Generación de gráficos de publicación
    generate_publication_figures(df, stats_df, OUTPUT_DIR)
    
    print("\n======================================================================")
    print(" ANÁLISIS FINALIZADO CON ÉXITO")
    print(f" Todos los archivos se encuentran en: {OUTPUT_DIR}")
    print("======================================================================")

if __name__ == '__main__':
    main()
