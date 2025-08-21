# Improvements made to use ROS2 TCP Tunnels and multiple interfaces for path selection

For familiarization with the system architecture and components, please refer to the original documentation [here](#original-documentation).
This part is about the changes and improvements made to the original implementation.

The original implementation made possible for ROS2 communication over TCP tunnels, but the user was not able to specify the interface. 
It was automatically chosen by the system during TCP socket creation.
This limitation was addressed by allowing the user to specify the desired network interface for the TCP socket by name, on both the server and client side. 
The AddTopic service was also improved by allowing the user to specify the interface for the topic it is interested in.



## Usage

How to start the server:
```bash
ros2 run tcp_tunnel ros2_tcp_tunnel_service_server /<default-topic-name> <interface-name>  --ros-args -r __ns:=/<server-namespace>
```

How to start the client:
```bash
ros2 run tcp_tunnel ros2_tcp_tunnel_service_client /<default-topic-name> <interface-name>  --ros-args -r __ns:=/<client-namespace>
```

How to add a topic to the TCP tunnel:
```bash
ros2 service call /<server-namespace>/tcp_tunnel_client/add_topic tcp_tunnel/srv/AddTopic "topic:
  data: '/<topic-name>'
tunnel_queue_size:
  data: '2'
server_namespace:
  data: '/<server-namespace>'
interface:
  data: '<interface-name>'"
```


### Example use case
On the publishing machine, run the server node using the following command:
```bash
ros2 run tcp_tunnel ros2_tcp_tunnel_service_server /pose_5G wlp  --ros-args -r __ns:=/pose_5G
ros2 run tcp_tunnel ros2_tcp_tunnel_service_server /pose_ETH enp  --ros-args -r __ns:=/pose_ETH
```

On the subscribing machine, run the client node using the following command:
```bash
ros2 run tcp_tunnel ros2_tcp_tunnel_service_client /pose_5G wlp  --ros-args -r __ns:=/pose_5G
ros2 run tcp_tunnel ros2_tcp_tunnel_service_client /pose_ETH enx  --ros-args -r __ns:=/pose_ETH
```

Registering the topics (they are demultiplexed by the unique flow topic relay are to be tunneled through TCP using different interfaces):
```bash
ros2 service call /pose_5G/tcp_tunnel_client/add_topic tcp_tunnel/srv/AddTopic "topic:
  data: '/pose_5G'
tunnel_queue_size:
  data: '2'
server_namespace:
  data: '/pose_5G'
interface:
  data: 'wlp'"
```

```bash
ros2 service call /pose_ETH/tcp_tunnel_client/add_topic tcp_tunnel/srv/AddTopic "topic:
  data: '/pose_ETH'
tunnel_queue_size:
  data: '2'
server_namespace:
  data: '/pose_ETH'
interface:
  data: 'enx'"
```


### Demo videos of the example use case
Server side:
[![A Solution for Multipath Channel Switching in ROS2 With 3GPP Integration](https://img.youtube.com/vi/g4lqaNn3j1Q/0.jpg)](https://youtu.be/g4lqaNn3j1Q "A Solution for Multipath Channel Switching in ROS2 With 3GPP Integration")

Client side:
[![A Solution for Multipath Channel Switching in ROS2 With 3GPP Integration cont.](https://img.youtube.com/vi/wyxSVCpNX_s/0.jpg)](https://youtu.be/wyxSVCpNX_s "A Solution for Multipath Channel Switching in ROS2 With 3GPP Integration cont.")

---

# Original documentation
<p align="center">
    <img src="logo.png" width="50%" height="50%">
</p>

---

## ros2_tcp_tunnel
Nodes that allow reliable TCP relay of ROS 2 topics between remote machines.

### Basic Usage
On the publishing machine, run the server node using the following command:
```bash
ros2 run tcp_tunnel server
```

On the subscribing machine, run the client node using the following command:
```bash
ros2 run tcp_tunnel client --ros-args -p client_ip:="<client ip address>"
```

Once both nodes are running, topics can be added to the TCP tunnel dynamically using the following service call:
```bash
ros2 service call /tcp_tunnel_client/add_topic tcp_tunnel/srv/AddTopic "topic:
  data: '<topic name>'
tunnel_queue_size:
  data: '<tunnel queue size>'
server_namespace:
  data: '<server namespace>'"
```
The `tunnel_queue_size` field can be left empty to use the default value of 2.
If the server node is located in the global namespace (default), the `server_namespace` field can be left empty or can be set to '/'.
This will create a new topic named `/tcp_tunnel_client/<topic name>` published on the subscribing machine in which the messages of the original topic are relayed.

Topics can be removed from the TCP tunnel at any time using the following service call:
```bash
ros2 service call /tcp_tunnel_client/remove_topic tcp_tunnel/srv/RemoveTopic "topic:
  data: '<topic name>'"
```
The provided topic name must be the name of the topic at the exit of the TCP tunnel.

### Advanced Usage
#### Tuning the tunnel queue size
The TCP tunnel uses a message acknowledgement mechanism between the server and client nodes.
The tunnel queue size parameter controls how many messages can be sent simultaneously through the TCP tunnel.
When the tunnel queue is full, the server drops messages received on the original topic until an acknowledgement is received from the client, after which the next message received on the original topic will be sent through the tunnel.
If messages are published on the original topic at a higher rate than what can pass through the tunnel, this mechanism will ensure that the tunnel latency does not grow indefinitely.
The tunnel queue size should be increased by the user until the publishing rate of the relayed topic reaches a plateau.
Be careful not to overshoot the queue size however, because a tunnel queue size that is too high can increase the latency of the TCP tunnel.

#### Providing a list of topics on startup of the client node
It is possible to provide a YAML file listing all the topics to add to the TCP tunnel when starting the client node.
For instance, if the user wanted to add the topics `/foo` and `/bar` automatically on startup of the client node, a YAML file with the following content should be created:
```yaml
- topic: /foo
  tunnel_queue_size: 2
  server_namespace: /

- topic: /bar
  tunnel_queue_size: 2
  server_namespace: /
```
The `tunnel_queue_size` field can be left blank to use the default value of 2.
If a server node is located in the global namespace (default), its `server_namespace` field can be left blank or can be set to '/'.
This topic list can then be passed to the client node on startup using the following command:
```bash
ros2 run tcp_tunnel client --ros-args -p client_ip:="<client ip address>" -p initial_topic_list_file_name:="<yaml file name>"
```

#### Running multiple client nodes simultaneously
In order to run multiple client nodes simultaneously, each client node must be located in its own namespace.
This can be achieved by running the client nodes with the following command:
```bash
ros2 run tcp_tunnel client --ros-args -p client_ip:="<client ip address>" -r __ns:="<client namespace>"
```
Make sure to provide a namespace starting with '/'.
It is then possible to call the client nodes `add_topic` services in their respective namespace:
```bash
ros2 service call <client namespace>/tcp_tunnel_client/add_topic tcp_tunnel/srv/AddTopic "topic:
  data: '<topic name>'
tunnel_queue_size:
  data: '<tunnel queue size>'
server_namespace:
  data: '<server namespace>'"
```

#### Running multiple server nodes simultaneously
Similarly, to run multiple server nodes simultaneously, each server node must be located in its own namespace.
This can be achieved by running the server nodes with the following command:
```bash
ros2 run tcp_tunnel server --ros-args -r __ns:="<server namespace>"
```
Make sure to provide a namespace starting with '/'.
Then, when adding a topic to the TCP tunnel, the proper server namespace must be passed in the service call:
```bash
ros2 service call /tcp_tunnel_client/add_topic tcp_tunnel/srv/AddTopic "topic:
  data: '<topic name>'
tunnel_queue_size:
  data: '<tunnel queue size>'
server_namespace:
  data: '<server namespace>'"
```
