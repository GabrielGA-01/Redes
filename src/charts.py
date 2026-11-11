import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns

plt.style.use(
    'seaborn-v0_8-darkgrid'
    if 'seaborn-v0_8-darkgrid' in plt.style.available
    else 'default'
)
sns.set_theme(style="darkgrid")
plt.rcParams['font.sans-serif'] = 'DejaVu Sans'
plt.rcParams['font.family'] = 'sans-serif'


def gerar_graficos_performance(csv_path='metricas_teste.csv'):
    df = pd.read_csv(csv_path)

    required = {
        'tamanho_alvo_bytes', 'clientes_ativos', 'total_mensagens',
        'total_bytes', 'duracao_segundos'
    }
    missing = required.difference(df.columns)
    if missing:
        raise ValueError(
            'CSV sem as colunas necessárias: ' + ', '.join(sorted(missing))
        )

    # Usa a duração real registrada pelo coordenador, não uma janela fixa de 3 s.
    df['duracao_segundos'] = pd.to_numeric(df['duracao_segundos'], errors='coerce')
    df = df[df['duracao_segundos'] > 0].copy()
    if df.empty:
        raise ValueError('O CSV não contém durações válidas maiores que zero.')

    df['mensagens_por_segundo'] = df['total_mensagens'] / df['duracao_segundos']
    df['mb_per_sec'] = (
        df['total_bytes'] / (1024 * 1024) / df['duracao_segundos']
    )

    fig, axes = plt.subplots(3, 1, figsize=(12, 18))

    sns.lineplot(
        data=df, x='clientes_ativos', y='mensagens_por_segundo',
        hue='tamanho_alvo_bytes', marker='o', ax=axes[0]
    )
    axes[0].set_title('Vazão de Mensagens por Segundo')
    axes[0].set_xlabel('Clientes ativos')
    axes[0].set_ylabel('Mensagens por segundo')

    sns.lineplot(
        data=df, x='clientes_ativos', y='mb_per_sec',
        hue='tamanho_alvo_bytes', marker='o', ax=axes[1]
    )
    axes[1].set_title('Vazão de Dados (MB/s)')
    axes[1].set_xlabel('Clientes ativos')
    axes[1].set_ylabel('MB/s')

    sns.barplot(
        data=df, x='clientes_ativos', y='total_mensagens',
        hue='tamanho_alvo_bytes', ax=axes[2]
    )
    axes[2].set_title('Total de Mensagens Processadas por Teste')
    axes[2].set_xlabel('Clientes ativos')
    axes[2].set_ylabel('Total de mensagens')

    for ax in axes:
        ax.grid(True, alpha=0.3)

    plt.tight_layout()
    plt.savefig('graficos_performance.png', dpi=300, bbox_inches='tight')
    plt.close(fig)
    print('Gráficos salvos em: graficos_performance.png')


if __name__ == '__main__':
    gerar_graficos_performance()
