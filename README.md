# Código original
gcc -Wall -Wextra -g udp_client.c -o chat_server -pthread

Existe um warning ao executar o comando abaixo
gcc -Wall -Wextra -g -x c chat_example -o chat_client -pthread

./chat_server 5000
./chat_client 127.0.0.1 5000 

# Código modificado 
gcc -Wall -Wextra -g -x c chat_continuous_server.c -o chat_continuous_server_exec -pthread
gcc -Wall -Wextra -g -x c chat_continuous_client.c -o chat_continuous_client_exec -pthread

./chat_continuous_server_exec 5000

# Geração de gráficos
Utiliza Python. Instale as depedências com:
pip install -r requirements.txt