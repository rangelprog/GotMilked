#if GM_DEBUG_TOOLS

#include "../DebugMenu.hpp"

#include <fmt/format.h>

namespace gm::debug {

void DebugMenu::RenderContentValidationWindow() {
    if (!m_showContentValidation) {
        return;
    }

    if (!ImGui::Begin("Content Validation", &m_showContentValidation)) {
        ImGui::End();
        return;
    }

    if (!m_contentDatabase) {
        ImGui::TextWrapped("Content database not available.");
        ImGui::End();
        return;
    }

    auto types = m_contentDatabase->GetRegisteredTypes();
    if (types.empty()) {
        ImGui::TextWrapped("No content schemas registered.");
        ImGui::End();
        return;
    }

    for (const auto& type : types) {
        auto records = m_contentDatabase->GetRecordsSnapshot(type);
        const std::size_t issueCount = std::count_if(records.begin(), records.end(), [](const content::ContentRecord& record) {
            return !record.valid;
        });

        const std::string header = fmt::format("{} ({}/{})", type, records.size() - issueCount, records.size());
        if (!ImGui::CollapsingHeader(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            continue;
        }

        if (records.empty()) {
            ImGui::TextDisabled("No records.");
            continue;
        }

        if (ImGui::BeginTable(fmt::format("ContentTable_{}", type).c_str(), 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 90.0f);
            ImGui::TableSetupColumn("Identifier", ImGuiTableColumnFlags_WidthFixed, 180.0f);
            ImGui::TableSetupColumn("File");
            ImGui::TableSetupColumn("Details");
            ImGui::TableHeadersRow();

            for (const auto& record : records) {
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                const ImVec4 color = record.valid ? ImVec4(0.35f, 0.75f, 0.35f, 1.0f)
                                                  : ImVec4(0.85f, 0.3f, 0.3f, 1.0f);
                const char* statusLabel = record.valid ? "Valid" : "Invalid";
                ImGui::PushStyleColor(ImGuiCol_Text, color);
                ImGui::TextUnformatted(statusLabel);
                ImGui::PopStyleColor();

                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(record.identifier.c_str());
                if (!record.displayName.empty() && record.displayName != record.identifier) {
                    ImGui::TextDisabled("%s", record.displayName.c_str());
                }

                ImGui::TableSetColumnIndex(2);
                ImGui::TextWrapped("%s", record.relativePath.c_str());

                ImGui::TableSetColumnIndex(3);
                if (record.valid) {
                    ImGui::TextDisabled("—");
                } else if (!record.issues.empty()) {
                    const auto& issue = record.issues.front();
                    ImGui::TextWrapped("%s: %s", issue.path.c_str(), issue.message.c_str());
                    if (record.issues.size() > 1 && ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary)) {
                        ImGui::BeginTooltip();
                        ImGui::TextUnformatted("All issues:");
                        for (const auto& extra : record.issues) {
                            ImGui::BulletText("%s: %s", extra.path.c_str(), extra.message.c_str());
                        }
                        ImGui::EndTooltip();
                    }
                }
            }

            ImGui::EndTable();
        }
    }

    ImGui::End();
}

} // namespace gm::debug

#endif // GM_DEBUG_TOOLS


