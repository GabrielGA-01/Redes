
#!/bin/bash

set -Eeuo pipefail

PORT=5000
HOST="127.0.0.1"
DURATION=10
MAX_CLIENTS=10
REPEATS=3

SERVER_PID=""

cleanup() {
    if [[ -n "$SERVER_PID" ]]; then
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
}

trap cleanup EXIT INT TERM

echo "======================================"
echo " COMPILING"
echo "======================================"

gcc -Wall -Wextra -O2 -std=c11 \
    chat_continuous_server.c \
    -o chat_continuous_server_exec -pthread

gcc -Wall -Wextra -O2 -std=c11 \
    chat_continuous_client.c \
    -o chat_continuous_client_exec -pthread

echo "Compilation successful!"

echo "======================================"
echo " STARTING SERVER"
echo "======================================"

./chat_continuous_server_exec "$PORT" &
SERVER_PID=$!

sleep 1

if ! kill -0 "$SERVER_PID" 2>/dev/null; then
    echo "Server failed to start."
    exit 1
fi

echo "Server PID: $SERVER_PID"

echo "======================================"
echo " RUNNING BENCHMARK"
echo "======================================"


for clients in $(seq 1 "$MAX_CLIENTS"); do

    for repeat in $(seq 1 "$REPEATS"); do

        echo ""
        echo "--------------------------------------"
        echo "Clients: $clients"
        echo "Repetition: $repeat/$REPEATS"
        echo "--------------------------------------"

        PIDS=()

        for i in $(seq 1 "$clients"); do
            ./chat_continuous_client_exec \
                "$HOST" "$PORT" "$DURATION" &

            PIDS+=("$!")
            sleep 0.1
        done

        FAILED=0

        for pid in "${PIDS[@]}"; do
            if ! wait "$pid"; then
                FAILED=1
            fi
        done

        if [[ "$FAILED" -ne 0 ]]; then
            echo "Warning: a client exited with an error."
        fi

        echo "Test completed."
    done
done


echo ""
echo "======================================"
echo " WAITING FOR SERVER"
echo "======================================"

wait "$SERVER_PID"
SERVER_PID=""

echo "======================================"
echo " GENERATING GRAPH"
echo "======================================"

python3 - <<'PY'
import csv
import os

try:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
except ImportError:
    print("Matplotlib is missing.")
    print("Install it with: python3 -m pip install matplotlib")
    raise SystemExit(1)

data = {}

with open("metricas_teste.csv", newline="") as f:
    reader = csv.DictReader(f)

    for row in reader:
        clients = int(row["clientes"])
        mbps = float(row["Mbps"])
        data.setdefault(clients, []).append(mbps)

x = sorted(data)
averages = [
    sum(data[n]) / len(data[n])
    for n in x
]

plt.figure(figsize=(10, 6))

plt.plot(x, averages, marker="o", linewidth=2)

plt.title("TCP Aggregate Throughput")
plt.xlabel("Number of simultaneous clients")
plt.ylabel("Aggregate throughput (Mbps)")
plt.xticks(x)
plt.grid(True, linestyle="--", alpha=0.6)

plt.tight_layout()
plt.savefig("graficos_performance.png", dpi=300)

print("Graph saved: graficos_performance.png")

print("\nAverage throughput:")
for n, value in zip(x, averages):
    print(f"{n} clients: {value:.3f} Mbps")
PY

echo ""
echo "======================================"
echo " BENCHMARK FINISHED"
echo "======================================"

echo "Generated files:"
echo " - metricas_teste.csv"
echo " - graficos_performance.png"
