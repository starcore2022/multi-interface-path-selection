#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/domain/DomainParticipantListener.hpp>
#include <fastdds/dds/domain/qos/DomainParticipantQos.hpp>
#include <fastdds/rtps/common/Locator.h>

#include "subscriber_discovery.h"

#include <mutex>

using namespace eprosima::fastdds::dds;
using namespace eprosima::fastrtps::rtps;

EventType::Enum to_event_type(ReaderDiscoveryInfo::DISCOVERY_STATUS status){
    switch(status){
        case ReaderDiscoveryInfo::DISCOVERED_READER: return EventType::ADDED;
        case ReaderDiscoveryInfo::REMOVED_READER: return EventType::REMOVED;
        default: return EventType::UNKNOWN;
    }
}
ProtocolKind::Enum to_protocol_kind(int32_t kind){
    switch(kind){
        case LOCATOR_KIND_UDPv4: return ProtocolKind::UDPv4;
        case LOCATOR_KIND_UDPv6: return ProtocolKind::UDPv6;
        case LOCATOR_KIND_TCPv4: return ProtocolKind::TCPv4;
        case LOCATOR_KIND_TCPv6: return ProtocolKind::TCPv6;
        default: return ProtocolKind::UNKNOWN;
    }
}

// Based on rmw_fastrtps_shared_cpp/src/namespace_prefix.cpp
std::string remove_dds_prefix(const std::string & name){
    std::string prefix("rt/");
    if (name.rfind(prefix, 0) == 0) {
        return name.substr(prefix.length()-1);
    }
    return name + "_NOT_ROS_TOPIC";
}


// TODO: Race condition: we can miss discovery messages if we call add_watched topic late.
//       Maybe when adding new topic recreate the participant?
class DiscoveryListenerImpl : public DomainParticipantListener{
    size_t domain_id;
    DomainParticipant* listener_participant;
    std::mutex update_mutex;
    std::vector<std::string> watched_topics;
    std::vector<SubscriberEvent> events;
public:
    DiscoveryListenerImpl(size_t domain_id_):domain_id(domain_id_){
        listener_participant = DomainParticipantFactory::get_instance()->create_participant(domain_id, PARTICIPANT_QOS_DEFAULT, this, StatusMask::none());
        
    }
    ~DiscoveryListenerImpl(){
        DomainParticipantFactory::get_instance()->delete_participant(listener_participant);
    }
    void add_watched_topic(std::string name){
        std::lock_guard<std::mutex> lk(update_mutex);
        watched_topics.push_back(name);
        DomainParticipantFactory::get_instance()->delete_participant(listener_participant);
        listener_participant = DomainParticipantFactory::get_instance()->create_participant(domain_id, PARTICIPANT_QOS_DEFAULT, this, StatusMask::none());

    }
    void remove_watched_topic(std::string name){
        std::lock_guard<std::mutex> lk(update_mutex);
        std::remove(watched_topics.begin(),watched_topics.end(),name);
    }
    std::vector<SubscriberEvent> get_subscriber_changes(){
        std::vector<SubscriberEvent> returned_events;
        std::lock_guard<std::mutex> lk(update_mutex);
        returned_events.swap(events);
        return returned_events;
    }
    /*
	void on_publisher_discovery(DomainParticipant* dp, WriterDiscoveryInfo&& info){
		auto topic = info.info.topicName();
		//if (topic != "rt/test_UNIQUE_FLOW") return;
		
		std::cout << "WriterDiscoveryInfo\n";
		//std::cout << info.info.topicName() << "\n";
		std::cout << info.info.remote_locators() << "\n";
		
	}*/
	void on_subscriber_discovery(DomainParticipant* , ReaderDiscoveryInfo&& info){
        if(info.status == ReaderDiscoveryInfo::CHANGED_QOS_READER) return;
        auto topic_name = remove_dds_prefix(info.info.topicName().to_string());
        if(std::find(std::begin(watched_topics),std::end(watched_topics), topic_name) == std::end(watched_topics)) {
            //printf("Ignoring topic: %s\n", topic_name.c_str());
            return;
        }
        SubscriberEvent event;
        event.type = to_event_type(info.status);
        event.topic = topic_name;
        auto locators = info.info.remote_locators();
        
        std::lock_guard<std::mutex> lk(update_mutex);
        
        event.cast = CastType::MULTICAST;
        for(const auto& loc:locators.multicast){
            static_assert(sizeof(event.address) == sizeof(loc.address), "Address sizes are not the same");

            event.kind = to_protocol_kind(loc.kind);
            memcpy(&event.address,&loc.address,sizeof(event.address));
            event.port = loc.port;
            events.push_back(event);
        }
        event.cast = CastType::UNICAST;
        for(const auto& loc:locators.unicast){
            event.kind = to_protocol_kind(loc.kind);
            memcpy(&event.address,&loc.address,sizeof(event.address));
            event.port = loc.port;
            events.push_back(event);
        }

	}
};


DiscoveryListener::DiscoveryListener(size_t domain_id){
    impl = new DiscoveryListenerImpl(domain_id);
}
DiscoveryListener::~DiscoveryListener(){
    delete impl;
}
void DiscoveryListener::add_watched_topic(std::string name){
    impl->add_watched_topic(name);
}
void DiscoveryListener::remove_watched_topic(std::string name){
    impl->remove_watched_topic(name);
}

std::vector<SubscriberEvent> DiscoveryListener::get_subscriber_changes(){
    return impl->get_subscriber_changes();
}
     
