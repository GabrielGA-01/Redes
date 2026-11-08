gcc -Wall -Wextra -g -x c chat_continuous_server.c -o chat_continuous_server_exec -pthread
gcc -Wall -Wextra -g -x c chat_continuous_client.c -o chat_continuous_client_exec -pthread

./chat_continuous_server_exec 5000
python charts.py