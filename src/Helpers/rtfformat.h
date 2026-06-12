#pragma once
#include <string>
#include <vector>
#include "imgui.h"
#include "imgui_markdown.h"

extern ImFont* g_FontMono;
extern ImGui::MarkdownConfig g_mdConfig;

class MarkdownRenderer {
public:
    static void Render(const std::string& text, const std::string& chatId) {
        size_t pos = 0;
        size_t blockIdx = 0;

        while (pos < text.length()) {
            size_t start_code = text.find("```", pos);

            if (start_code == std::string::npos) {
                std::string md_part = text.substr(pos);
                if (!md_part.empty()) {
                    ImGui::Markdown(md_part.c_str(), md_part.length(), g_mdConfig);
                }
                break;
            }

            std::string md_part = text.substr(pos, start_code - pos);
            if (!md_part.empty()) {
                ImGui::Markdown(md_part.c_str(), md_part.length(), g_mdConfig);
            }

            size_t end_code = text.find("```", start_code + 3);
            if (end_code == std::string::npos) {
                end_code = text.length();
            }
            else {
                end_code += 3;
            }

            std::string code_part = text.substr(start_code, end_code - start_code);
            size_t first_newline = code_part.find('\n');
            size_t last_backticks = code_part.rfind("```");

            std::string clean_code = code_part;
            if (first_newline != std::string::npos && last_backticks != std::string::npos) {
                clean_code = code_part.substr(first_newline + 1, last_backticks - first_newline - 1);
            }

            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.05f, 0.06f, 0.07f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 12));

            std::string child_id = "cb_" + chatId + "_" + std::to_string(blockIdx++);

            ImGui::PushFont(g_FontMono);
            ImVec4 text_color = ImGui::GetStyle().Colors[ImGuiCol_Text];
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.0f)); 

            ImVec2 textSize = ImGui::CalcTextSize(clean_code.c_str());
            float blockHeight = textSize.y + 24.0f;

            ImGui::BeginChild(child_id.c_str(), ImVec2(0, blockHeight), true, ImGuiWindowFlags_HorizontalScrollbar);
            ImGui::TextUnformatted(clean_code.c_str());
            ImGui::EndChild();

            ImGui::PopStyleColor(2);
            ImGui::PopFont();
            ImGui::PopStyleVar(2);

            pos = end_code;
        }
    }
};