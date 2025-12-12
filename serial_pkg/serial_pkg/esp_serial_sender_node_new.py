import rclpy
from rclpy.node import Node
from std_msgs.msg import Int32, Int32MultiArray
from geometry_msgs.msg import TwistStamped
import serial


class SerialSenderNode(Node):
    def __init__(self):
        super().__init__("serial_sender_node")

        # Parameters for serial port
        self.declare_parameter("port", "/dev/ttyACM0")
        self.declare_parameter("baud", 115200)

        port = self.get_parameter("port").get_parameter_value().string_value
        baud = self.get_parameter("baud").get_parameter_value().integer_value

        # Open serial
        try:
            self.ser = serial.Serial(port, baud, timeout=0.01)
            self.get_logger().info(f"Opened serial port {port} @ {baud}")
        except Exception as e:
            self.get_logger().error(f"Could not open serial port: {e}")
            raise e

        # Save start time
        self.start_time = self.get_clock().now().to_msg()
        self.get_logger().info(f"Start time recorded: {self.start_time}")
        self.prev_time_stamp = 0

        # Internal state
        self.drive_state = 0
        self.turn_state = 0
        self.hinge_drive_state = [0, 0]
        
        self.last_drive_state = None
        self.last_turn_state = None
        self.last_hinge_drive_state = [None, None]

        # Subscribers
        self.create_subscription(Int32, "/drive_state", self.drive_state_callback, 10)
        self.create_subscription(Int32, "/turn_state", self.turn_state_callback, 10)
        self.create_subscription(Int32MultiArray, "/hinge_drive_state", self.hinge_drive_state_callback, 10)

        # Timer to evaluate and send out serial data
        self.timer = self.create_timer(0.001, self.loop)

    # ---------------------------- Callbacks ----------------------------
    def drive_state_callback(self, msg: Int32):
        self.drive_state = msg.data
    
    def turn_state_callback(self, msg: Int32):
        self.turn_state = msg.data
        
    def hinge_drive_state_callback(self, msg: Int32MultiArray):
        if len(msg.data) >= 2:
            self.hinge_drive_state = [msg.data[0], msg.data[1]]

    # ---------------------------- Main Loop ----------------------------

    def loop(self):
        
        # Check if any state has changed
        if (self.drive_state == self.last_drive_state) and \
           (self.turn_state == self.last_turn_state) and \
           (self.hinge_drive_state[0] == self.last_hinge_drive_state[0]) and \
           (self.hinge_drive_state[1] == self.last_hinge_drive_state[1]):
            return
        
        # Compute timestamp difference
        now = self.get_clock().now().to_msg()
        start = self.start_time

        # Convert both to nanoseconds for convenience
        now_ns = now.sec * 1_000_000_000 + now.nanosec
        start_ns = start.sec * 1_000_000_000 + start.nanosec
        delta_ns = now_ns - start_ns
        if delta_ns < 0:
            delta_ns = 0

        # Convert to sec:ms:us
        sec = delta_ns // 1_000_000_000
        remainder = delta_ns % 1_000_000_000

        ms = remainder // 1_000_000
        us = (remainder % 1_000_000) // 1_000

        timestamp_str = f"{sec}:{ms:03d}:{us:03d}"

        # Get drive and turn states
        linX = str(self.drive_state)
        AngZ = str(self.turn_state)
        
        # Extract hinges
        hinge1 = str(self.hinge_drive_state[0])
        hinge2 = str(self.hinge_drive_state[1])

        # Build final message
        msg = (
            f"{timestamp_str},"
            f"{linX},"
            f"{AngZ},"
            f"{hinge1},{hinge2},END"
        )

        # Write to serial
        try:
            self.ser.write((msg + "\n").encode())
        except Exception as e:
            self.get_logger().error(f"Serial write failed: {e}")
            
        self.last_drive_state = self.drive_state
        self.last_turn_state = self.turn_state
        self.last_hinge_drive_state = self.hinge_drive_state.copy()

        # Debug log (throttled)
        self.get_logger().info(f"TX: {msg}", throttle_duration_sec=0.3)


# ---------------------------------------------------------------------

def main(args=None):
    rclpy.init(args=args)
    node = SerialSenderNode()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()


if __name__ == "__main__":
    main()
