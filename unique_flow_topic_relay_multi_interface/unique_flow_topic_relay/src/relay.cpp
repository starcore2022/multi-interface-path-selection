// Based on topic_tools

#include <cassert>
#include <memory>
#include <string>
#include <array>
#include <deque>
#include <utility>

#include "rclcpp/rclcpp.hpp"
#include "subscriber_discovery.h"
#include "unique_flow_topic_relay/msg/throttle.hpp"
#include "unique_flow_topic_relay/msg/tunnel_select.hpp"
#include "unique_flow_topic_relay/msg/unique_topic_flow.hpp"
#include "unique_flow_topic_relay/msg/unique_topic_flow_stats.hpp"
#include "unique_flow_topic_relay/msg/serialized.hpp"
using namespace unique_flow_topic_relay::msg;


#include <arpa/inet.h> //For parsing ip addresses
#include <ifaddrs.h> // For getting interface addresses
#include <net/if.h>

constexpr uint8_t MAX_SPLIT_TOPICS = 4;

bool convert_flow_endpoint_to_locator(Locator* loc, const rclcpp::NetworkFlowEndpoint& nfe);
std::vector<std::array<uint8_t,16>> get_network_interface_addresses_ipv4(bool include_loopback);
std::vector<std::array<uint8_t,16>> get_network_interface_addresses_ipv6(bool include_loopback);
bool is_ipv4(const Locator& loc);
bool is_ipv6(const Locator& loc);


struct TopicInfo{
    std::string input_topic;
    std::string output_topic;
    std::string topic_type;
    rclcpp::QoS qos_profile = rclcpp::SystemDefaultsQoS();
};

struct TopicStats{
    //referenced to start
    std::deque<rclcpp::Duration> times;
    std::deque<size_t> sizes;
    
    rclcpp::Time start{0,0,RCL_CLOCK_UNINITIALIZED};
    size_t transferred_bytes;
    size_t num_messages;
    size_t dropped_messages;
};

struct Message{
    rclcpp::Time time;
    rclcpp::SerializedMessage msg;
};

struct RelayData{
    TopicInfo info;
    
    TopicStats stats;
    
    std::deque<Message> delay_queue;
    uint32_t delay_target = 0; // 0 is off
    uint32_t drop_every_nth = 0; // 0 is off
    
    uint8_t selected_tunnel = 0; // For the split tunnel case
    
    rclcpp::GenericPublisher::SharedPtr pub;
    rclcpp::GenericSubscription::SharedPtr sub;
    
    std::array<rclcpp::Publisher<Serialized>::SharedPtr, MAX_SPLIT_TOPICS> serialized_pubs;

    
    //~RelayData(){
    //    RCLCPP_INFO(rclcpp::get_logger("relaydata") , "Destructing RelayData for %s", info.input_topic.c_str());
    //}
};

void publish(const RelayData& relay, bool use_tunnel, Message msg){
    if(!use_tunnel){
        relay.pub->publish(msg.msg);
    }else{
        Serialized ser_msg;
        const rcl_serialized_message_t& rcl_msg = msg.msg.get_rcl_serialized_message();
        ser_msg.serialized_msg.assign(rcl_msg.buffer, rcl_msg.buffer + rcl_msg.buffer_length);
        ser_msg.sent_time = msg.time;
        ser_msg.msg_num = relay.stats.num_messages + relay.stats.dropped_messages; // num_messages counts the sent messages, if dropped we dont count it
        
        relay.serialized_pubs[relay.selected_tunnel]->publish(ser_msg);
    }
}

auto get_network_flow_endpoints(const RelayData& relay, bool use_tunnel, uint8_t selected_tunnel){
    if(!use_tunnel){
        return relay.pub->get_network_flow_endpoints();
    }else{
        return relay.serialized_pubs[selected_tunnel]->get_network_flow_endpoints();
    }
}

static constexpr std::array<uint8_t,16> ALL_ZERO = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static constexpr std::array<uint8_t,16> LOCALHOST_IPV6 = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1};
static constexpr std::array<uint8_t,16> LOCALHOST_IPV4 = {0,0,0,0,0,0,0,0,0,0,0,0,127,0,0,1};

class UFTopicRelay : public rclcpp::Node {
    rclcpp::TimerBase::SharedPtr discovery_timer;
    std::chrono::duration<float> discovery_period = std::chrono::milliseconds{100};
    
    std::vector<std::unique_ptr<RelayData>> relays;

    std::vector<std::string> split_tunnel_suffixes; // if not empty, and use_serialized_tunnel is true, split the tunnel and append these names to the topic name 
    
    std::vector<std::string> topic_names; // if not empty, relay these nodes instead of using to_uf_name, append uf_name as usual
    std::string to_uf_name;
    std::string uf_name;
    bool publish_localhost_addresses = false;
    bool use_serialized_tunnel = false;
    
    size_t window_size = 100;
    
    
    DiscoveryListener sub_listener;
    
    rclcpp::Subscription<Throttle>::SharedPtr throttle_sub; 
    rclcpp::Subscription<TunnelSelect>::SharedPtr select_sub; 

    
    rclcpp::Publisher<UniqueTopicFlow>::SharedPtr flow_topic_publisher; 
    rclcpp::Publisher<UniqueTopicFlowStats>::SharedPtr stat_topic_publisher; 
    
public:
    UFTopicRelay(const rclcpp::NodeOptions & options): rclcpp::Node("relay", options),sub_listener(rclcpp::contexts::get_global_default_context()->get_domain_id()) {
        topic_names = declare_parameter<std::vector<std::string>>("topic_names", decltype(topic_names)());
        to_uf_name = declare_parameter<std::string>("to_uf_name", "TO_UNIQUE_FLOW");
        uf_name = declare_parameter<std::string>("uf_name", "UNIQUE_FLOW");
        publish_localhost_addresses = declare_parameter<bool>("publish_localhost_addresses", false); // For debugging
        use_serialized_tunnel = declare_parameter<bool>("use_serialized_tunnel", false);
        split_tunnel_suffixes = declare_parameter<std::vector<std::string>>("split_tunnel_suffixes", std::vector<std::string>());
        if(!use_serialized_tunnel && split_tunnel_suffixes.size() > 1){
            RCLCPP_WARN(this->get_logger(), "Using serialized tunnels is off, ignoring split_tunnel_suffixes parameter");
            split_tunnel_suffixes.clear();
        }
        if(split_tunnel_suffixes.size() > MAX_SPLIT_TOPICS){
            RCLCPP_ERROR(this->get_logger(), "split_tunnel_suffixes size must be smaller than %u, ignoring last names",MAX_SPLIT_TOPICS);
            split_tunnel_suffixes.resize(MAX_SPLIT_TOPICS);
        }
        discovery_period = std::chrono::duration<float>{declare_parameter<float>("discovery_period", 0.1)};

        discovery_timer = this->create_wall_timer(discovery_period, std::bind(&UFTopicRelay::make_subscribe_unsubscribe_decisions, this));
        
        throttle_sub = this->create_subscription<Throttle>("throttle", 10, std::bind(&UFTopicRelay::process_throttle_message, this, std::placeholders::_1));
        
        if(use_serialized_tunnel && split_tunnel_suffixes.size() > 1){   
            select_sub = this->create_subscription<TunnelSelect>("select", 10, std::bind(&UFTopicRelay::process_select_message, this, std::placeholders::_1));   
        }
        flow_topic_publisher = this->create_publisher<UniqueTopicFlow>("unique_topic_flow", 10);   
        stat_topic_publisher = this->create_publisher<UniqueTopicFlowStats>("unique_topic_flow_stats", 10);    

        make_subscribe_unsubscribe_decisions();  
        
    }
private:
    std::string network_flow_endpoints_str(const std::vector<rclcpp::NetworkFlowEndpoint> & network_flow_endpoints) const{
        std::ostringstream stream;
        stream << "{\"networkFlowEndpoints\": [";
        bool comma_skip = true;
        for (auto network_flow_endpoint : network_flow_endpoints) {
            if (comma_skip) {
                comma_skip = false;
            } else {
                stream << ",";
            }
            stream << network_flow_endpoint;
        }
        stream << "]}";
        return stream.str();
    }
    
    std::optional<std::pair<std::string, rclcpp::QoS>> try_discover_source(const std::string& topic_name){
        // borrowed this from domain bridge
        // (https://github.com/ros2/domain_bridge/blob/main/src/domain_bridge/wait_for_graph_events.hpp)
        // Query QoS info for publishers
        std::vector<rclcpp::TopicEndpointInfo> endpoint_info_vec = this->get_publishers_info_by_topic(topic_name);
        std::size_t num_endpoints = endpoint_info_vec.size();

        // If there are no publishers, return an empty optional
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
    
    // Handle subscriber discovery, which finds the subscibers for the relayed topic, and publish the topic flow information
    void publish_unique_topic_flow_msgs(){

        auto sub_updates = sub_listener.get_subscriber_changes();
        for(const auto& upd: sub_updates){
            RCLCPP_INFO(this->get_logger(), "Sub Update: %s: %s %s %s %d.%d.%d.%d:%d", EventType_txt[upd.type], upd.topic.c_str(), CastType_txt[upd.cast], ProtocolKind_txt[upd.kind], upd.address[12],upd.address[13], upd.address[14], upd.address[15], upd.port);
            for(const auto& relayp: relays){
                const auto& relay = *relayp;
                bool found_topic = relay.info.output_topic == upd.topic;
                uint8_t tunnel = 0;
                
                auto pos = upd.topic.rfind(relay.info.output_topic,0);
                bool found_split_topic = split_tunnel_suffixes.size()>0 && (pos == 0) && !found_topic; // The subscribed topic only starts with the output name, but it's longer, we assume its a split topic 
                if (found_split_topic){
                    auto suffix = upd.topic.substr(relay.info.output_topic.size());
                    uint8_t suffix_idx = 0;
                    for (const auto& s: split_tunnel_suffixes){
                        if (s==suffix) break;
                        suffix_idx++;
                    }
                    if(suffix_idx<split_tunnel_suffixes.size()){
                        tunnel = suffix_idx;
                    }else{
                        // As we only add watched topics to sub_listener that have suffixes in split_tunnel_suffixes, this shouldn't happen.
                        RCLCPP_WARN(this->get_logger(), "Found a topic '%s' whose suffix '%s' is not in the parameter split_tunnel_suffixes. Ignoring...", upd.topic.c_str(), suffix.c_str());
                        found_split_topic = false;
                    }
                }
                
                if(found_topic || found_split_topic){
                    for(const auto& nfe : get_network_flow_endpoints(relay, use_serialized_tunnel, tunnel)){
                        UniqueTopicFlow msg;
                        //uint8 ADDED=1
                        //uint8 REMOVED=2
                        msg.event_type = upd.type;
                        
                        msg.topic = relay.info.output_topic;
                        
                        //Locator source
                        bool success = convert_flow_endpoint_to_locator(&msg.source, nfe);
                        if(!success) {
                            RCLCPP_ERROR_STREAM(this->get_logger(), "Couldn't convert network flow endpoint! " << nfe); 
                            continue;
                        }
                        //RCLCPP_INFO(this->get_logger(),"nfe: %d.%d.%d.%d", msg.source.address[12], msg.source.address[13], msg.source.address[14], msg.source.address[15]);
                        
                        if(!publish_localhost_addresses && is_ipv4(msg.source) && msg.source.address == LOCALHOST_IPV4) continue; // TODO: ipv4 localhost is not just 127.0.0.1 but 127.0.0.0/8
                        if(!publish_localhost_addresses && is_ipv6(msg.source) && msg.source.address == LOCALHOST_IPV6) continue;
                        
                        if(found_split_topic){
                            msg.split_tunnel_name_suffix = split_tunnel_suffixes[tunnel];
                            msg.tunnel_select = tunnel;
                        }
                        
                        //Locator destination
                        msg.destination.cast = static_cast<uint8_t>(upd.cast);
                        msg.destination.kind = static_cast<uint8_t>(upd.kind);
                        msg.destination.port = upd.port;
                        memcpy(msg.destination.address.data(), upd.address, 16);
                        
                        if(!publish_localhost_addresses && is_ipv4(msg.destination) && msg.destination.address == LOCALHOST_IPV4) continue; // TODO: ipv4 localhost is not just 127.0.0.1 but 127.0.0.0/8
                        if(!publish_localhost_addresses && is_ipv6(msg.destination) && msg.destination.address == LOCALHOST_IPV6) continue;
                        
                        // When the source is 0.0.0.0 then it publishes on all interfaces, so we need to send all possible sources
                        // (or we could get the used source ip for a destination ip (like ip route get, but with the netlink interface, like the command))
                                               
                        if(msg.source.address == ALL_ZERO){
                            if(is_ipv4(msg.source)){
                                const auto addresses = get_network_interface_addresses_ipv4(publish_localhost_addresses);
                                for(const auto& addr: addresses){
                                    msg.source.address = addr;
                                    flow_topic_publisher->publish(msg);
                                    RCLCPP_INFO(this->get_logger(), "Published replaced 0.0.0.0 flow msg: %s", to_yaml(msg,true).c_str());
                                }
                            }
                            if(is_ipv6(msg.source)){
                                const auto addresses = get_network_interface_addresses_ipv6(publish_localhost_addresses);
                                for(const auto& addr: addresses){
                                    msg.source.address = addr;
                                    flow_topic_publisher->publish(msg);
                                    //RCLCPP_INFO(this->get_logger(), "Published flow msg: %s", to_yaml(msg).c_str());
                                }
                            }
                        }else{
                        
                            flow_topic_publisher->publish(msg);
                            RCLCPP_INFO(this->get_logger(), "Published not 0.0.0.0 flow msg: %s", to_yaml(msg,true).c_str());
                        }
                    }
                }
            }
        }
    }
    
    void compute_and_publish_stats(){
        UniqueTopicFlowStats msg;
        auto now = this->get_clock()->now();
        msg.stamp = now;
        msg.stats.reserve(relays.size());
        for(auto& relay: relays){
            if(relay->stats.start.get_clock_type() == RCL_CLOCK_UNINITIALIZED) continue;
            
            auto start_time = relay->stats.start;
            //double duration = (now - start_time).seconds();
            //RCLCPP_INFO(this->get_logger(),"%s: rate: %f bw: %f num: %ld", relay->info.input_topic.c_str(), relay->stats.num_messages/duration, relay->stats.transferred_bytes/duration, relay->stats.num_messages);
            
            auto& times = relay->stats.times;
            auto num_in_window = times.size();
            auto& sizes = relay->stats.sizes;
            if (num_in_window < 2) continue;
            //needs mutex to allow reading the times and sizes
            auto duration_window = (times.back()-times.front()).seconds();
            //auto rate = num_in_window / duration_window;
            
            // iterate over pairs and compute average rate
            double exact_average_delta = 0.0;
            for(size_t i = 0; i < num_in_window -1;i++){ // !!SLOW if window is big, this could be done better
                auto f = times[i];
                auto s = times[i+1];
                auto delta = (s-f).seconds();
                exact_average_delta = (delta + i * exact_average_delta) / (i+1);
            }
            auto exact_rate = 1 / exact_average_delta;
            
            size_t sum = 0;
            for(auto size : sizes){
                sum+=size; // !!SLOW if window is big, this could be done better, by keeping thrac of the sum of sizes seperately, and adding and subtracting from it, when adding and removing from the window
            }
            auto exact_bw = sum / duration_window;
            //RCLCPP_INFO(this->get_logger(),"%s: rate: %f exact_rate: %f bw: %f sum: %ld dur: %f", relay->info.input_topic.c_str(), rate, exact_rate, exact_bw ,sum, duration_window);
            auto& stat_msg = msg.stats.emplace_back();            
            stat_msg.topic = relay->info.output_topic;
            stat_msg.start = start_time;
            stat_msg.num_messages = relay->stats.num_messages;
            stat_msg.num_bytes = relay->stats.transferred_bytes;
            stat_msg.window_duration_secs = duration_window;
            stat_msg.window_bandwidth = exact_bw;
            stat_msg.window_rate = exact_rate;
            stat_msg.window_average_delta = exact_average_delta;
        }
        stat_topic_publisher->publish(msg);
    }
    void make_subscribe_unsubscribe_decisions(){        
        //std::map        topic_name_str:topic_type_strarray      
        for (const auto& n : this->get_topic_names_and_types()) {
            auto& topic_name = n.first;
            //RCLCPP_INFO(this->get_logger(), "Searching for topic : %s", topic_name.c_str());

            
            std::string output_name = "";
            // If topic_names are specified only use it
            if(std::find(std::begin(topic_names),std::end(topic_names), topic_name) != std::end(topic_names)){
                auto loc = topic_name.find(to_uf_name);
                if(loc == std::string::npos) {
                    std::string suffix = uf_name;
                    if(suffix.size() == 0) suffix = "UF";
                    output_name = topic_name + suffix;
                }else{
                    output_name = std::string(topic_name).replace(loc, to_uf_name.size(), uf_name);
                }
                //RCLCPP_DEBUG(this->get_logger(), 
                //    "Found topic in relay topic list: %s -> %s", topic_name.c_str(), output_name.c_str());
                
            // If werent in topic_names but its not empty, ignore
            } else if(!topic_names.empty()){
                continue;
            // If topic_names is empty
            }else{
                auto loc = topic_name.find(to_uf_name);
                if(loc == std::string::npos) continue;
                
                output_name = std::string(topic_name).replace(loc, to_uf_name.size(), uf_name);
            }
            
            
            // Check if we found this topic before
            RelayData* found = nullptr;
            size_t found_index = 0;            
            for(auto& relay: relays){
                if(relay->info.input_topic == topic_name){
                    found = relay.get();
                    //RCLCPP_INFO(this->get_logger(),"Found existing topic %s", found->info.input_topic.c_str());
                    break;
                }
                found_index++;
            }
            
            // we tested only one publisher per topic
            if (auto source_info = try_discover_source(topic_name)) {
                if(!found){
                    found = relays.emplace_back(std::make_unique<RelayData>()).get();
                    found->info.input_topic = topic_name;
                    found->info.output_topic = output_name;
                    
                    auto num_tunnels = split_tunnel_suffixes.size();
                    if (num_tunnels == 0) { // No split tunnels
                        sub_listener.add_watched_topic(output_name);
                    }
                    RCLCPP_INFO(this->get_logger(), "Added relay: %s -> %s", found->info.input_topic.c_str(),found->info.output_topic.c_str());
                    for(uint8_t i = 0; i<num_tunnels; i++){
                        auto full_name = output_name+split_tunnel_suffixes[i];
                        sub_listener.add_watched_topic(full_name);
                        RCLCPP_INFO(this->get_logger(), "%d: with split tunnel suffix %s", i, split_tunnel_suffixes[i].c_str());
                    }
                }
                
                // always relay same topic type and QoS profile as the first available source
                bool no_pub_yet = !(found->pub || found->serialized_pubs[0]);
                if (found->info.topic_type != source_info->first || found->info.qos_profile != source_info->second || no_pub_yet) {
                    found->info.topic_type = source_info->first;
                    found->info.qos_profile = source_info->second;
                    
                    // From https://github.com/ros2/examples/blob/master/rclcpp/topics/minimal_publisher/member_function_with_unique_network_flow_endpoints.cpp
                    auto options = rclcpp::PublisherOptions();
                    options.require_unique_network_flow_endpoints = RMW_UNIQUE_NETWORK_FLOW_ENDPOINTS_OPTIONALLY_REQUIRED;
                    if(!use_serialized_tunnel){
                        found->pub = this->create_generic_publisher(found->info.output_topic, found->info.topic_type, found->info.qos_profile, options);
                    }else{
                        auto num_tunnels = split_tunnel_suffixes.size();
                        if (num_tunnels == 0) {
                            found->serialized_pubs[0] = this->create_publisher<Serialized>(found->info.output_topic, found->info.qos_profile, options);
                        } else {
                            RCLCPP_INFO(this->get_logger(), "Creating %lu split tunnels...", num_tunnels);
                            for(uint8_t i = 0; i<num_tunnels; i++){
                                auto full_name = found->info.output_topic+split_tunnel_suffixes[i];
                                RCLCPP_INFO(this->get_logger(), "%d: %s", i, full_name.c_str());
                                found->serialized_pubs[i] = this->create_publisher<Serialized>(full_name, found->info.qos_profile, options);
                            }
                        }
                    }
                    //auto nfs = network_flow_endpoints_str(get_network_flow_endpoints(*found, use_serialized_tunnel));
                    
                    RCLCPP_INFO(this->get_logger(), "Changed relay: %s -> %s", found->info.input_topic.c_str(),found->info.output_topic.c_str());//,nfs.c_str());
                }
                //auto nfs = network_flow_endpoints_str(found->pub->get_network_flow_endpoints());
                    
                //RCLCPP_INFO(this->get_logger(),"nfs: %s", nfs.c_str());

                // at this point it is certain that our publisher exists
                if (!found->sub) {
                    //:MaybeMutex
                    //found->sub = this->create_generic_subscription(found->info.input_topic, found->info.topic_type, found->info.qos_profile,
                    //    std::bind(&UFTopicRelay::process_message, this, found, std::placeholders::_1));
                    found->sub = this->create_generic_subscription(found->info.input_topic, found->info.topic_type, found->info.qos_profile,
                        std::bind(&UFTopicRelay::process_message_and_measure, this, found, std::placeholders::_1));
                    
                }
            } else {
                // it's not being published, but it once was, so delete it
                if(found) {
                    RCLCPP_INFO(this->get_logger(), "Removing relay: %s -> %s", found->info.input_topic.c_str(),found->info.output_topic.c_str());
                    sub_listener.remove_watched_topic(found->info.output_topic);
                    relays.erase(relays.begin() + found_index);
                }
            }
            
        }
        
        compute_and_publish_stats();
        publish_unique_topic_flow_msgs();
    }
    
    // Maybe needs a mutex to avoid using the relay after it was deleted :MaybeMutex 
    void process_message(RelayData* relay_data, std::shared_ptr<rclcpp::SerializedMessage> msg){
        publish(*relay_data,use_serialized_tunnel,{this->get_clock()->now(),*msg});
    }
    
    void process_message_and_measure(RelayData* relay_data, std::shared_ptr<rclcpp::SerializedMessage> msg){
        RelayData& relay = *relay_data; 
        //
        // Drop handling
        //
        if(relay.drop_every_nth != 0 && (relay.stats.dropped_messages+relay.stats.num_messages+1) % relay.drop_every_nth == 0){
            // TODO decide what to do if not published, should we count it in stats?
            relay.stats.dropped_messages++;
            return;
        }
        
        //
        // Collecting stats
        //
        auto now = this->get_clock()->now();
        if(relay.stats.start.get_clock_type() == RCL_CLOCK_UNINITIALIZED){
            relay.stats.start = now;
            //RCLCPP_INFO(this->get_logger(), "Got starttime");
        }
        
        auto& start = relay.stats.start;
        const auto& size = msg->size();
        relay.stats.num_messages++;
        relay.stats.transferred_bytes+=size;
        
        //needs mutex to allow changing the times and sizes
        relay.stats.times.emplace_back(now-start);
        relay.stats.sizes.emplace_back(size);
        if (relay.stats.times.size()>window_size){
            relay.stats.times.pop_front();
            relay.stats.sizes.pop_front();
        }
        
        //
        // Delay handling
        //
        if(relay.delay_queue.size() == 0 && relay.delay_target == 0){ // No delay
            // Just publish the msg
            publish(relay,use_serialized_tunnel,{this->get_clock()->now(),*msg});
        } else if(relay.delay_queue.size() == relay.delay_target){ // Has delay but its constant
            relay.delay_queue.push_back({this->get_clock()->now(),*msg});
            auto delayed_msg = relay.delay_queue.front();
            relay.delay_queue.pop_front();
            publish(relay,use_serialized_tunnel,delayed_msg);
        } else if(relay.delay_queue.size() > relay.delay_target){ // Delay become smaller
            relay.delay_queue.push_back({this->get_clock()->now(),*msg});
            while(relay.delay_queue.size() > relay.delay_target){
                auto delayed_msg = relay.delay_queue.front();
                relay.delay_queue.pop_front();
                publish(relay,use_serialized_tunnel,delayed_msg);
            }
        } else if(relay.delay_queue.size() < relay.delay_target){ // Delay become larger
            relay.delay_queue.push_back({this->get_clock()->now(),*msg}); 
            // Don't publish
        } else {
            RCLCPP_FATAL(this->get_logger(), "This can't happen");
            rclcpp::shutdown();
            exit(1);
        }
    }
    
    void process_throttle_message(Throttle::SharedPtr msg){
        for(auto& relayp: relays){
            auto& relay = *relayp;
            if(relay.info.output_topic == msg->topic){
                RCLCPP_INFO(this->get_logger(), "Setting throttle data for %s:\ndrop_every_nth:%d, message_delay:%d",msg->topic.c_str(),msg->drop_every_nth, msg->message_delay);
                // Maybe needs a mutex :MaybeMutex
                relay.drop_every_nth = msg->drop_every_nth;
                relay.delay_target = msg->message_delay;
            }
        }
    }
    
    void process_select_message(TunnelSelect::SharedPtr msg){
        if(msg->select > split_tunnel_suffixes.size()){
            RCLCPP_WARN(this->get_logger(), "selected tunnel is invalid. %d > %lu", msg->select, split_tunnel_suffixes.size());
        }
        RCLCPP_INFO(this->get_logger(), "Selecting tunnel %d for all relays",msg->select);
        for(auto& relayp: relays){
            // Selecting tunnel for all relays, no per topic setting
            auto& relay = *relayp;
            // Maybe needs a mutex :MaybeMutex
            relay.selected_tunnel = msg->select;
        }
    }

};

int main(int argc, char** argv){
    auto args = rclcpp::init_and_remove_ros_arguments(argc, argv);
    auto options = rclcpp::NodeOptions{};

    if (args.size() >= 2) {
        options.append_parameter_override("topic_names", decltype(args){args.begin() + 1, args.end()});
    }

    auto node = std::make_shared<UFTopicRelay>(options);

    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}

bool is_ipv4(const Locator& loc){
    return loc.kind == Locator::UDPV4 ||loc.kind == Locator::TCPV4;
}
bool is_ipv6(const Locator& loc){
    return loc.kind == Locator::UDPV6 ||loc.kind == Locator::TCPV6;
}

// Would be better if we could use std::span
bool parse_ipv4_address(const std::string& input, std::array<uint8_t, 16> *result) {
    in_addr address;
    if (inet_pton(AF_INET, input.c_str(), &address) == 1) {
        memcpy(&((*result)[12]), &address.s_addr, 4); 
        return true;
    }
    return false;
}
 
bool parse_ipv6_address(const std::string& input, std::array<uint8_t, 16 > *result) {
    in6_addr address;
    if (inet_pton(AF_INET6, input.c_str(), &address) == 1) {
        memcpy(result->data(), &address.s6_addr, 16);
        return true;
    }
    return false;
}

// rclcpp is converting from the rcl type to std::strings and we are converting back :(
// strings location: rmw/src/network_flow_endpoint:c
bool convert_flow_endpoint_to_locator(Locator* loc, const rclcpp::NetworkFlowEndpoint& nfe){
    //uint8 UNICAST=1
    //uint8 MULTICAST=2
    loc->cast =  Locator::UNICAST;
    
    //uint8 UDPV4=1
    //uint8 UDPV6=2
    //uint8 TCPV4=3
    //uint8 TCPV6=4
    uint8_t kind = 0;
    auto& ip = nfe.internet_protocol();
    auto& tp = nfe.transport_protocol();
    if(ip == "IPv4"){
        if(tp == "UDP"){
            kind = Locator::UDPV4;
        }else if(ip == "TCP"){
            kind = Locator::TCPV4;
        }
        bool success = parse_ipv4_address(nfe.internet_address(), &loc->address);
        if(!success) return false;
    }else if(ip == "IPv6"){
        if(tp == "UDP"){
            kind = Locator::UDPV6;
        }else if(ip == "TCP"){
            kind = Locator::TCPV6;
        }
        
        bool success = parse_ipv6_address(nfe.internet_address(), &loc->address);
        if(!success) return false;
    }
    loc->kind = kind;

    loc->port = nfe.transport_port();
    return true;
}

// from getifaddrs(3) man page
std::vector<std::array<uint8_t,16>> get_network_interface_addresses_ipv4(bool include_loopback){
    struct ifaddrs *ifaddr;
    
    std::vector<std::array<uint8_t,16>> addresses;

    if (getifaddrs(&ifaddr) == -1) {
        return addresses;
    }

    for (struct ifaddrs *ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;
        // we dont care about the loopback, loopback doesnt come as 0.0.0.0
        if(!include_loopback && (ifa->ifa_flags & IFF_LOOPBACK)) continue;

        int family = ifa->ifa_addr->sa_family;

        if (family == AF_INET ) {
            std::array<uint8_t,16> arr_addr = {};

            sockaddr_in *address = reinterpret_cast<sockaddr_in*>(ifa->ifa_addr);
            memcpy(&arr_addr[12],&address->sin_addr.s_addr,4);
            addresses.push_back(arr_addr);
        }
    }

    freeifaddrs(ifaddr);
    return addresses;
}
std::vector<std::array<uint8_t,16>> get_network_interface_addresses_ipv6(bool include_loopback){
    struct ifaddrs *ifaddr;
    
    std::vector<std::array<uint8_t,16>> addresses;

    if (getifaddrs(&ifaddr) == -1) {
        return addresses;
    }


    for (struct ifaddrs *ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;
        // we dont care about the loopback, loopback doesnt come as 0.0.0.0
        if(!include_loopback && (ifa->ifa_flags & IFF_LOOPBACK)) continue;

        int family = ifa->ifa_addr->sa_family;

        if (family == AF_INET6 ) {
            std::array<uint8_t,16> arr_addr = {};

            sockaddr_in6 *address = reinterpret_cast<sockaddr_in6*>(ifa->ifa_addr);
            memcpy(arr_addr.data(),&address->sin6_addr.s6_addr,16);
            addresses.push_back(arr_addr);
        }
    }

    freeifaddrs(ifaddr);
    return addresses;
}
