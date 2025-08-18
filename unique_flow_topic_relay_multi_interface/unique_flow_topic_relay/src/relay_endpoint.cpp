// Based on topic_tools

#include <memory>
//#include <optional>
#include <string>
#include <array>
#include <deque>
#include <utility>

#include "rclcpp/rclcpp.hpp"
#include "unique_flow_topic_relay/msg/serialized.hpp"
#include "unique_flow_topic_relay/msg/unique_topic_flow_stats.hpp"
using namespace unique_flow_topic_relay::msg;

constexpr uint8_t MAX_SPLIT_TOPICS = 4; // same as in relay.cpp

struct TopicInfo{
    std::string input_topic;
    std::string output_topic;
    std::string topic_type;
    rclcpp::QoS qos_profile = rclcpp::SystemDefaultsQoS();
};


struct RelayData{
    TopicInfo info;
    
    uint64_t num_messages = 0;
    uint64_t last_sequence_num = 0;
    uint64_t dropped_messages = 0;
    
    double average_latency = 0.0;
    
    rclcpp::GenericPublisher::SharedPtr pub;
    std::array<rclcpp::Subscription<Serialized>::SharedPtr, MAX_SPLIT_TOPICS> subs;
    
    //~RelayData(){
    //    RCLCPP_INFO(rclcpp::get_logger("relaydata") , "Destructing RelayData for %s", info.input_topic.c_str());
    //}
};

static constexpr std::array<uint8_t,16> ALL_ZERO = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static constexpr std::array<uint8_t,16> LOCALHOST_IPV6 = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1};
static constexpr std::array<uint8_t,16> LOCALHOST_IPV4 = {0,0,0,0,0,0,0,0,0,0,0,0,127,0,0,1};

class UFTopicRelayEndpoint : public rclcpp::Node {
    rclcpp::TimerBase::SharedPtr discovery_timer;
    std::chrono::duration<float> discovery_period = std::chrono::milliseconds{100};
    
    std::vector<std::unique_ptr<RelayData>> relays;
    
    std::vector<std::string> split_tunnel_suffixes;
    
    std::vector<std::string> topic_names; // if not empty, relay these nodes instead of using from_uf_name, append uf_name as usual
    std::string from_uf_name;
    std::string uf_name;
    
    
    rclcpp::Publisher<UniqueTopicFlowStats>::SharedPtr stat_topic_publisher; 
    
public:
    UFTopicRelayEndpoint(const rclcpp::NodeOptions & options): rclcpp::Node("endpoint", options) {
        topic_names = declare_parameter<std::vector<std::string>>("topic_names", decltype(topic_names)());
        from_uf_name = declare_parameter<std::string>("from_uf_name", "FROM_UNIQUE_FLOW");
        uf_name = declare_parameter<std::string>("uf_name", "UNIQUE_FLOW");
        discovery_period = std::chrono::duration<float>{declare_parameter<float>("discovery_period", 0.1)};
        
        split_tunnel_suffixes = declare_parameter<std::vector<std::string>>("split_tunnel_suffixes", std::vector<std::string>());

        discovery_timer = this->create_wall_timer(discovery_period, std::bind(&UFTopicRelayEndpoint::make_subscribe_unsubscribe_decisions, this));
                
        stat_topic_publisher = this->create_publisher<UniqueTopicFlowStats>("unique_topic_flow_stats", 10);    

        make_subscribe_unsubscribe_decisions();  
        
    }
private:
    
    std::optional<std::pair<std::string, rclcpp::QoS>> try_discover_drain(const std::string& topic_name){
        // borrowed this from domain bridge
        // (https://github.com/ros2/domain_bridge/blob/main/src/domain_bridge/wait_for_graph_events.hpp)
        // Query QoS info for publishers
        std::vector<rclcpp::TopicEndpointInfo> endpoint_info_vec = this->get_subscriptions_info_by_topic(topic_name);
        std::size_t num_endpoints = endpoint_info_vec.size();

        // If there are no subscribers, return an empty optional
        if (num_endpoints < 1u) {
            return {};
        }

        // Initialize QoS
        rclcpp::QoS qos{10};
        // Default reliability and durability to value of first endpoint
        qos.reliability(endpoint_info_vec[0].qos_profile().reliability());
        qos.durability(endpoint_info_vec[0].qos_profile().durability());
        // Always use automatic liveliness
        qos.liveliness(rclcpp::LivelinessPolicy::Automatic);

        // Reliability and durability policies can cause trouble with enpoint matching
        // Count number of "reliable" publishers and number of "transient local" publishers
        std::size_t reliable_count = 0u;
        std::size_t transient_local_count = 0u;
        // For duration-based policies, note the largest value to ensure matching all publishers
        rclcpp::Duration max_deadline(0, 0u);
        rclcpp::Duration max_lifespan(0, 0u);
        for (const auto & info : endpoint_info_vec) {
            const auto & profile = info.qos_profile();
            if (profile.reliability() == rclcpp::ReliabilityPolicy::Reliable) {
                reliable_count++;
            }
            if (profile.durability() == rclcpp::DurabilityPolicy::TransientLocal) {
                transient_local_count++;
            }
            if (profile.deadline() > max_deadline) {
                max_deadline = profile.deadline();
            }
            if (profile.lifespan() > max_lifespan) {
                max_lifespan = profile.lifespan();
            }
        }

        // If not all publishers have a "reliable" policy, then use a "best effort" policy
        // and print a warning
        if (reliable_count > 0u && reliable_count != num_endpoints) {
            qos.best_effort();
            RCLCPP_WARN(
                this->get_logger(), "Some, but not all, publishers on topic %s "
                "offer 'reliable' reliability. Falling back to 'best effort' reliability in order"
                "to connect to all publishers.", topic_name.c_str());
        }

        // If not all publishers have a "transient local" policy, then use a "volatile" policy
        // and print a warning
        if (transient_local_count > 0u && transient_local_count != num_endpoints) {
            qos.durability_volatile();
            RCLCPP_WARN(this->get_logger(), 
                "Some, but not all, publishers on topic %s "
                "offer 'transient local' durability. Falling back to 'volatile' durability in order"
                "to connect to all publishers.", topic_name.c_str());
        }

        qos.deadline(max_deadline);
        qos.lifespan(max_lifespan);

        if (!endpoint_info_vec.empty()) {
            return std::make_pair(endpoint_info_vec[0].topic_type(), qos);
        }

        return {};

    }
    
    
    void compute_and_publish_stats(){
       UniqueTopicFlowStats msg;
        auto now = this->get_clock()->now();
        msg.stamp = now;
        msg.tunnel_stats.reserve(relays.size());
        for(auto& relay: relays){
            if(relay->num_messages == 0) continue; 
            auto& stat_msg = msg.tunnel_stats.emplace_back();            
            stat_msg.topic = relay->info.input_topic;
            stat_msg.measured_latency = relay->average_latency;
           
            stat_msg.measured_droprate = 1.0 - relay->num_messages / (double) (relay->num_messages + relay->dropped_messages) ;
        }
        stat_topic_publisher->publish(msg);
    }
    void make_subscribe_unsubscribe_decisions(){        
        //std::map        topic_name_str:topic_type_strarray      
        for (const auto& n : this->get_topic_names_and_types()) {
            auto& topic_name = n.first;
            //RCLCPP_INFO(this->get_logger(), "Searching for topic : %s", topic_name.c_str());

            
            std::string input_name = "";
            // If topic_names are specified only use it
            if(std::find(std::begin(topic_names),std::end(topic_names), topic_name) != std::end(topic_names)){
                auto loc = topic_name.find(from_uf_name);
                if(loc == std::string::npos) {
                    std::string suffix = uf_name;
                    if(suffix.size() == 0) suffix = "UF";
                    input_name = topic_name + suffix;
                }else{
                    input_name = std::string(topic_name).replace(loc, from_uf_name.size(), uf_name);
                }
                //RCLCPP_DEBUG(this->get_logger(), 
                //    "Found topic in relay topic list: %s -> %s", topic_name.c_str(), input_name.c_str());
                
            // If werent in topic_names but its not empty, ignore
            } else if(!topic_names.empty()){
                continue;
            // If topic_names is empty
            }else{
                auto loc = topic_name.find(from_uf_name);
                if(loc == std::string::npos) continue;
                
                input_name = std::string(topic_name).replace(loc, from_uf_name.size(), uf_name);
            }
            
            
            // Check if we found this topic before
            RelayData* found = nullptr;
            size_t found_index = 0;            
            for(auto& relay: relays){
                if(relay->info.output_topic == topic_name){
                    found = relay.get();
                    //RCLCPP_INFO(this->get_logger(),"Found existing topic %s", found->info.input_topic.c_str());
                    break;
                }
                found_index++;
            }
            
            // we tested only one publisher per topic
            if (auto drain_info = try_discover_drain(topic_name)) {
                if(!found){
                    found = relays.emplace_back(std::make_unique<RelayData>()).get();
                    found->info.output_topic = topic_name;
                    found->info.input_topic  = input_name;
                    
                    auto num_tunnels = split_tunnel_suffixes.size();
                    RCLCPP_INFO(this->get_logger(), "Added endpoint: %s -> %s", found->info.input_topic.c_str(),found->info.output_topic.c_str());
                    for(uint8_t i = 0; i<num_tunnels; i++){
                        auto full_name = input_name+split_tunnel_suffixes[i];
                        RCLCPP_INFO(this->get_logger(), "%d: with split tunnel suffix %s", i, split_tunnel_suffixes[i].c_str());
                    }
                }
                
                // always relay same topic type and QoS profile as the first available source
                if (found->info.topic_type != drain_info->first || found->info.qos_profile != drain_info->second || !found->pub) {
                    found->info.topic_type = drain_info->first;
                    found->info.qos_profile = drain_info->second;
                    
                    // From https://github.com/ros2/examples/blob/master/rclcpp/topics/minimal_publisher/member_function_with_unique_network_flow_endpoints.cpp
                    auto options = rclcpp::PublisherOptions();
                    found->pub = this->create_generic_publisher(found->info.output_topic, found->info.topic_type, found->info.qos_profile, options);
                    
                    RCLCPP_INFO(this->get_logger(), "Changed endpoint: %s -> %s\n", found->info.input_topic.c_str(),found->info.output_topic.c_str());
                }
                    
                // at this point it is certain that our publisher exists
                if (!found->subs[0]) {
                    //:MaybeMutex
                    
                    // Stupid workaround https://answers.ros.org/question/308386/ros2-add-arguments-to-callback/
                    std::function<void(const Serialized::SharedPtr msg)> fcn = std::bind(&UFTopicRelayEndpoint::process_message_and_unwrap, this, found, std::placeholders::_1);
                    
                    auto num_tunnels = split_tunnel_suffixes.size();
                    if (num_tunnels == 0) {
                        found->subs[0] = this->create_subscription<Serialized>(found->info.input_topic, found->info.qos_profile, fcn);
                    } else {
                        RCLCPP_INFO(this->get_logger(), "Subscribing to %lu split tunnels...", num_tunnels);
                        for(uint8_t i = 0; i<num_tunnels; i++){
                            auto full_name = found->info.input_topic+split_tunnel_suffixes[i];
                            RCLCPP_INFO(this->get_logger(), "%d: %s", i, full_name.c_str());
                            found->subs[i] = this->create_subscription<Serialized>(full_name, found->info.qos_profile, fcn);
                        }
                    }
                    
                    
                }
            } else {
                // it's not being subscribed to, but it once was, so delete it
                if(found) {
                    RCLCPP_INFO(this->get_logger(), "Removing endpoint: %s -> %s", found->info.input_topic.c_str(),found->info.output_topic.c_str());
                    relays.erase(relays.begin() + found_index);
                }
            }
            
        }
        
        compute_and_publish_stats();
    }
    
    // Maybe needs a mutex to avoid using the relay after it was deleted :MaybeMutex 
    
    void process_message_and_unwrap(RelayData* relay_data, const Serialized::SharedPtr msg){
        RelayData& relay = *relay_data; 

        auto ser_msg = rclcpp::SerializedMessage(msg->serialized_msg.size());
        auto& rcl_ser_msg = ser_msg.get_rcl_serialized_message();
        
        memcpy(rcl_ser_msg.buffer,msg->serialized_msg.data(),msg->serialized_msg.size());
        rcl_ser_msg.buffer_length = msg->serialized_msg.size();
        
        relay.pub->publish(ser_msg);
        
        // Compute stats
        auto now = this->get_clock()->now();
        auto msg_time = rclcpp::Time(msg->sent_time);
        auto latency = (now-msg_time).seconds();
        
        relay.average_latency = (relay.average_latency*relay.num_messages + latency) / (relay.num_messages+1);

        relay.num_messages++;
        
        relay.dropped_messages += msg->msg_num - relay.last_sequence_num - 1; // Difference of one means we didnt drop any
        relay.last_sequence_num = msg->msg_num;
    }
    

};

int main(int argc, char** argv){
    auto args = rclcpp::init_and_remove_ros_arguments(argc, argv);
    auto options = rclcpp::NodeOptions{};
    

    if (args.size() >= 2) {
        options.append_parameter_override("topic_names", decltype(args){args.begin() + 1, args.end()});
    }

    auto node = std::make_shared<UFTopicRelayEndpoint>(options);

    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
