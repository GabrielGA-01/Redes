# Projeto de Redes

## Código original

### 1. Acesse a pasta

```bash
cd old
```

### 2. Compile o servidor e o cliente

```bash
gcc -Wall -Wextra -g udp_client.c -o chat_server -pthread
gcc -Wall -Wextra -g -x c chat_example -o chat_client -pthread
```

> A compilação do cliente pode exibir um aviso (*warning*).

### 3. Execute

Inicie o servidor:

```bash
./chat_server 5000
```

Em outro terminal, execute um ou mais clientes:

```bash
cd old
./chat_client 127.0.0.1 5000
```

## Código modificado

### 1. Acesse a pasta

```bash
cd src
```

### 2. Compile e execute o teste

#### Forma automática

```bash
./execute.sh
```

#### Forma manual

Compile o servidor e o cliente:

```bash
gcc -Wall -Wextra -g -x c chat_continuous_server.c -o chat_continuous_server_exec -pthread
gcc -Wall -Wextra -g -x c chat_continuous_client.c -o chat_continuous_client_exec -pthread
```

Execute o servidor com o teste:

```bash
./chat_continuous_server_exec 5000
```

### 3. Instale as dependências Python

```bash
pip install -r requirements.txt
```

### 4. Gere os gráficos

```bash
python charts.py
```
