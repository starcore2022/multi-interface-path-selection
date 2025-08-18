from mimetypes import init
import traceback
import time
import rclpy
from rclpy.node import Node
from rclpy.executors import MultiThreadedExecutor

from std_msgs.msg import String
from unique_flow_topic_relay.msg import UniqueTopicFlow
from unique_flow_topic_relay.msg import UniqueTopicFlowStats


from .bandwidth_and_frequency_measurement import ROSTopicMeasure

from ros2cli.node.direct import DirectNode
from ros2topic.api import get_msg_class
from rclpy.qos import qos_profile_sensor_data

from .QoS_mapper import QoSmapper
from .ros2qos_parser import Ros2QoS_parser

from .SEAL import SEAL
from .SEAL_state_machine import SEAL_state_machine

from rclpy.context import Context

local_test = 1
active_topics = {}
ros2_domain_id = 0
seal_sm = None

class MinimalSubscriber(Node):

    def __init__(self):
        super().__init__('minimal_subscriber')
        self.subscription = self.create_subscription(
            UniqueTopicFlow,
            'unique_topic_flow',
            self.listener_callback,
            10)
        self.subscription  # prevent unused variable warning

        self.subscription_stat = self.create_subscription(
            UniqueTopicFlowStats,
            'unique_topic_flow_stats',
            self.listener_callback_stats,
            10)
        self.subscription_stat  # prevent unused variable warning

        self.networkexposure = SEAL()
        
        global ros2_domain_id
        global seal_sm
        seal_sm = SEAL_state_machine("seal", self.networkexposure, ros2_domain_id)
        print("SEAL state current: " + seal_sm.state)
        while seal_sm.state != 'connection_in_group':
            print("SEAL state before: " + seal_sm.state)
            # get to the state when connection is ready
            if seal_sm.state == 'group_created':
                seal_sm.add_connection_to_group()
            if seal_sm.state == 'no_group':
                seal_sm.create_group()
            if seal_sm.state == 'no_auth':
                seal_sm.get_auth_token()
            print("SEAL state after: " + seal_sm.state)

    def listener_callback(self, msg):
        self.get_logger().info('I heard: "%s"' % msg.topic)
        print("event_type: ")
        print(msg.event_type)

        global active_topics
        if msg.event_type == 1: #ADDED
            # src, dst, hz, bw
            active_topics[msg.topic] = (msg.source, msg.destination, 1, 1)

        if msg.event_type == 2: #REMOVED
            try:
                active_topics.pop(msg.topic)
            except KeyError:
                pass

        print(active_topics)


    def listener_callback_stats(self, msg):

        for i in msg.stats:
            self.get_logger().info('I heard stats: "%s"' % i.topic)
            global active_topics
            res = active_topics.get(i.topic)
            if res is not None:
                print("window rate: " + str(i.window_rate))
                src = res[0]
                dst = res[1]
                current_rate = res[2]
                current_bw = res[3]
                print("src: "+ str(src))
                print("dst: "+ str(dst))
                print("current_rate: "+ str(current_rate))
                print("current_bw: "+ str(current_bw))

                pub_info_by_pub = self.get_publishers_info_by_topic(i.topic)
                for publisher in pub_info_by_pub:
                    qos_profile = publisher._qos_profile
                    print(qos_profile)
                    print("reliability: " + str(qos_profile.reliability))
                    ros2qos_parser = Ros2QoS_parser(qos_profile)
                    PDB_Ros2, PELR_Ros2 = ros2qos_parser.parseQoS(qos_profile)
                        
                    print("PDB_Ros2: "+ str(PDB_Ros2))
                    print("PELR_Ros2: "+ str(PELR_Ros2))
                    qos_mapper = QoSmapper()
                    RT = None
                    PL = 2

                    # Resource Type (RT) (GBR/NON-GBR)
                    # Priority Level (PL)
                    # Packet Delay Budget (PDB)
                    # Packet Error Loss Rate (PELR)
                    # Maximum Data Burst Volume (MDBV)
                    # Data Rate Averaging Window (DRAW)
                    # calculate ms from Hz
                    PDB_measured = 1. / current_rate * 10e3
                    print("PDB_measured: "+ str(PDB_measured))
                    if PDB_measured > PDB_Ros2:
                        print("WARNING: ROS2 lifespan setting is too low")
                    
                    if abs(current_rate/i.window_rate-1) < 0.1:
                        RT = "GBR"
                    else:
                        RT = "Non-GBR"
                    print("RT: "+ RT)

                    DRAW = 2000

                    qos_mapper_result = qos_mapper.getQCI(RT, PL, PDB_measured, PELR_Ros2, current_bw, DRAW)

                    qci, example_service = qos_mapper_result
                    if qci is None:
                        qci = -1
                    if example_service is None:
                        example_service = "no proper service found"
                    print("QCI:" + str(qci))
                    print("Example service: " + example_service)
                        
                    uplinkMaxBitRate = max(current_bw, i.window_bandwidth)

                    sourceIPv4 = str(src.address[12]) + "." + str(src.address[13]) + "." + str(src.address[14]) + "." + str(src.address[15])
                    sourcePort = str(src.port)
                    destinationIPv4 = str(dst.address[12]) + "." + str(dst.address[13]) + "." + str(dst.address[14]) + "." + str(dst.address[15])
                    destinationPort = str(dst.port)
                    #currently one initialization of the unicast resource is possible, if there is unicast patch and put calls then this could be done frequently
                    global seal_sm
                    if seal_sm.state == 'connection_in_group':
                        seal_sm.nrm_posted()
                        #ugly, the call should be in the state machine
                        self.networkexposure.NetworkResourceAdaptation("GROUP_ID", qci, uplinkMaxBitRate, 10, destinationIPv4, destinationPort, sourcePort, "UDP", "bidirectional")

                    active_topics[i.topic] = (res[0], res[1], i.window_rate, i.window_bandwidth)


def main(args=None):
    rclpy.init(args=args)
    ros2_domain_id = rclpy.get_default_context().get_domain_id()
    print("domain id: " + str(ros2_domain_id))

    minimal_subscriber = MinimalSubscriber()
    rclpy.spin(minimal_subscriber)

    print("Step 1")
    minimal_subscriber.destroy_node()
    rclpy.shutdown()


if __name__ == '__main__':
    main()
