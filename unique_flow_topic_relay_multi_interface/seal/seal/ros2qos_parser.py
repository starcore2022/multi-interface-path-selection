from rclpy.qos import QoSDurabilityPolicy, QoSHistoryPolicy, QoSReliabilityPolicy
from rclpy.qos import QoSProfile

class Ros2QoS_parser(object):

    def __init__(self, qos_profile):
        self.qos_profile = qos_profile

    def parseQoS(self, qos_profile):
        print("--- Parse QoS function ---")
        """Map QCI values from Resource Type (RT), Priority Level (PL), Packet Delay Budget (PDB), Packet Error Loss Rate (PELR), Maximum Data Burst Volume (MDBV), Data Rate Averaging Window (DRAW)"""
        PDB = 500
        PELR = 1e-2
        #History
        #    Keep last: only store up to N samples, configurable via the queue depth option.
        #    Keep all: store all samples, subject to the configured resource limits of the underlying middleware.

        #Depth
        #    Queue size: only honored if the “history” policy was set to “keep last”.

        #Reliability
        #    Best effort: attempt to deliver samples, but may lose them if the network is not robust.
        #    Reliable: guarantee that samples are delivered, may retry multiple times.

        # the DDS does this functionality by itself, should this be supported by the network by low PELR or switch on e.g., HARQ?
        if qos_profile.reliability is QoSReliabilityPolicy.BEST_EFFORT:
            print("Best effort")
        elif qos_profile.reliability is QoSReliabilityPolicy.RELIABLE:
            print("Reliable")
            PELR = 1e-6
        
        print("PELR " + str(PELR))


        #Durability
        #    Transient local: the publisher becomes responsible for persisting samples for “late-joining” subscriptions.
        #    Volatile: no attempt is made to persist samples.

        #Deadline
        #    Duration: the expected maximum amount of time between subsequent messages being published to a topic
        # NOTE: this is part of the profile, which might be not setup properly, it could be measured by Hz
        duration_miliseconds = qos_profile.deadline.nanoseconds / 10e9
        #if duration_miliseconds < rate ** 1e9 / 1000:
        #    print("WARNING: QoS profile duration is set lower than the publishing rate")
        # PDB = min(PDB, duration_miliseconds)
        # print("PDB " + str(PDB))

        #Lifespan
        #    Duration: the maximum amount of time between the publishing and the reception of a message without the message being considered stale or expired (expired messages are silently dropped and are effectively never received).
        lifespan_miliseconds = qos_profile.lifespan.nanoseconds / 10e9
        PDB = min(PDB, lifespan_miliseconds)
        print("PDB " + str(PDB))

        #Liveliness
        #    Automatic: the system will consider all of the node’s publishers to be alive for another “lease duration” when any one of its publishers has published a message.
        #    Manual by topic: the system will consider the publisher to be alive for another “lease duration” if it manually asserts that it is still alive (via a call to the publisher API).
        liveliness = qos_profile.liveliness

        #Lease Duration
        #    Duration: the maximum period of time a publisher has to indicate that it is alive before the system considers it to have lost liveliness (losing liveliness could be an indication of a failure).
        lease_duration_nanoseconds = qos_profile.liveliness_lease_duration

        print("--------- ros2qos_parser END ----------")
        return (PDB, PELR)