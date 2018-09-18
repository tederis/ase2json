#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <curl/curl.h>

#define ASE_BUFFER_SIZE 600000

enum
{
    ASE_PLAYER_COUNT = 0x0004,
    ASE_MAX_PLAYER_COUNT = 0x0008,
    ASE_GAME_NAME = 0x0010,
    ASE_SERVER_NAME = 0x0020,
    ASE_GAME_MODE = 0x0040,
    ASE_MAP_NAME = 0x0080,
    ASE_SERVER_VER = 0x0100,
    ASE_PASSWORDED = 0x0200,
    ASE_SERIALS = 0x0400,
    ASE_PLAYER_LIST = 0x0800,
    ASE_RESPONDING = 0x1000,
    ASE_RESTRICTION = 0x2000,
    ASE_SEARCH_IGNORE_SECTIONS = 0x4000,
    ASE_KEEP_FLAG = 0x8000,
    ASE_HTTP_PORT = 0x080000,
    ASE_SPECIAL = 0x100000,
};

union IPAddress
{
    struct
    {
        unsigned char a;
        unsigned char b;
        unsigned char c;
        unsigned char d;
    } Decomposed;
    unsigned int Value;
};

class ASEBuffer
{
public:
    template<typename T>
    void read(T& out)
    {
        if (position + sizeof(T) <= size)
        {
            char value[sizeof(T)];
            for (int i = 0; i < sizeof(T); ++i)
                value[i] = *(buffer + position + sizeof(T) - i - 1);
            out = *(T*)value;

            position += sizeof(T);
        }
    }
    void readString(std::string& str)
    {
        unsigned char len = 0;
        read(len);

        for (int i = 0; i < len; ++i)
        {
            char ch = buffer[position + i];
            if (ch == 0x22)
                str += "\\";
            str += ch;
        }

        position += len;
    }
    void write(char* data, size_t dataSize)
    {
        memcpy(buffer + position, data, dataSize);
        position += dataSize;
        size += dataSize;
    }
    void seek(unsigned int pos)
    {
        if (pos >= 0 && pos < size)
            position = pos;
    }
    unsigned int tell()
    {
        return position;
    }
    bool step(unsigned int value)
    {
        return position + value >= 0 && position + value < size;
    }
private:
    unsigned char buffer[ASE_BUFFER_SIZE];
    unsigned int position{};
    size_t size{};
};

struct Server
{
    IPAddress ip;
    unsigned short port{};
    unsigned short playersCount{};
    unsigned short maxPlayersCount{};
    std::string gameName;
    std::string serverName;
    std::string modeName;
    std::string mapName;
    std::string verName;
    unsigned char passworded;
    std::vector<std::string> players;
    unsigned short httpPort;
};

size_t parse_server(ASEBuffer& buffer, std::vector<Server>& servers)
{
    unsigned short count = 0;
    buffer.read(count);

    servers.reserve(count);

    while (buffer.step(6) && count--)
    {
        Server server;

        buffer.read(server.ip.Value);

        buffer.read(server.port);

        servers.push_back(server);
    }
}

size_t parse_server_v2(ASEBuffer& buffer, std::vector<Server>& servers)
{
    unsigned int flags = 0;
    buffer.read(flags);

    unsigned int sequenceNumber = 0;
    buffer.read(sequenceNumber);

    unsigned int count = 0;
    buffer.read(count);

    servers.reserve(count);

    while (buffer.step(6) && count--)
    {
        Server server;

        unsigned int startPos = buffer.tell();

        unsigned short len = 0;
        buffer.read(len);

        buffer.read(server.ip.Value);

        buffer.read(server.port);

        if ((flags & ASE_PLAYER_COUNT) != 0)
            buffer.read(server.playersCount);

        if ((flags & ASE_MAX_PLAYER_COUNT) != 0)
            buffer.read(server.maxPlayersCount);

        if ((flags & ASE_GAME_NAME) != 0)
            buffer.readString(server.gameName);


        if ((flags & ASE_SERVER_NAME) != 0)
            buffer.readString(server.serverName);

        if ((flags & ASE_GAME_MODE) != 0)
            buffer.readString(server.modeName);

        if ((flags & ASE_MAP_NAME) != 0)
            buffer.readString(server.mapName);

        if ((flags & ASE_SERVER_VER) != 0)
            buffer.readString(server.verName);

        if ((flags & ASE_PASSWORDED) != 0)
            buffer.read(server.passworded);

        unsigned char serials;
        if ((flags & ASE_SERIALS) != 0)
            buffer.read(serials);

        if ((flags & ASE_PLAYER_LIST) != 0)
        {
            unsigned short listSize = 0;
            buffer.read(listSize);

            for (int i = 0; i < listSize; ++i)
            {
                std::string playerName;
                buffer.readString(playerName);

                server.players.push_back(playerName);
            }
        }

        unsigned char noResponce;
        if ((flags & ASE_RESPONDING) != 0)
            buffer.read(noResponce);

        unsigned int restriction;
        if ((flags & ASE_RESTRICTION) != 0)
            buffer.read(restriction);

        if ((flags & ASE_SEARCH_IGNORE_SECTIONS) != 0)
        {
            unsigned char numItems = 0;
            buffer.read(numItems);

            // Skip
            buffer.seek(buffer.tell() + 2*numItems);
        }

        unsigned char keepFlag = 0;
        if ((flags & ASE_KEEP_FLAG) != 0)
            buffer.read(keepFlag);

        if ((flags & ASE_HTTP_PORT) != 0)
            buffer.read(server.httpPort);

        unsigned char specialFlags = 0;
        if ((flags & ASE_SPECIAL) != 0)
            buffer.read(specialFlags);

        buffer.seek(startPos + len);

        servers.push_back(server);
    }
}

void write_servers(const std::vector<Server>& servers, std::string& str, bool light = false)
{
    unsigned short playersCountTotal = 0;
    for(const auto& server : servers)
        playersCountTotal += server.playersCount;

    str += "{\n";
    str += "    \"serversCount\": " + std::to_string(servers.size()) + ",\n";
    str += "    \"playersCount\": " + std::to_string(playersCountTotal);

    if (!light)
    {
        str += ",\n";
        str += "    \"servers\": [\n";

        bool firstServer = true;
        for(const auto& server : servers)
        {
            if (!firstServer)
                str += ",\n";
            str += "        { ";

            str += "\"ip\": \"" + std::to_string(server.ip.Decomposed.a) + "." + std::to_string(server.ip.Decomposed.b)  + "." + std::to_string(server.ip.Decomposed.c) + "." + std::to_string(server.ip.Decomposed.d) + "\", ";
            str += "\"port\": " + std::to_string(server.port) + ", ";
            str += "\"playersCount\": " + std::to_string(server.playersCount) + ", ";
            str += "\"maxPlayersCount\": " + std::to_string(server.maxPlayersCount) + ", ";
            str += "\"gameName\": \"" + server.gameName + "\", ";
            str += "\"serverName\": \"" + server.serverName + "\", ";
            str += "\"modeName\": \"" + server.modeName + "\", ";
            str += "\"mapName\": \"" + server.mapName + "\", ";
            str += "\"version\": \"" + server.verName + "\", ";
            if (server.passworded != 0)
                str += "\"passworded\": true, ";
            else
                str += "\"passworded\": false, ";
            str += "\"players\": [";
            for (const auto& playerName : server.players)
            {
                str += "\"" + playerName + "\", ";
            }
            str += "], ";
            str += "\"httpPort\": " + std::to_string(server.httpPort);

            firstServer = false;

            str += " }";
        }

        str += "\n    ]";
    }

    str += "\n}";
}

size_t callback(char* content, size_t size, size_t nmemb, void* userdata)
{
    ((ASEBuffer*)userdata)->write(content, size * nmemb);

    return size * nmemb;
}

int main(int argc, char* argv[])
{
    ASEBuffer buffer;

    // Init cURL
    curl_global_init(CURL_GLOBAL_ALL);

    CURL* handle = curl_easy_init();

    curl_easy_setopt(handle, CURLOPT_URL, "https://master.mtasa.com/ase/mta/");
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, callback);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &buffer);

    curl_easy_perform(handle);

    curl_easy_cleanup(handle);
    curl_global_cleanup();

    // Switch back at the beginning
    buffer.seek(0);

    unsigned short ver = 0;
    unsigned short count = 0;

    buffer.read(count);
    if (count == 0)
        buffer.read(ver);

    std::vector<Server> servers;

    if (ver == 0)
        parse_server(buffer, servers);
    else if (ver == 2)
        parse_server_v2(buffer, servers);

    std::string output;
    bool lightData = argc > 1 && strcmp(argv[1], "-l") == 0;
    write_servers(servers, output, lightData);

    std::cout << output << std::endl;

    return 0;
}
