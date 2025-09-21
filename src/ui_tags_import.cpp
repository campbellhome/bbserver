// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#include "tags.h"
#include "ui_tags_import.h"
#include "va.h"

#include "parson/parson.h"
#include "wrap_imgui.h"
#include "wrap_imgui_internal.h"

sb_t s_openingPopup;
sbs_t s_selectedTags;
tags_config_t s_sourceTagConfig;

void UITagsImport_StartImport(const char* path)
{
	sb_reset(&s_openingPopup);
	sbs_reset(&s_selectedTags);
	tags_config_reset(&s_sourceTagConfig);

	JSON_Value* val = json_parse_file(path);
	if (val)
	{
		s_sourceTagConfig = json_deserialize_tags_config_t(val);
		json_value_free(val);
	}

	s_openingPopup = sb_from_c_string("Tag Import");
}

void UITagsImport_Shutdown()
{
	sb_reset(&s_openingPopup);
	sbs_reset(&s_selectedTags);
	tags_config_reset(&s_sourceTagConfig);
}

void UITagsImport_Apply()
{
	for (u32 tagIndex = 0; tagIndex < s_sourceTagConfig.tags.count; ++tagIndex)
	{
		tag_t *tag = s_sourceTagConfig.tags.data + tagIndex;
		const char* tagName = sb_get(&tag->name);

		bool bChecked = sbs_contains(&s_selectedTags, tag->name) != 0;
		if (!bChecked)
		{
			continue;
		}

		for (u32 categoryIndex = 0; categoryIndex < tag->categories.count; ++categoryIndex)
		{
			sb_t* category = tag->categories.data + categoryIndex;
			const char* categoryName = sb_get(category);
			tag_add_category(tagName, categoryName);
		}
	}

	tags_write();
	tag_apply_tag_to_all_views();
}

void UITagsImport_Update()
{
	if (sb_len(&s_openingPopup) > 0)
	{
		ImGui::OpenPopup(sb_get(&s_openingPopup));
		sb_reset(&s_openingPopup);
	}

	// Always center this window when appearing
	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

	if (ImGui::BeginPopupModal("Tag Import", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("Select Tags to import");
		ImGui::Separator();

		ImGuiTreeNodeFlags tagNodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;

		for (u32 tagIndex = 0; tagIndex < s_sourceTagConfig.tags.count; ++tagIndex)
		{
			tag_t *tag = s_sourceTagConfig.tags.data + tagIndex;

			bool bChecked = sbs_contains(&s_selectedTags, tag->name) != 0;
			ImGui::PushID((int)tagIndex);
			if (ImGui::Checkbox("", &bChecked))
			{
				if (bChecked)
				{
					sbs_add_unique(&s_selectedTags, tag->name);
				}
				else
				{
					sbs_remove_unique(&s_selectedTags, tag->name);
				}
			}
			ImGui::PopID();

			ImGui::SameLine();
			ImGui::BeginGroup();

			const char* tagName = sb_get(&tag->name);
			const char* resolvedTagName = tagName;
			/*
			if (tag->visibility == kTagVisibility_AlwaysVisible)
			{
				resolvedTagName = va("%s (visible)", resolvedTagName);
			}
			else if (tag->visibility == kTagVisibility_AlwaysHidden)
			{
				resolvedTagName = va("%s (hidden)", resolvedTagName);
			}
			if (tag->noColor)
			{
				resolvedTagName = va("%s (no color)", resolvedTagName);
			}
			*/
			resolvedTagName = va("%s##%s", resolvedTagName, resolvedTagName);
			bool bOpen = ImGui::TreeNodeEx(resolvedTagName, tagNodeFlags);
			if (bOpen)
			{
				for (u32 categoryIndex = 0; categoryIndex < tag->categories.count; ++categoryIndex)
				{
					sb_t* category = tag->categories.data + categoryIndex;
					const char* categoryName = sb_get(category);

					ImGui::PushID((int)categoryIndex);
					ImGui::TextUnformatted(categoryName);
					ImGui::PopID();
				}
				ImGui::TreePop();
			}

			ImGui::EndGroup();
		}

		ImGui::Separator();
		ImGui::BeginDisabled(s_selectedTags.count == 0);
		if (ImGui::Button("OK", ImVec2(120, 0)))
		{
			ImGui::CloseCurrentPopup();

			sb_reset(&s_openingPopup);
			s_openingPopup = sb_from_c_string("Confirm Tag Import");
		}
		ImGui::EndDisabled();
		ImGui::SetItemDefaultFocus();
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 0)))
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	if (ImGui::BeginPopupModal("Confirm Tag Import", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextUnformatted("Are you sure you want to import these tags?");
		ImGui::Separator();

		for (u32 i = 0; i < s_selectedTags.count; ++i)
		{
			const char* tagName = sb_get(s_selectedTags.data + i);
			ImGui::TextUnformatted(tagName);
		}

		ImGui::Separator();
		if (ImGui::Button("Yes", ImVec2(120, 0)))
		{
			UITagsImport_Apply();
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("No", ImVec2(120, 0)))
		{
			ImGui::CloseCurrentPopup();

			sb_reset(&s_openingPopup);
			s_openingPopup = sb_from_c_string("Tag Import");
		}
		ImGui::SetItemDefaultFocus();
		ImGui::EndPopup();
	}
}
