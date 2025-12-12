import rclpy
from rclpy.node import Node
import serial
import sys
import time


class SerialEcho(Node):
    def __init__(self):
        super().__init__('serial_echo')

        # Declare parameter
        self.declare_parameter('port', '/dev/ttyUSB0')
        port = self.get_parameter('port').get_parameter_value().string_value

        # Open serial port
        try:
            self.ser = serial.Serial(port, 115200, timeout=1)
            time.sleep(2)   # give Arduino time to reset
            self.get_logger().info(f"Opened serial port: {port}")
        except serial.SerialException:
            self.get_logger().error(f"Failed to open port: {port}")
            sys.exit(1)

        self.run_console_loop()

    def run_console_loop(self):
        """
        Reads user input from terminal -> sends over serial ->
        prints the Arduino echo.
        """
        self.get_logger().info("Type text to send. Press CTRL+C to exit.\n")

        while rclpy.ok():
            try:
                user_text = input("> ")

                # Send to Arduino
                self.ser.write((user_text + "\n").encode('utf-8'))

                # Wait for echo
                response = self.ser.readline().decode('utf-8', errors='ignore').strip()
                if response:
                    print(f"[Echo] {response}")

            except KeyboardInterrupt:
                print("\nExiting...")
                break

        self.ser.close()


def main(args=None):
    rclpy.init(args=args)
    SerialEcho()
    rclpy.shutdown()


if __name__ == '__main__':
    main()
