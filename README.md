# Table of Contents
1. [Introduction](#introduction)
2. [Usage](#usage)
    - [ROS2 TCP Tunnel](#ros2-tcp-tunnel)
    - [ROS2 multi interface 3GPP SA6 Mapper](#ros2-multi-interface-3gpp-sa6-mapper)
    - [Automatic path switching](#automatic-path-switching)
3. [Running codes in the expected order](#running-codes-in-the-expected-order)
4. [Demos](#demos)


READMEs:
- [ROS2 TCP Tunnel](<ros2_tcp_tunnel_multi_interface/README.md>)
- [ROS2 multi interface 3GPP SA6 Mapper](<ros2_multi_interface_3gpp_sa6_mapper/README.md>)
- [ROS2 unique_flow_topic_relay with multiple interfaces](<ros2_unique_flow_topic_relay/README.md>)

# Introduction
This branch explores the possibility of enhancing the ROS2 unique flow topic relay with multiple interfaces through ROS2 TCP tunneling. It builds on the potential explored in the main branch, which made use of 3GPP TS 23.434 SEAL (Service Enabler Architecture Layer). The proposed solution involves using an enhanced version of ROS2 TCP Tunnel server-client communicating through TCP, with describeable interfaces. The ROS2 TCP Tunnel server subscribes to the demultiplexed topics and relays them to the ROS2 TCP Tunnel client. The unique flow topic relay endpoint was modified to subscribe to the topics tunneled through TCP instead of the unique flow topic relay.

This all was built with ROS2 Humble in mind and was tested on Ubuntu 22.04 LTS operating system.

# Usage

Before running any code the user has to make sure that the firewall doesn't block that communication. 
This can be done, by allowing the necessary ports for ROS2 communication, or allowing all messages from certain IP addresses through the firewall. 
It is not recommended to disable the firewall entirely.

In Linux this is as simple as:

```
sudo ufw allow from <IP_ADDRESS> to any port <PORT>
```

The user has to make sure, that both side allow communication for ROS2 and/or ROS2 TCP Tunnel.
For testing, all messages from both device's IPs were allowed.
Also make sure that the correct branch is selected, before running the unique flow topic relay.
The ROS2 TCP Tunnel server-client and ROS2 TCP unique flow topic relay can be started in any order, but it is advised to first start the publisher side, then the ROS2 TCP Tunnel client, before the relay endpoint.
The AddTopic service can be called after launching the relay endpoint, but it is expected to called before the endpoint relay tries to subscribe to the topics tunneled by the tunnel client.




## ROS2 TCP Tunnel

Commands to use in two different machines.

On all machines and before running the TCP Tunnel server or client don't forget to source the necessary ROS2 environment.
Sourcing the unique flow relay is advised to be source for the TCP Tunnel server(s) too. This is the second sourcing command.

```
source /opt/ros/humble/setup.bash 
source ../ros2-3gppSA6-mapper/install/setup.bash
source install/setup.bash 
```

### PC1, or the publisher side

The format for running the ROS2 TCP Tunnel server is: 

```
ros2 run tcp_tunnel ros2_tcp_tunnel_service_server /<topic-name> <interface-name>  --ros-args -r __ns:=/<namespace> 
```
Here the `<interface-name>` is the name of the network interface to be used for the TCP tunnel, which can be found using the `ip a` command.
The interface can be the full or a shortened version, but it is advised to use the full version or a shorter version that is unique.
The `<namespace>` is the ROS2 namespace to be used for the TCP tunnel and must start with '/'. 
For running multiple servers and/or clients, it is necessary of neither of the same type to share the same namespace. 
The `/<topic-name>` specifies a default topic name to be used if the AddTopic service has no topic name in it, but this normally should not happen, because the AddTopic service throws an error if no topic name is provided.



#### Examples for running 2 servers with different interfaces:
```
ros2 run tcp_tunnel ros2_tcp_tunnel_service_server /pose_5G wlp  --ros-args -r __ns:=/pose_5G
```

```
ros2 run tcp_tunnel ros2_tcp_tunnel_service_server /pose_ETH enp  --ros-args -r __ns:=/pose_ETH
```

### PC2, or the subscriber side

The expected formats for the starting the ROS2 TCP Tunnel client is:

```
ros2 run tcp_tunnel ros2_tcp_tunnel_service_client /<topic-name> <interface-name>  --ros-args -r __ns:=/<namespace>
```

The name is similar to the publisher side seen [above](#pc1-or-the-publisher-side). The difference is the at instead of the server, the client application is ran by ROS2.
For tunneling topics the client has to register the topics it is interested in, which can be done in the following format, by calling the AddTopic service of the client:

```
ros2 service call /<client-namespace>/tcp_tunnel_client/add_topic tcp_tunnel/srv/AddTopic "topic:
  data: '/<topic-name>'
tunnel_queue_size:
  data: '2'
server_namespace:
  data: '/<namespace>'
interface:
  data: '<interface-name>'"
```
Here there are two different namespaces. The `<client-namespace>` is the namespace of the TCP Tunnel client, so that the appropriate client will register the topic to the appropriate server.
This server is `<namespace>` is the namespace of the server.

#### Examples for running 2 clients with different interfaces:
For the first tunnel and interface:
```
ros2 run tcp_tunnel ros2_tcp_tunnel_service_client /pose_5G wlp  --ros-args -r __ns:=/pose_5G
```
To add a topic to be tunneled through the TCP connection:
```
ros2 service call /pose_5G/tcp_tunnel_client/add_topic tcp_tunnel/srv/AddTopic "topic:
  data: '/pose_5G'
tunnel_queue_size:
  data: '2'
server_namespace:
  data: '/pose_5G'
interface:
  data: 'wlp'"
```

For the second tunnel and interface:
```
ros2 run tcp_tunnel ros2_tcp_tunnel_service_client /pose_ETH enp  --ros-args -r __ns:=/pose_ETH
#But if you don't have physical Ethernet port, but connected one with USB adapter, then
ros2 run tcp_tunnel ros2_tcp_tunnel_service_client /pose_ETH enx  --ros-args -r __ns:=/pose_ETH
#But always check what desired interface is named by the system
```


Examples for calling AddTopic for the client using ethernet interface: 
```
#Ethernet interface with regular eth0 format
ros2 service call /pose_ETH/tcp_tunnel_client/add_topic tcp_tunnel/srv/AddTopic "topic:
  data: '/pose_ETH'
tunnel_queue_size:
  data: '2'
server_namespace:
  data: '/pose_ETH'
interface:
  data: 'eth'"

#Ethernet interface with enp0s0 format
ros2 service call /pose_ETH/tcp_tunnel_client/add_topic tcp_tunnel/srv/AddTopic "topic:
  data: '/pose_5G'
tunnel_queue_size:
  data: '2'
server_namespace:
  data: '/pose_ETH'
interface:
  data: 'enp'"

#Ethernet interface USB dongle in enx0... format
ros2 service call /pose_ETH/tcp_tunnel_client/add_topic tcp_tunnel/srv/AddTopic "topic:
  data: '/pose_5G'
tunnel_queue_size:
  data: '2'
server_namespace:
  data: '/pose_ETH'
interface:
  data: 'enx'"
```
The code expect correct format, if does any, then the other possible formats are searched for. For example, if there is no eth0, then it tries to find it with 'enp', after that the 'enx' is used.

Example for calling the AddTopic service for the client using the WiFi interface:
```
ros2 service call /pose_5G/tcp_tunnel_client/add_topic tcp_tunnel/srv/AddTopic "topic:
  data: '/pose_5G'
tunnel_queue_size:
  data: '2'
server_namespace:
  data: '/pose_5G'
interface:
  data: 'wlp'"
```
Just like for the Ethernet interface, it is advised to check the correct WiFi (or wireless) interface name before running the application.

After this the system is ready for running the ROS2 unique flow topic relay endpoint. Before calling the AddTopic service the topic the server has to subscribe to, has to be available.
If the topic does not exist, then the server return error, that the topic is not available/has not been published yet.





## ROS2 multi interface 3GPP SA6 Mapper

First source the appropriate ROS2 environments:
```
source /opt/ros/humble/setup.bash 
source install/setup.bash 
```
After this you can run the unique flow topic relay and relay endpoint.

### PC1, or the publisher side

The relay uses Turtlesim for publisher and the topic that is demultiplexed is the /Pose topic of the /turtle1 namespace.
```
ros2 launch unique_flow_topic_relay test_topic_tcp_tunnel_server_turtlesim.launch.yaml
```


### PC2, or the subscriber side
```
ros2 launch unique_flow_topic_relay test_topic_tcp_tunnel_client_turtlesim.launch.yaml
```

Subscriber is node is not launched by the YAML file. For quick testing the topic can be echoed using:
```
ros2 topic echo /pose_fromuf
```

## Automatic Path switching

The topic watcher was modified to support automatic path switching based on ellapsed time or the current network conditions.
The demos show the automatic switching between different network interfaces based on time. It switches every 15 seconds interval.
The topic watcher has been expended with a minimal publisher, that independently from the collected unique flow statistics switches between path 0 and path 1.
There is an implementation for the automatic path switching based delays. This delay can be calculated through the window rate delta and switch if the specified threshold is reached.
Example: 1/window_rate_delta > 50 ms

Running the code from the seal/seal library.
```
python3 topic_watcher_path_selector_using_stats.py
```


# Running codes in the expected order

## PC1

Terminal 1:
```
ros2 launch unique_flow_topic_relay test_topic_tcp_tunnel_server_turtlesim.launch.yaml
```

Terminal 2:
```
ros2 run tcp_tunnel ros2_tcp_tunnel_service_server /pose_5G wlp  --ros-args -r __ns:=/pose_5G
```

Terminal 3:
```
ros2 run tcp_tunnel ros2_tcp_tunnel_service_server /pose_ETH enp  --ros-args -r __ns:=/pose_ETH
```


## PC2

Terminal 1:
```
ros2 run tcp_tunnel ros2_tcp_tunnel_service_client /pose_5G wlp  --ros-args -r __ns:=/pose_5G
```
Terminal 2:
```
ros2 run tcp_tunnel ros2_tcp_tunnel_service_client /pose_ETH enx  --ros-args -r __ns:=/pose_ETH
```

Terminal 3:
```
ros2 service call /pose_5G/tcp_tunnel_client/add_topic tcp_tunnel/srv/AddTopic "topic:
  data: '/pose_5G'
tunnel_queue_size:
  data: '2'
server_namespace:
  data: '/pose_5G'
interface:
  data: 'wlp'"
```

Terminal 4:
```
ros2 service call /pose_ETH/tcp_tunnel_client/add_topic tcp_tunnel/srv/AddTopic "topic:
  data: '/pose_ETH'
tunnel_queue_size:
  data: '2'
server_namespace:
  data: '/pose_ETH'
interface:
  data: 'enx'"
```

Terminal 5:
```
ros2 launch unique_flow_topic_relay test_topic_tcp_tunnel_client_turtlesim.launch.yaml
```

Terminal 6:
```
ros2 topic echo /pose_fromuf
```


## Automatic path switching either on PC1 or PC2

In the seal/seal library the following command can be used to start the automatic path switching:

```
python3 topic_watcher_path_selector_using_stats.py
```


# Demos

There are two demo videos, because the system was tested on two machines.

## ROS2 TCP Tunnel server side

[![A Solution for Multipath Channel Switching in ROS2 With 3GPP Integration](https://img.youtube.com/vi/g4lqaNn3j1Q/0.jpg)](https://youtu.be/g4lqaNn3j1Q "A Solution for Multipath Channel Switching in ROS2 With 3GPP Integration")




## ROS2 TCP Tunnel client side

[![A Solution for Multipath Channel Switching in ROS2 With 3GPP Integration cont.](https://img.youtube.com/vi/wyxSVCpNX_s/0.jpg)](https://youtu.be/wyxSVCpNX_s "A Solution for Multipath Channel Switching in ROS2 With 3GPP Integration cont.")



