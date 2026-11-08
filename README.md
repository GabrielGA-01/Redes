# Para compilar os código
gcc -Wall -Wextra -g udp_client.c -o chat_server -pthread

Existe um warning ao executar o comando abaixo
gcc -Wall -Wextra -g -x c chat_example -o chat_client -pthread

# Para executar o servidor
./chat_server 5000

# Para executar o cliente
./chat_client 127.0.0.1 5000 