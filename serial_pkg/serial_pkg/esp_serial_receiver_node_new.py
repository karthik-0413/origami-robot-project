import rclpy
from rclpy.node import Node
from sensor_msgs.msg import Imu
from std_msgs.msg import Int32MultiArray
import serial

def get_imu_covariance_matrix():
    g_cov = [
        0.001, 0.0, 0.0,
        0.0, 0.001, 0.0,
        0.0, 0.0, 0.001
    ]
    a_cov = [
        0.01, 0.0, 0.0,
        0.0, 0.01, 0.0,
        0.0, 0.0, 0.01
    ]
    o_cov = [
        -1.0, 0.0, 0.0,
        0.0, 0.0, 0.0,
        0.0, 0.0, 0.0
    ]
    
    return o_cov, g_cov, a_cov

class SerialListener(Node):
    def __init__(self):
        super().__init__("serial_listener")

        # Parameters
        self.declare_parameter("port", "/dev/ttyACM0")
        self.declare_parameter("baud", 115200)

        port = self.get_parameter("port").value
        baud = self.get_parameter("baud").value
        
        # Create topic publishers
        self.ir_pub = self.create_publisher(Int32MultiArray, '/ir_sensors', 10)
        self.hinge_pub = self.create_publisher(Int32MultiArray, '/hinge_angles', 10)
        
        self.ir_data = [0]*6
        self.hinge_data = [0]*2
        
        self.last_ir_data = [None]*6
        self.last_hinge_data = [None]*2

        # Open serial port
        try:
            self.ser = serial.Serial(port, baud, timeout=0.1)
            self.get_logger().info(f"Opened serial port: {port} @ {baud}")
        except Exception as e:
            self.get_logger().error(f"Failed to open serial port: {e}")
            raise SystemExit

        # Timer to check for messages
        self.timer = self.create_timer(0.01, self.read_serial)

        self.buffer = ""

    def read_serial(self):
        """
        Reads incoming bytes and looks for a full message ending with 'END'.
        """
        try:
            data = self.ser.read(128)  # read up to 128 bytes
            if not data:
                return

            self.buffer += data.decode(errors="ignore")

            # Process full messages
            while "END" in self.buffer:
                msg, self.buffer = self.buffer.split("END", 1)
                self.get_logger().debug(f"Received message: {msg}")
                msg = msg.strip().strip(",")
                self.process_message(msg)

        except Exception as e:
            self.get_logger().error(f"Serial read error: {e}")

    def process_message(self, msg: str):
        """
        Parses the comma-separated message into the correct types.
        """
        if not msg:
            self.get_logger("no message received")
            return

        parts = msg.split(",")

        if len(parts) != 1 + 6 + 2:   
            # 1 timestamp + 6 IR + 2 hinge
            self.get_logger().warn(f"Invalid message length: {len(parts)} | {msg}")
            return

        try:
            idx = 0

            # Timestamp sec:ms:us
            msg_timestamp = parts[idx]; idx += 1

            # IR1, IR2, IR3, IR4, IR5, IR6
            self.ir_data = [int(p) for p in parts[idx:idx+6]]
            idx += 6

            # Hinge1, Hinge2
            self.hinge_data = [float(p) for p in parts[idx:idx+2]]
            self.hinge_data = [int(hp) for hp in self.hinge_data]
            
            # Check if data has changed
            if self.ir_data == self.last_ir_data and \
               self.hinge_data == self.last_hinge_data:
                return  # No change, skip processing

            # LOG the parsed values (you can publish here)
            self.get_logger().info(
                f"Timestamp={msg_timestamp} | IR_sensors={self.ir_data} | hinge_positions={self.hinge_data}"
            )

        except Exception as e:
            self.get_logger().error(f"Parse error: {e} | original: {msg}")
            return
        
        ir_msg = Int32MultiArray()
        hinge_msg = Int32MultiArray()
        
        try:
            # Load IR sensor data
            ir_msg.data = self.ir_data
            
            # Load hinge position data
            hinge_msg.data = self.hinge_data
            
        except Exception as e:
            self.get_logger().error(f"Message load error: {e}")
            return
        
        # Publish messages
        try:
            self.ir_pub.publish(ir_msg)
            self.hinge_pub.publish(hinge_msg)
        except Exception as e:
            self.get_logger().error(f"Publish error: {e}")
            return
        

def main(args=None):
    rclpy.init(args=args)
    node = SerialListener()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()


if __name__ == "__main__":
    main()
