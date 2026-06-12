#include "Dto/ChatText.h"
#include <string>
#include "Sanitizer.h"
#include "include/base/cef_logging.h"
#include <iostream>

static constexpr float CHAR_WIDTH_AVG = 8.0f;
static constexpr float LINE_HEIGHT = 22.0f;
static constexpr float CODE_HEADER_H = 30.0f;
static constexpr float SEPARATOR_H = 12.0f;
static constexpr float MATH_PADDING = 12.0f;
static constexpr float WRAP_WIDTH = 1180.0f;


static int Utf8CharLength(unsigned char c) {
    if ((c & 0x80) == 0x00) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}
static float EstimateTextHeight(
    const std::string& text
    )
{
    int lines = 0;

    std::stringstream ss(text);
    std::string line;

    const int chars_per_line =
        (int)(WRAP_WIDTH / CHAR_WIDTH_AVG);

    while (std::getline(ss, line))
    {
        if (line.empty())
        {
            lines++;
            continue;
        }

        int wrapped =
            std::max(
                1,
                (int)((line.size() + chars_per_line - 1)
                    / chars_per_line));

        if (line[0] == '#')
            lines += wrapped + 1; 
        else
            lines += wrapped;
    }

    return lines * LINE_HEIGHT;
}
void Santizer(ChatMessage& chatmessage, const std::string& data) {
    size_t i = 0;
    size_t size = data.size();

    std::string curr;

    auto save = [&]() -> void {
        if (!curr.empty()) {
            RenderBlock rnd;
            rnd.type = BlockType::Text;
            rnd.content = curr;

            //float sz = ImGui::CalcTextSize(curr.c_str(), nullptr, false, 1162.0f).y;
            //rnd.blockheight += sz;
            //chatmessage.total_height += sz;
            chatmessage.blocks.push_back(rnd);
            curr.clear();
        }
        };

    auto codeblock = [&](size_t idx) -> bool {
        return (idx + 2 < size && data[idx] == '`' && data[idx + 1] == '`' && data[idx + 2] == '`');
        };

    auto separator = [&](size_t idx) -> bool {
        bool at_line_start = (idx == 0 || data[idx - 1] == '\n');
        return (at_line_start && idx + 2 < size && data[idx] == '-' && data[idx + 1] == '-' && data[idx + 2] == '-');
        };

    auto math_open = [&](size_t idx) -> bool {
        return (idx + 1 < size && data[idx] == '\\' && data[idx + 1] == '[');
        };

    auto math_close = [&](size_t idx) -> bool {
        return (idx + 1 < size && data[idx] == '\\' && data[idx + 1] == ']');
        };

    while (i < size) {
        if (codeblock(i)) {
            save();

            size_t j = i + 3;
            std::string lang;

            while (j < size && data[j] != '\n') {
                lang.push_back(data[j]);
                j++;
            }

            if (j < size && data[j] == '\n') {
                j++;
            }

            std::string code;
            while (j < size && !codeblock(j)) {
                int char_len = Utf8CharLength(static_cast<unsigned char>(data[j]));

                if (j + char_len > size) char_len = size - j;

                code.append(data, j, char_len);
                j += char_len;
            }

            int lines = 1;

            for (char c : code)
            {
                if (c == '\n')
                    lines++;
            }

            RenderBlock rnd;
            rnd.type = BlockType::Code;
            rnd.lang = lang;
            rnd.content = code;
            rnd.numlines = lines;
            rnd.ParentBlock = &chatmessage;

            //rnd.blockheight = ImGui::CalcTextSize(code.c_str(), nullptr, false, 0.0f).y + ImGui::GetTextLineHeight() + 16.0f;
            //chatmessage.total_height += rnd.blockheight;
            chatmessage.blocks.push_back(rnd);

            i = (j < size) ? j + 3 : size;
        }
        else if (math_open(i)) {
            save();

            size_t j = i + 2;
            std::string math;

            while (j < size && !math_close(j)) {
                int char_len = Utf8CharLength(static_cast<unsigned char>(data[j]));

                if (j + char_len > size) char_len = size - j;

                math.append(data, j, char_len);
                j += char_len;
            }

            RenderBlock rnd;
            rnd.type = BlockType::Math;
            rnd.content = math;
            rnd.ParentBlock = &chatmessage;

            //rnd.blockheight = ImGui::CalcTextSize(math.c_str(), nullptr, false, 1162.0f).y + 12.0f;
            //chatmessage.total_height += rnd.blockheight;
            chatmessage.blocks.push_back(rnd);

            i = (j < size) ? j + 2 : size;
        }
        else if (separator(i)) {
            save();

            RenderBlock rnd;
            rnd.type = BlockType::Separator;
            rnd.ParentBlock = &chatmessage;
            chatmessage.total_height += rnd.blockheight;
            chatmessage.blocks.push_back(rnd);

            i += 3;
        }
        else {
            int char_len = Utf8CharLength(static_cast<unsigned char>(data[i]));

            if (i + char_len > size) char_len = size - i;

            curr.append(data, i, char_len);
            i += char_len;
        }
    }

    save();
}