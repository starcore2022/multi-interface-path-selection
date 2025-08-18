#include <rclcpp/rclcpp.hpp>
#include <tcp_tunnel/srv/register_client.hpp>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include "semaphore.h"
//#include <string>

//Header for getting interface addresses
#include <ifaddrs.h>

const std::map<rclcpp::ReliabilityPolicy, std::string> RELIABILITY_POLICIES = {{rclcpp::ReliabilityPolicy::BestEffort,    "BestEffort"},
                                                                               {rclcpp::ReliabilityPolicy::Reliable,      "Reliable"},
                                                                               {rclcpp::ReliabilityPolicy::SystemDefault, "SystemDefault"},
                                                                               {rclcpp::ReliabilityPolicy::Unknown,       "Unknown"}};
const std::map<rclcpp::DurabilityPolicy, std::string> DURABILITY_POLICIES = {{rclcpp::DurabilityPolicy::Volatile,       "Volatile"},
                                                                             {rclcpp::DurabilityPolicy::TransientLocal, "TransientLocal"},
                                                                             {rclcpp::DurabilityPolicy::SystemDefault,  "SystemDefault"},
                                                                             {rclcpp::DurabilityPolicy::Unknown,        "Unknown"}};
const std::map<rclcpp::LivelinessPolicy, std::string> LIVELINESS_POLICIES = {{rclcpp::LivelinessPolicy::Automatic,     "Automatic"},
                                                                             {rclcpp::LivelinessPolicy::ManualByTopic, "ManualByTopic"},
                                                                             {rclcpp::LivelinessPolicy::SystemDefault, "SystemDefault"},
                                                                             {rclcpp::LivelinessPolicy::Unknown,       "Unknown"}};


// Search for eth/wifi interface
struct ifaddrs* find_interface(struct ifaddrs **ifap, const char* interface = "wlp"){
    struct ifaddrs *ifaddr_p = *ifap;
    while (ifaddr_p){
        if (strstr(ifaddr_p->ifa_name, interface) && !strstr(ifaddr_p->ifa_name,"veth") && ifaddr_p->ifa_addr->sa_family==AF_INET){
            struct sockaddr_in *sa;
            char *addr;
            sa = (struct sockaddr_in *) ifaddr_p->ifa_addr;
            addr = inet_ntoa(sa->sin_addr);
            printf("Family: %d\n", ifaddr_p->ifa_addr->sa_family);
            printf("Interface: %s\tAddress: %s\n", ifaddr_p->ifa_name, addr);
            return ifaddr_p;
        }
        ifaddr_p = ifaddr_p->ifa_next;
    }
    return nullptr;
}


void test(){
    struct ifaddrs *ifap, *ifa;
    struct sockaddr_in *sa;
    char *addr;

    getifaddrs (&ifap);
    for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family==AF_INET) {
            sa = (struct sockaddr_in *) ifa->ifa_addr;
            addr = inet_ntoa(sa->sin_addr);
            printf("Interface: %s\tAddress: %s\n", ifa->ifa_name, addr);
        }
    }

    freeifaddrs(ifap);
    return;
}


class TCPTunnelServer : public rclcpp::Node
{
public: 
    TCPTunnelServer(const char* topic2 = "", const char* interface2 = ""): //or enp63s0
            Node("tcp_tunnel_server")
    {
        ////Examples
        // enp0s10:
        // |  |  |
        // v  |  |
        // en |  |   --> ethernet
        //    |  |
        //    v  |
        //    p0 |   --> bus number (0)
        //       v
        //       s10 --> slot number (10)
        // wlp62s0:
        // |  |   |
        // v  |   |
        // wl |   |   --> wireless (WiFi)
        //    |   |
        //    v   |
        //    p62 |   --> bus number (62)
        //        v
        //        s0  --> slot number (0)
        ////

        struct ifaddrs* ifaddr_p2;
        struct sockaddr_in *sa;
        char *addr;
        getifaddrs(&ifaddr_p2);


        // For checking values and values being passed
        // printf("Given interface and topic (default: wlp62s0, pose_5G)\n");
        // printf("%s %s\n",interface2,topic2);

        //Chosen interface is Ethernet
        if (strstr(interface2,"eth")) {
            if (!find_interface(&ifaddr_p2, interface2)) {
                interface2 = "enp";
            }
            ifaddr_p2 = find_interface(&ifaddr_p2, interface2);
            if (!ifaddr_p2) {
                RCLCPP_ERROR_STREAM(this->get_logger(), "Error no interface: " << interface2 << " found.");
                freeifaddrs(ifaddr_p2);
                return;
            } else {
                sa = (struct sockaddr_in *) ifaddr_p2->ifa_addr;
                addr = inet_ntoa(sa->sin_addr);
                printf("Interface: %s\tAddress: %s\n", ifaddr_p2->ifa_name, addr);
                //freeifaddrs(ifaddr_p2);
            }
        } 
        //Chosen interface is Wireless
        else if (strstr(interface2,"enp")) {   
            if (!find_interface(&ifaddr_p2, interface2)) {
                interface2 = "eth";
            }
            
            ifaddr_p2 = find_interface(&ifaddr_p2, interface2);
            
            if (!ifaddr_p2) {
                RCLCPP_ERROR_STREAM(this->get_logger(), "Error no interface: " << interface2 << " found.");
                freeifaddrs(ifaddr_p2);
                return;
            } 
            else {
                sa = (struct sockaddr_in *) ifaddr_p2->ifa_addr;
                addr = inet_ntoa(sa->sin_addr);
                printf("Interface: %s\tAddress: %s\n", ifaddr_p2->ifa_name, addr);
                // freeifaddrs(ifaddr_p2);
            }
        }
        // //Chosen interface is 4G/5G 
        // else if (interface2 == "lte"){
        //     interface2 = "lte";
        //     if (!find_interface(&ifaddr_p2, "lte")) {
        //         interface2 = "lte";
        //     }
        // }
        
        std::string prefix = this->get_namespace();
        // std::cout << prefix << "\n";
        if(prefix.back() != '/')
        {
            prefix += "/";
        }
        if (prefix == "/" && (strstr(topic2,"5G") || (strstr(topic2,"LTE") || strstr(topic2,"4G") || strstr(topic2,"WL")))) {
            prefix += "pose_5G/";
        } 
        else if (prefix == "/" && (strstr(topic2,"ETH") || (strstr(topic2,"ENP")))) {
            prefix += "pose_ETH/";
        }
        std::cout << prefix <<"\n";
        registerClientService = this->create_service<tcp_tunnel::srv::RegisterClient>(prefix + "tcp_tunnel_server/register_client",
                                                                                      std::bind(&TCPTunnelServer::registerClientCallback, this, 
                                                                                                std::placeholders::_1, std::placeholders::_2, interface2));
    }

    ~TCPTunnelServer()
    {
        for(size_t i = 0; i < sockets.size(); ++i)
        {
            if(confirmationThreads[i].joinable())
            {
                confirmationSemaphores[i]->wakeUp();
                confirmationThreads[i].join();
            }
            if(socketStatuses[i])
            {
                close(sockets[i]);
            }
        }
    }

private:
    void registerClientCallback(const std::shared_ptr<tcp_tunnel::srv::RegisterClient::Request> req, std::shared_ptr<tcp_tunnel::srv::RegisterClient::Response> res, const char* interface)
    {
        std::string topicName = req->topic.data;

        std::string interf;
        if ((req->interface.data != "")) {
            interf = req->interface.data;
        } else {
            interf = interface;
        }
        

        // find interfaces
        struct ifaddrs* ifaddr_p;
        if(getifaddrs(&ifaddr_p) < 0) {
            RCLCPP_ERROR_STREAM(this->get_logger(), "Error \"" << strerror(errno) << "\" occurred while getting network interfaces.");
            return;
        }



        // create socket
        int sockfd;
        struct sockaddr_in serv_addr;

        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if(sockfd < 0)
        {
            RCLCPP_ERROR_STREAM(this->get_logger(), "Error \"" << strerror(errno) << "\" occurred while creating a socket for topic " << topicName << ".");
            return;
        }

        struct ifaddrs* ifaddr;
        // Interface was chosen
        if (interf != "") {
            ifaddr = find_interface(&ifaddr_p, interf.c_str());
            // Wrong interface name
            if(!ifaddr || !ifaddr->ifa_name) {
                RCLCPP_ERROR_STREAM(this->get_logger(), "Error \"" << strerror(errno) << "\" occurred, there is no such network interface: " << interf << ".");
                return;
            } 
            // Desired interface found
            else {
                //Bind to specific interface
                if(setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, ifaddr->ifa_name, sizeof(ifaddr->ifa_name)) < 0)
                {
                    RCLCPP_INFO_STREAM(this->get_logger(), "Tried binding to specific interface: " << ifaddr->ifa_name << " \n");
                    RCLCPP_ERROR_STREAM(this->get_logger(), "Error \"" << strerror(errno) << "\" occurred while setting socket options for topic " << topicName << ".");
                    close(sockfd);
                    return;
                }
            }
        }
        

        //Disable Nagle's alogrithm, low latency is the aim, not efficient bandwidth usage
        int flag = 1;
        if(setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int)) < 0)
        {
            RCLCPP_INFO_STREAM(this->get_logger(), "Tried setting TCP option to TCP_NODELAY: minimal delay, but not most efficient BW usage.\n");
            RCLCPP_ERROR_STREAM(this->get_logger(), "Error \"" << strerror(errno) << "\" occurred while setting socket options for topic " << topicName << ".");
            close(sockfd);
            return;
        }

        if(fcntl(sockfd, F_SETFL, O_NONBLOCK) < 0)
        {
            RCLCPP_INFO_STREAM(this->get_logger(), "Tried setting fcntl() to 0_NONBLOCK.\n");
            RCLCPP_ERROR_STREAM(this->get_logger(), "Error \"" << strerror(errno) << "\" occurred while trying to set a socket flag for topic " << topicName << ".");
            close(sockfd);
            return;
        }

        // connect to client
        int n = -1;
        bzero(&serv_addr, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = inet_addr(req->client_ip.data.c_str());
        serv_addr.sin_port = htons(req->client_port.data);
        std::chrono::time_point<std::chrono::steady_clock> startTime = std::chrono::steady_clock::now();
        while(std::chrono::steady_clock::now() - startTime < std::chrono::duration<float>(3) && n < 0)
        {
            n = connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
        }
        if(n < 0)
        {
            RCLCPP_ERROR_STREAM(this->get_logger(),
                                "Error \"" << strerror(errno) << "\" occurred while trying to connect to " << req->client_ip.data << " on port " << req->client_port.data
                                           << " for topic " << topicName << ".");
            close(sockfd);
            return;
        }

        // fetch topic info
        if(this->get_topic_names_and_types().count(topicName) == 0 || this->get_publishers_info_by_topic(topicName).empty())
        {
            //Check whether topic exists
            auto topics = this->get_topic_names_and_types();
            for (const auto& topic_it : topics) {
                std::string topic_name = topic_it.first;
                std::vector<std::string> types = topic_it.second;
                for (const auto& type: types)
                    RCLCPP_INFO_STREAM(this->get_logger(), "Topic: " << topic_name.c_str() << ", type: " << type.c_str() << "\n");
            }
            std::cout << "Error: check if statement\n" << topicName << "\n this->get_topic_names_and_types().count(topicName): " << this->get_topic_names_and_types().count(topicName) << "\n";
            std::cout << "this->get_publishers_info_by_topic(topicName).empty(): " << this->get_publishers_info_by_topic(topicName).empty() << "\n";
            std::cout << "If topic is in the list, but not found by (this->get_topic_names_and_types().count(topicName)), \nthan the issue is the missing '/' in the beginning of the topicName.\n";
            RCLCPP_ERROR_STREAM(this->get_logger(), "Cannot add topic " << topicName << " to TCP tunnel, this topic doesn't exist.");
            close(sockfd);
            res->topic_exists.data = false;
            return;
        }
        std::string topicType = this->get_topic_names_and_types()[topicName][0];
        rclcpp::QoS qos = this->get_publishers_info_by_topic(topicName)[0].qos_profile();

        sockets.push_back(sockfd);
        socketStatuses.push_back(true);

        // initialize confirmation thread
        confirmationSemaphores.emplace_back(std::make_unique<Semaphore>(req->tunnel_queue_size.data));
        confirmationThreads.emplace_back(&TCPTunnelServer::receiveConfirmationLoop, this, confirmationThreads.size());

        // create subscription
        subscriptions.push_back(this->create_generic_subscription(topicName, topicType, qos.keep_last(1),
                                                                  std::bind(&TCPTunnelServer::subscriptionCallback, this, std::placeholders::_1, subscriptions.size())));

        // return topic info to client
        res->topic_exists.data = true;
        res->topic_type.data = topicType;
        res->reliability_policy.data = RELIABILITY_POLICIES.at(qos.reliability());
        res->durability_policy.data = DURABILITY_POLICIES.at(qos.durability());
        res->liveliness_policy.data = LIVELINESS_POLICIES.at(qos.liveliness());

        RCLCPP_INFO_STREAM(this->get_logger(), "Successfully registered client for topic " << topicName << ".");
        
        // no memory leak allowed
        freeifaddrs(ifaddr_p);
    }

    void subscriptionCallback(std::shared_ptr<rclcpp::SerializedMessage> msg, const int& subscriptionId)
    {
        if(!confirmationSemaphores[subscriptionId]->tryAcquire())
        {
            return;
        }
        if(!writeToSocket(subscriptionId, &msg->get_rcl_serialized_message().buffer_length, sizeof(size_t)))
        {
            return;
        }
        if(!writeToSocket(subscriptionId, msg->get_rcl_serialized_message().buffer, msg->get_rcl_serialized_message().buffer_length))
        {
            return;
        }
    }

    bool writeToSocket(const int& socketId, const void* buffer, const size_t& nbBytesToWrite, const bool& isMainThread = true)
    {
        size_t nbBytesWritten = 0;
        while(rclcpp::ok() && nbBytesWritten < nbBytesToWrite)
        {
            int n = send(sockets[socketId], ((char*)buffer) + nbBytesWritten, nbBytesToWrite - nbBytesWritten, MSG_NOSIGNAL);
            if(n >= 0)
            {
                nbBytesWritten += n;
            }
            else if(errno == EPIPE || errno == ECONNRESET || errno == EBADF)
            {
                if(isMainThread)
                {
                    RCLCPP_INFO_STREAM(this->get_logger(), "Connection closed by client for topic " << subscriptions[socketId]->get_topic_name() << ".");
                    socketStatuses[socketId] = false;
                    subscriptions[socketId].reset();
                    close(sockets[socketId]);
                }
                else
                {
                    confirmationSemaphores[socketId]->release();
                }
                return false;
            }
            else if(errno == EWOULDBLOCK || errno == EAGAIN)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            else
            {
                throw std::runtime_error(
                        std::string("Error \"") + strerror(errno) + "\" occurred while writing to socket for topic " + subscriptions[socketId]->get_topic_name() + ".");
            }
        }
        return nbBytesWritten == nbBytesToWrite;
    }

    void receiveConfirmationLoop(int threadId)
    {
        while(rclcpp::ok())
        {
            if(!readFromSocket(threadId, &confirmationBuffer, sizeof(char), false))
            {
                return;
            }
            confirmationSemaphores[threadId]->release();
        }
    }

    bool readFromSocket(const int& socketId, void* buffer, const size_t& nbBytesToRead, const bool& isMainThread = true)
    {
        size_t nbBytesRead = 0;
        while(rclcpp::ok() && nbBytesRead < nbBytesToRead)
        {
            int n = read(sockets[socketId], ((char*)buffer) + nbBytesRead, nbBytesToRead - nbBytesRead);
            if(n > 0)
            {
                nbBytesRead += n;
            }
            else if(n == 0 || errno == ECONNRESET || errno == EBADF)
            {
                if(isMainThread)
                {
                    RCLCPP_INFO_STREAM(this->get_logger(), "Connection closed by client for topic " << subscriptions[socketId]->get_topic_name() << ".");
                    socketStatuses[socketId] = false;
                    subscriptions[socketId].reset();
                    close(sockets[socketId]);
                }
                else
                {
                    confirmationSemaphores[socketId]->release();
                }
                return false;
            }
            else if(errno == EWOULDBLOCK || errno == EAGAIN)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            else
            {
                throw std::runtime_error(
                        std::string("Error \"") + strerror(errno) + "\" occurred while reading from socket for topic " + subscriptions[socketId]->get_topic_name() + ".");
            }
        }
        return nbBytesRead == nbBytesToRead;
    }

    rclcpp::Service<tcp_tunnel::srv::RegisterClient>::SharedPtr registerClientService;
    std::vector<rclcpp::GenericSubscription::SharedPtr> subscriptions;
    std::vector<int> sockets;
    std::vector<bool> socketStatuses;
    std::vector<std::thread> confirmationThreads;
    std::vector<std::unique_ptr<Semaphore>> confirmationSemaphores;
    char confirmationBuffer;
};

int main(int argc, char** argv)
{
    //test();
    // const char* interface2 = argv[2];
    // if (1) {
    //     struct ifaddrs* ifaddr_p2;
    //     struct sockaddr_in *sa;
    //     char *addr;
    //     struct ifaddrs* ifa;
    //     getifaddrs(&ifaddr_p2);
    //     ifa = find_interface(&ifaddr_p2, interface2);
    //     if (ifa) {
    //         printf("Found interface\n");
    //         sa = (struct sockaddr_in *) ifa->ifa_addr;
    //         addr = inet_ntoa(sa->sin_addr);
    //         printf("\tInterface: %s\tAddress: %s\n", ifa->ifa_name, addr);
    //     } else {
    //         printf("\tNo specified interface found: %s\n", interface2);
    //     }
    //     freeifaddrs(ifaddr_p2);
    //     freeifaddrs(ifa);
    // }


    // For checking input
    // std::cout << argv[1] << "\n" << argv[2] << "\n" << argc<< "\n";

    std::shared_ptr<rclcpp::Node> node /*= std::make_shared<TCPTunnelServer>()*/;
    rclcpp::init(argc, argv);
    if (argv && argc > 1) {
        if (argc > 2){
            node = std::make_shared<TCPTunnelServer>(argv[1], argv[2]);
        }
        else {
            node = std::make_shared<TCPTunnelServer>(argv[1], "");
            // if (strstr(argv[1],"wlp") || strstr(argv[1],"enp") || strstr(argv[1],"eth")) {
            //     node = std::make_shared<TCPTunnelServer>("/pose_5G", argv[1]);
            // } else {
            //     node = std::make_shared<TCPTunnelServer>(argv[1], "");
            // }
        }
    }
    else {
        node = std::make_shared<TCPTunnelServer>("","");
    }
    
    rclcpp::executors::MultiThreadedExecutor executor;
    executor.add_node(node);
    executor.spin();
    rclcpp::shutdown();
    return 0;
}
