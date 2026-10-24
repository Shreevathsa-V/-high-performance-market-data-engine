import socket
import struct
import time

HOST = "127.0.0.1"
PORT = 9000


def create_new_order(order_id, side, price, quantity):
    # Format:
    # 1 byte message type
    # 2 bytes message length
    # 8 bytes order ID
    # 1 byte side
    # 1 byte order type
    # 8 bytes signed price
    # 8 bytes quantity
    return struct.pack(
        "!BHQBBqQ",
        1,
        29,
        order_id,
        side,
        0,          # GoodTillCancel
        price,
        quantity,
    )


def create_cancel_order(order_id):
    # Format:
    # 1 byte message type
    # 2 bytes message length
    # 8 bytes order ID
    return struct.pack(
        "!BHQ",
        2,
        11,
        order_id,
    )


def main():
    order_id = 9001

    with socket.create_connection(
        (HOST, PORT),
        timeout=5,
    ) as client:

        client.settimeout(5)

        print("Connected to C++ TCP server")

        order_message = create_new_order(
            order_id=order_id,
            side=0,       # Buy
            price=10000,
            quantity=25,
        )

        # Deliberately split the message to test TCP
        # stream reassembly.
        client.sendall(order_message[:10])

        print("Sent first 10 bytes of new-order message")

        time.sleep(0.1)

        client.sendall(order_message[10:])

        print("Sent remaining 19 bytes of new-order message")

        time.sleep(0.1)

        cancel_message = create_cancel_order(order_id)

        client.sendall(cancel_message)

        print("Sent cancellation message")

        time.sleep(0.1)

        print("Closing client connection")


if __name__ == "__main__":
    main()