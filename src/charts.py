import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns

plt.style.use('seaborn-v0_8-darkgrid' if 'seaborn-v0_8-darkgrid' in plt.style.available else 'default')
sns.set_theme(style="darkgrid")
plt.rcParams['font.sans-serif'] = 'DejaVu Sans'
plt.rcParams['font.family'] = 'sans-serif'

def gerar_graficos_performance(csv_path='metricas_teste.csv'):
    try:
        df = pd.read_csv(csv_path)
    except Exception as e:
        print(f"Erro ao ler o arquivo CSV: {e}")
        return

    # Ajustado o layout para 3 gráficos (1 linha com 2 e 1 linha centralizada/expandida abaixo)
    fig, axes = plt.subplots(3, 1, figsize=(14, 16))
    fig.suptitle('Análise de Performance do Servidor TCP (Stress Test)', fontsize=20, fontweight='bold', color='#1e293b', y=0.98)

    palette = sns.color_palette("Set1", len(df['tamanho_alvo_bytes'].unique()))

    # Gráfico 1: Throughput Msg/s
    sns.lineplot(
        data=df, 
        x='clientes_ativos', 
        y=df['total_mensagens'] / 3.0, 
        hue='tamanho_alvo_bytes', 
        marker='o', 
        linewidth=2.5, 
        palette=palette, 
        ax=axes[0]
    )
    axes[0].set_title('Throughput: Mensagens por Segundo (Msg/s)', fontsize=14, fontweight='bold')
    axes[0].set_xlabel('Quantidade de Clientes Ativos', fontsize=11)
    axes[0].set_ylabel('Mensagens / segundo', fontsize=11)
    axes[0].legend(title='Tamanho Msg (Bytes)', loc='upper left')

    # Gráfico 2: Throughput MB/s
    df['mb_per_sec'] = (df['total_bytes'] / (1024 * 1024)) / 3.0
    sns.lineplot(
        data=df, 
        x='clientes_ativos', 
        y='mb_per_sec', 
        hue='tamanho_alvo_bytes', 
        marker='s', 
        linewidth=2.5, 
        palette=palette, 
        ax=axes[1]
    )
    axes[1].set_title('Vazão de Dados (MB/s)', fontsize=14, fontweight='bold')
    axes[1].set_xlabel('Quantidade de Clientes Ativos', fontsize=11)
    axes[1].set_ylabel('Throughput (MB/s)', fontsize=11)
    axes[1].legend(title='Tamanho Msg (Bytes)', loc='upper left')

    # Gráfico 3: Total de Mensagens Processadas (Barplot)
    sns.barplot(
        data=df, 
        x='clientes_ativos', 
        y='total_mensagens', 
        hue='tamanho_alvo_bytes', 
        palette=palette, 
        ax=axes[2]
    )
    axes[2].set_title('Total de Mensagens Processadas (Janela 3s)', fontsize=14, fontweight='bold')
    axes[2].set_xlabel('Quantidade de Clientes Ativos', fontsize=11)
    axes[2].set_ylabel('Total Mensagens', fontsize=11)
    axes[2].legend(title='Tamanho Msg (Bytes)', loc='upper left')

    plt.tight_layout(rect=[0, 0, 1, 0.97])
    
    output_filename = 'graficos_performance.png'
    plt.savefig(output_filename, dpi=300, bbox_inches='tight')

if __name__ == '__main__':
    gerar_graficos_performance()