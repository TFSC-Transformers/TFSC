
#include "unregister_node.h"
#include "net/peer_node.h"
#include "utils/MagicSingleton.h"
#include "common/global_data.h"
#include "common/config.h"
#include "net_api.h"
#include "net.pb.h"
#include "handle_event.h"
#include "net/peer_node_cache.h"

UnregisterNode::UnregisterNode()
{
}
UnregisterNode::~UnregisterNode()
{
}

int UnregisterNode::Add(const Node & node)
{
    std::unique_lock<std::shared_mutex> lck(_mutex_for_nodes);
    std::string key = std::to_string(node.public_ip) + std::to_string(node.public_port);

    if(key.size() == 0)
	{
		return -1;
	} 
	auto itr = _nodes.find(key);
	if (itr != _nodes.end())
	{
		return -2;
	}
	this->_nodes[key] = node;


    return 0;
}
    
int UnregisterNode::Register()
{
    std::unique_lock<std::shared_mutex> lck(_mutex_for_nodes);

    std::vector<Node> nodelist = MagicSingleton<PeerNode>::GetInstance()->get_nodelist();
    
    for (auto & unconnect_node : _nodes)
    {
        bool isFind = false;
        for (auto & node : nodelist)
        {
            if (unconnect_node.second.public_ip == node.public_ip && 
                unconnect_node.second.public_port == node.public_port)
            {
                isFind = true;
                break;
            }
        }
        
        if (isFind)
        {
            continue;
        }


        if (unconnect_node.second.fd > 0)
        {
            continue;
        }
    }

    _nodes.clear();

    return 0;
}

bool UnregisterNode::Register(std::map<uint32_t, Node> node_map)
{
    std::string msg_id;
    uint32 send_num = node_map.size();
    if (!GLOBALDATAMGRPTR2.CreateWait(5, send_num, msg_id))
    {
        return false;
    }

    std::vector<Node> nodelist = MagicSingleton<PeerNode>::GetInstance()->get_nodelist();
    
    for (auto & unconnect_node : node_map)
    {
        bool isFind = false;
        for (auto & node : nodelist)
        {
            if (unconnect_node.second.public_ip == node.public_ip)
            {
                isFind = true;
                break;
            }
        }
        
        if (isFind)
        {
            continue;
        }

        int ret = net_com::SendRegisterNodeReq(unconnect_node.second, msg_id, false);
        if(ret != 0)
        {
            ERRORLOG("SendRegisterNodeReq fail ret = {}", ret);
        }
    }

    std::vector<std::string> ret_datas;
    if (!GLOBALDATAMGRPTR2.WaitData(msg_id, ret_datas))// Wait for enough voting data to be received
    {
        if (ret_datas.empty())
        {
            ERRORLOG("wait Register time out send:{} recv:{}", send_num, ret_datas.size());
            return false;
        }
    }

    RegisterNodeAck registerNodeAck;
    for (auto &ret_data : ret_datas)
    {
        registerNodeAck.Clear();
        if (!registerNodeAck.ParseFromString(ret_data))
        {
            continue;
        }
        uint32_t ip = registerNodeAck.from_ip();
        uint32_t port = registerNodeAck.from_port();
        std::cout << "registerNodeAck.nodes_size(): " << registerNodeAck.nodes_size() <<std::endl;
        if(registerNodeAck.nodes_size() <= 1)
	    {
            const NodeInfo &nodeinfo = registerNodeAck.nodes(0);
            //  Determine if TCP is connected
			if (MagicSingleton<BufferCrol>::GetInstance()->is_exists(ip, port) /* && node.is_established()*/)
			{
                DEBUGLOG("handleRegisterNodeAck--FALSE from.ip: {}", IpPort::ipsz(ip));
                auto ret = VerifyRegisterNode(nodeinfo, ip, port);
                if(ret < 0)
                {
                    DEBUGLOG("VerifyRegisterNode error ret:{}", ret);
                }
			}
        }
    }
    return true;
}
bool UnregisterNode::StartRegisterNode(std::map<std::string, int> &server_list)
{
    std::string msg_id;
    uint32 send_num = server_list.size();
    if (!GLOBALDATAMGRPTR2.CreateWait(5, send_num, msg_id))
    {
        return false;
    }
    Node selfNode = MagicSingleton<PeerNode>::GetInstance()->get_self_node();
    for (auto & item : server_list)
	{
        //  The party actively establishing the connection
		Node node;
		node.public_ip = IpPort::ipnum(item.first);
		node.listen_ip = selfNode.listen_ip;
		node.listen_port = SERVERMAINPORT;

		if (item.first == global::local_ip)
		{
			continue;
		}

		int ret = net_com::SendRegisterNodeReq(node, msg_id, true);
        if(ret != 0)
        {
            ERRORLOG("StartRegisterNode error ret : {}", ret);
        }
	}

    std::vector<std::string> ret_datas;
    if (!GLOBALDATAMGRPTR2.WaitData(msg_id, ret_datas))// Wait for enough voting data to be received
    {
        if (ret_datas.empty())
        {
            ERRORLOG("wait StartRegisterNode time out send:{} recv:{}", send_num, ret_datas.size());
            return false;
        }
    }
    RegisterNodeAck registerNodeAck;
    std::map<uint32_t, Node> node_map;


    for (auto &ret_data : ret_datas)
    {
        registerNodeAck.Clear();
        if (!registerNodeAck.ParseFromString(ret_data))
        {
            continue;
        }

        uint32_t ip = registerNodeAck.from_ip();
        uint32_t port = registerNodeAck.from_port();
        uint32_t fd = registerNodeAck.fd();
        for (int i = 0; i < registerNodeAck.nodes_size(); i++)
	    {
            const NodeInfo &nodeinfo = registerNodeAck.nodes(i);
            {
                Node node;
                node.listen_ip = selfNode.listen_ip;
	            node.listen_port = SERVERMAINPORT;
                node.public_ip = nodeinfo.public_ip();
                node.base58address = nodeinfo.base58addr();
                MagicSingleton<PeerNodeCache>::GetInstance()->Add(node);
            }
            if(nodeinfo.base58addr() == selfNode.base58address)
            {
                continue;
            }
            if(i == 0)
            {
                 
                //Determine if TCP is connected
                if (MagicSingleton<BufferCrol>::GetInstance()->is_exists(ip, port) /* && node.is_established()*/)
                {
                    DEBUGLOG("handleRegisterNodeAck--TRUE from.ip: {}", IpPort::ipsz(ip));
                    auto ret = VerifyRegisterNode(nodeinfo, ip, port);
                    if(ret < 0)
                    {
                        DEBUGLOG("VerifyRegisterNode error ret:{}", ret);
                        MagicSingleton<PeerNode>::GetInstance()->disconnect_node(ip, port, fd);
                        continue;
                    }
                }
            }
            else
            {
                Node node;
                node.listen_ip = selfNode.listen_ip;
	            node.listen_port = SERVERMAINPORT;
                node.public_ip = nodeinfo.public_ip();
                DEBUGLOG("Add NodeList--TRUE ip: {}", IpPort::ipsz(node.public_ip));
                if(node_map.find(node.public_ip) == node_map.end())
                {
                    node_map[nodeinfo.public_ip()] = node;
                }
            } 
        }
    }
    Register(node_map);
    return true;
}

bool UnregisterNode::StartSyncNode()
{
    std::string msg_id;
    std::vector<Node> node_list = MagicSingleton<PeerNode>::GetInstance()->get_nodelist();
    uint32 send_num = node_list.size();
    if (!GLOBALDATAMGRPTR3.CreateWait(5, send_num, msg_id))
    {
        return false;
    }
    Node selfNode = MagicSingleton<PeerNode>::GetInstance()->get_self_node();
    
    for (auto & node : node_list)
    {
        //Determine if TCP is connected
        if (MagicSingleton<BufferCrol>::GetInstance()->is_exists(node.public_ip, node.public_port) /* && node.is_established()*/)
        {
            net_com::SendSyncNodeReq(node, msg_id);
        }
        else
        {
            DEBUGLOG("SendSyncNodeReq error id:{} ip:{} port:{}", node.base58address, IpPort::ipsz(node.public_ip), node.public_port);
        }
    }

    std::vector<std::string> ret_datas;
    if (!GLOBALDATAMGRPTR3.WaitData(msg_id, ret_datas))// Wait for enough voting data to be received
    {
        if (ret_datas.empty())
        {
            ERRORLOG("wait StartRegisterNode time out send:{} recv:{}", send_num, ret_datas.size());
            return false;
        }
    }
    SyncNodeAck syncNodeAck;
    std::map<uint32_t, Node> node_map;

    for (auto &ret_data : ret_datas)
    {
        syncNodeAck.Clear();
        if (!syncNodeAck.ParseFromString(ret_data))
        {
            continue;
        }
        for (int i = 0; i < syncNodeAck.nodes_size(); i++)
	    {
            const NodeInfo &nodeinfo = syncNodeAck.nodes(i);
            if(nodeinfo.base58addr() == selfNode.base58address)
            {
                continue;
            }
            Node node;
            node.listen_ip = selfNode.listen_ip;
            node.listen_port = SERVERMAINPORT;
            node.public_ip = nodeinfo.public_ip();
            node.base58address = nodeinfo.base58addr();
            MagicSingleton<PeerNodeCache>::GetInstance()->Add(node);

            if(node_map.find(node.public_ip) == node_map.end())
            {
                node_map[nodeinfo.public_ip()] = node;
            }
        }
    }
    Register(node_map);
    return true;
}