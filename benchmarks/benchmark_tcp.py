import socket
import struct
import time

HOST = "127.0.0.1"
PORT = 9000

NUM_ORDERS = 10_000


def create_new_order(order_id):
    return struct.pack(
        "!BHQBBqQ",
        1,          # NewOrder
        29,         # Message length
        order_id,
        0,          # Buy
        0,          # GoodTillCancel
        10000,      # Price
        1,          # Quantity
    )


def create_cancel_order(order_id):
    return struct.pack(
        "!BHQ",
        2,          # CancelOrder
        11,         # Message length
        order_id,
    )


def main():
    messages = bytearray()

    for i in range(NUM_ORDERS):
        order_id = 1_000_000 + i

        messages.extend(create_new_order(order_id))
        messages.extend(create_cancel_order(order_id))

    total_messages = NUM_ORDERS * 2
    total_bytes = len(messages)

    print(f"Orders to submit: {NUM_ORDERS:,}")
    print(f"Messages to process: {total_messages:,}")
    print(f"Payload size: {total_bytes:,} bytes")

    start = time.perf_counter()

    with socket.create_connection(
        (HOST, PORT),
        timeout=30,
    ) as client:

        client.settimeout(60)

        # Send the entire batch through one sendall call.
        client.sendall(messages)

        # Signal that no more bytes will be sent.
        client.shutdown(socket.SHUT_WR)

        # Wait until the server closes the connection.
        # This ensures the server has finished reading
        # and processing the submitted stream.
        while client.recv(4096):
            pass

    elapsed = time.perf_counter() - start

    print("\nTCP benchmark complete")
    print(f"Elapsed time: {elapsed:.6f} seconds")
    print(
        "End-to-end message rate: "
        f"{total_messages / elapsed:,.0f} messages/second"
    )
    print(
        "Payload throughput: "
        f"{total_bytes / elapsed / 1_000_000:.2f} MB/s"
    )


if __name__ == "__main__":
    main()