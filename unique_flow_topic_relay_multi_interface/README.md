# Introduction

These set of ROS2 nodes explores the potential for making the use of 3GPP TS 23.434 SEAL (Service Enabler Architecture Layer) even easier and more automatic in various industry verticals applying ROS2 for their operation. The proposed solution involves implementing a mapping node that converts ROS2 settings to 3GPP SEAL requests, as well as an information collecting proxy node. 

![Basic architecture](/docs/chatter.png)

We cover three SEAL functionalities: 
-   group management
-   network resource management
-   network monitoring

We provide a solution that automatically maps ROS2 application layer information elements to SEAL requests, requiring the ROS2 application developer need only to tag the ROS2 topics for the special QoS handling. We demonstrate the feasibility and light-weight nature of their proposed solution.

The demo video of the proposed system in action can be seen at [https://youtu.be/JXGAHvDSU4o](https://youtu.be/JXGAHvDSU4o).


# Usage
After sourcing the workspace

	ros2 launch unique_flow_topic_relay test_topic.launch.yaml
	ros2 run seal topic_watcher
	ros2 topic echo /chatter

# Citation
[G. Szabó, "Towards The Automatic Network Resource Management of Robot Operating System In Programmable Mobile Networks," in IEEE Access, doi: 10.1109/ACCESS.2023.3289922.](https://ieeexplore.ieee.org/document/10164106)

@ARTICLE{10164106,
  author={Szabó, Géza},
  journal={IEEE Access}, 
  title={Towards The Automatic Network Resource Management of Robot Operating System In Programmable Mobile Networks}, 
  year={2023},
  volume={},
  number={},
  pages={1-1},
  doi={10.1109/ACCESS.2023.3289922}}



