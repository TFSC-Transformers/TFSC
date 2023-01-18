#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <string>
#include <set>
#include "../utils/json.hpp"
#include "../utils/MagicSingleton.h"

#define SERVERMAINPORT (MagicSingleton<Config>::GetInstance()->GetServerPort())
#define CLIENTMAINPORT (SERVERMAINPORT - 10)

class Node;
class Config
{
public:
    struct Info
    {
        std::string logo;
        std::string name;
    };

    struct Log
    {
        bool console;
        std::string level;
        std::string path;
    };

    struct HttpCallback
    {
        std::string ip;
        std::string path;
        uint32_t port;
    };

    struct SyncData
    {
        int count = 200;
        int interval;
    };

    // file
    const std::string kConfigFilename = "config.json";

    // key
    const std::string kCfgHttpCallback = "http_callback";
    const std::string kCfgHttpCallbackIp = "ip";
    const std::string kCfgHttpCallbackPort = "port";
    const std::string kCfgHttpCallbackPath = "path";

    const std::string kCfgHttpPort = "http_port";

    const std::string kCfgInfo = "info";
    const std::string kCfgInfoLogo = "logo";
    const std::string kCfgInfoName = "name";

    const std::string kCfgIp = "ip";

    const std::string kCfgLog = "log";
    const std::string kCfgLogLevel = "level";
    const std::string kCfgLogPath = "path";
    const std::string kCfgLogConsole = "console";

    const std::string kCfgServerPort = "server_port";

    const std::string kCfgSyncData = "sync_data";
    const std::string kCfgSyncDataCount = "count"; 
    const std::string kCfgSyncDatainterval = "interval";

    const std::string kCfgThreadNumVersion = "thread_num";
    
    const std::string kCfgKeyVersion = "version";

public:
    Config()
    {
        InitFile();
    };
    Config(bool & flag)
    {
        int ret = InitFile();
        if ( ret == 0 ) flag = true;
    };
    ~Config(){};

    int InitFile();
    int verify(const Node& _node);
    int GetServerPort();
    int GetHttpPort();
    int GetThreadNum();
    std::string GetIP();
    int GetInfo(Config::Info & info);
    int SetIP(const std::string & ip);

    std::set<std::string> GetServer();

    std::string GetVersion();

    int GetLog(Config::Log & log);
    int GetSyncData(Config::SyncData & syncData);
    int GetHttpCallback(Config::HttpCallback & httpCallback);


private:
    int _Parse(nlohmann::json json);
    int verify();
private:
    Info _info;
    std::string _ip;
    Log _log;
    HttpCallback _httpCallback;
    uint32_t _httpPort;
    std::set<std::string> _server;
    uint32_t _serverPort;
    SyncData _syncData;
    uint32_t _threadNum;
    std::string _version;
};

#endif