# TRABALHO LABORATORIAL Nº 3: Compressão de Parâmetros de LLM (Qwen2-0.5B)

**Ficheiro:** `model.safetensors` (942.32 MB, 494 milhões de parâmetros)

**Objetivo:** Encontrar a combinação ótima de Taxa de Compressão (CR) e Tempo de Processamento.

---

## 1. Instruções de Execução


1.  `gzip_benchmarking.c`: Calcula a Taxa de Compressão (CR) e gere os ficheiros.
2.  `run_tests.sh`: Utiliza o comando nativo `time` para medir o tempo de processamento de forma fiável.

### 1.1. Pré-requisitos

1.  **Ficheiro de Dados:** `model.safetensors` no diretório raiz.
2.  **Utilitários:** Compilador `gcc`, `gzip` e `time`.

### 1.2. Passos de Execução

1.  **Compilação do Código C:**
    ```bash
    gcc gzip_benchmarking.c -o gzip_benchmarking
    ```

2.  **Execução dos Testes:**
    O script irá correr os testes Gzip nos níveis -1 (Velocidade) e -9 (Taxa Máxima).

    ```bash
    ./run_tests.sh
    ```


---
