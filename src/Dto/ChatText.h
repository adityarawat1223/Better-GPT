#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include "FileRef.h"
enum class BlockType
{
    Text,
    Code,
    Math,
    Image,
    File,
    Thinking,
    Separator
};
struct ChatMessage;  



struct RenderBlock
{
    BlockType type;
    std::string content;
    std::string lang;
    int numlines = 1;    
    float blockheight = 0.0f;
    float width_calculated_at = 1180.0f;
    ChatMessage* ParentBlock;

};

struct ChatMessage
{
    bool user = false;
    std::vector<RenderBlock> blocks;
    std::string message_id;
    uint64_t timestamp = 0;
    std::vector<FileRef> assets;
    float total_height = 0.0f;
    std::string thinking;              
    std::vector<std::string> urls;
    bool is_system_or_tool = false;
    std::string raw_content;
};      