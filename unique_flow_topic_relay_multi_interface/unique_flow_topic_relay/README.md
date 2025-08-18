# unique_flow_topic_relay

ROS2 package for relaying and tunneling ROS2 topics, that can get statistics, unique flow information about the topics, and can split and merge a topic into multiple tunnels with way to select between them.

Only supports fastdds middleware (it has unique flow support and  getting subscriber port information).

# Usage

## Get unique flow information and statistics for any topic

Needs only unique_flow_topic_relay node.

![Basic usage](/docs/basic.png)

Subscribes to one or multiple topics and republishes them to a topic where both the sending and receiving ip:port information are known and  publishes this information to a topic: unique_topic_flow.  It’s recommended that all relays publish to the same topic.

The node only publishes a topic if there is a subscriber for that topic.

The node also collects and publishes statistics on the subscribed topics. The following statistics are published on the unique_topic_flow_stats topic:

-   topic  Name of the topic
-   start  When it started publishing
-   num_messages  Number of messages since start
-   num_bytes  Sent bytes since start
-   window_duration_secs  Length of window in seconds
-   window_bandwidth  Bandwidth used during window
-   window_rate  Rate of messages in window
-   window_average_delta  Average time between messages in window
    

The default window size is 100 messages.

### Parameters of unique_flow_topic_relay:

-   discovery_period  How often new subscribers get discovered, and statistics published
-   topic_names  Optional list of topics, that the node subscribes to.
-   to_uf_name  This string is searched in topic names to find topics the node subscribes to.
-   uf_name  This string replaces to_uf_name in the topic names for the published topic name.
-   publish_localhost_addresses  Publish unique flows with localhost  addresses. Only useful if publisher and subscriber on the same host, eg. During development.
    

If topic_names is empty the node subscribes to all topics that have to_uf_name  in them.

### Demo and example:

After sourcing the workspace:

    ros2 launch unique_flow_topic_relay  test_topic.launch.yaml  
    ros2 topic echo /unique_topic_flow  
    ros2 topic echo /unique_topic_flow_stats  
    ros2 topic echo /chatter std_msgs/String

Runs a relay that subscribes to /chatter_touf and publishes /chatter.

This demo disables fastrtps’s shared memory optimalization, making it possible to run the demo on one host. For use cases where the publisher and subscriber are not on the same host, it can be enabled.  
See FASTRTPS_DEFAULT_PROFILES_FILE in launch.yaml.

## Throttle mode

Uses unique_flow_topic_relay.

![Throttle usage](/docs/throttle.png)

You can send Throttle messages to the relay’s /throttle topic, that can set 2 parameters of every subscribed and published topic.

-   drop_every_nth  Drop every nth message it receives.
-   message_delay  Delay received messages by this number.
    

This can be used to emulate latency and droprate of a network connection.

### Demo and example:

After sourcing the workspace:

    ros2 launch unique_flow_topic_relay  test_topic.launch.yaml  
    ros2 topic echo /chatter std_msgs/String  
    ros2 topic pub /throttle "{topic: /chatter, drop_every_nth: 2}"

This demonstrates, that after publishing the message to the /throttle topic the subscriber to the /chatter topic only gets every second message.

## Get extra statistics with a tunnel

Uses unique_flow_topic_relay and unique_flow_topic_relay_endpoint node.

![Tunnel usage](/docs/tunnel.png)

By setting the use_serialized_tunnel parameter of unique_flow_topic_relay it creates a tunnel for every subscribed topic. This tunnel is using serialized messages with extra information to make computing latency and droprate possible. Every message gets a sent timestamp and a sequence number.

The relay_endpoint also publishes unique_topic_flow_stats topic with 2 stats:

-   topic  Name of the topic
-   measured_latency  Latency of the topic
-   measured_droprate  Droprate of the topic
    
The endpoint doesn’t publish unique flow information.

The endpoint doesn’t subscribe to anything until it notices that some node subscribed to a topic it publishes.

### Parameters of unique_flow_topic_relay_endpoint:

-   discovery_period  How often new subscribers get discovered, and statistics published
-   topic_names  Optional list of topics, that the node publishes to.
-   from_uf_name  This string is searched in topic names to find topics the node publishes to.
 -   uf_name  This string replaces  from_uf_name in the topic names for the subscribed topic name.
    
If topic_names is empty the node publishes to all topics that have from_uf_name  in them.

### Demo and example:

After sourcing the workspace:

    ros2 launch unique_flow_topic_relay  test_topic_tunnel.launch.yaml  
    ros2 topic echo /unique_topic_flow_stats  
    ros2 topic echo /chatter_fromuf  std_msgs/String

Runs a relay that subscribes to /chatter_touf and publishes /chatter with Serialized messages, and it runs a relay_endpoint that subscribes to /chatter and publishes /chatter_fromuf.

This demo disables fastrtps’s shared memory optimalization, making it possible to run the demo on one host. For use cases where the publisher and subscriber are not on the same host, it can be enabled.  
See FASTRTPS_DEFAULT_PROFILES_FILE in launch.yaml.

## Use multiple tunnels per topic

Uses unique_flow_topic_relay and unique_flow_topic_relay_endpoint node.

![Tunnel usage](/docs/tunnel.png)

By setting the split_tunnel_suffixes parameter of unique_flow_topic_relay and relay_endpoint it creates multiple tunnels for every topic. The suffixes get appended to the topic names.

This is useful if you take the unique flow information for every tunnel and the tunnels get different QoS in the underlying network layer, independent of ROS2 QoS-s. The unique_flow_topic_relay subscribes to a /select topic that selects which tunnel of the topic the data goes through. This facilitates faster switching of network QoS-s.

### Demo and example:

After sourcing the workspace:

    ros2 launch unique_flow_topic_relay  test_topic_tunnel_split.launch.yaml  
    ros2 topic echo /unique_topic_flow  
    ros2 topic echo /chatter_fromuf  std_msgs/String  
    ros2 topic pub --once /select unique_flow_topic_relay/msg/TunnelSelect '{select: 1}'

Runs a relay that subscribes to /chatter_touf and publishes /chatter_1 and /chatter_2 with Serialized messages, and it runs a relay_endpoint that subscribes to /chatter_1 and /chatter_2 and publishes /chatter_fromuf.

This demo disables fastrtps’s shared memory optimalization, making it possible to run the demo on one host. For use cases where the publisher and subscriber are not on the same host, it can be enabled.  
See FASTRTPS_DEFAULT_PROFILES_FILE in launch.yaml.

# Description

The relay works by checking available topics every discovery_period (make_subscribe_unsubscribe_decisions()). It checks all topics ROS knows about if it's interested in it. Is it in topic_names, does it contain to_uf_name? If the topic is relevant and there is a publisher for it (try_discover_source()), it checks if it’s already relayed. If not or the topic has QoS changes, the node creates a RelayData  for that topic. The RelayData  has a Generic Subscriber and Publisher for the topic. The Publisher is created with RMW_UNIQUE_NETWORK_FLOW_ENDPOINTS_OPTIONALLY_REQUIRED.  The published topic name is generated by string replacing to_uf_name with uf_name. The node also registers the topic with Subscriber Discovery.  
After the topic checks are done, the node handles publishing the new unique flow messages (publish_unique_topic_flow_msgs()). The RelayData  has the Publisher for every relayed topic, so the source information is available. The topic was registered with Subscriber Discovery that returns changes since the last time it was called (get_subscriber_changes()). The changes contain the destination information. All information for the flow is available, so it is published. The source can contain 0.0.0.0 as the source ip, in this case a flow message is created for all network interface with an ip. These messages will have the same source port, destination ip and port, but the source ip will have the ips of the network interfaces.

The relay has the same callback for all Subscribers (process_message_and_measure()), but it has the RelayData as a parameter. The callback handles statistic collection, drop and delay handling and it also handles the tunnel functionality.

Tunneling is handled, that instead of publishing the same topic type it Subscribed, the message is packaged into a Serialized message as a byte\[] with extra information: sending time and sequence number. This allows the measurement of delay and droprate.

The relay endpoint is like the relay, but simplified as it only handles subscribing to Serialized messages, doesn’t throttle, and only collects delay and droprate statistics.  
It works by checking if there are subscribers for its configured topics (topic_names, from_uf_name) )(try_discover_drain()). If there are, it generates the topic name to subscribe to by replacing from_uf_name with uf_name. It subscribes to that Serialized topic and will create a Generic Publisher to publish the payload message  with the type it was subscribed to  at the start.  The subscription callback also handles computing statistics.

# Possible improvements

-   Only FastDDS Subscriber Discovery is implemented.
-   Possibly make statistics collecting optional.
-   Unique flow information is only published if there was a change (new topic, or topic stopped), make a Service that can be used to get the current state.
