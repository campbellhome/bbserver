// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#include "ui_tags.h"
#include "bb_array.h"
#include "imgui_tooltips.h"
#include "imgui_utils.h"
#include "recorded_session.h"
#include "tags.h"
#include "ui_loglevel_colorizer.h"
#include "va.h"
#include "view.h"
#include "view_config.h"
#include "wrap_imgui_internal.h"

static sb_t s_newTagName;
static sb_t s_categoryTagNames;
static view_category_collection_t s_matching;
static view_category_collection_t s_unmatching;

void UITags_Shutdown()
{
	sb_reset(&s_newTagName);
	sb_reset(&s_categoryTagNames);
	bba_free(s_matching.viewCategories);
	bba_free(s_matching.configCategories);
	bba_free(s_unmatching.viewCategories);
	bba_free(s_unmatching.configCategories);
}

static void TooltipLevelText(const char *fmt, u32 count, bb_log_level_e logLevel)
{
	if(count || logLevel == kBBLogLevel_Log) {
		LogLevelColorizer colorizer(logLevel);
		ImGui::Text(fmt, count);
	}
}

static void UITags_CategoryToolTip(recorded_category_t *category)
{
	if(ImGui::IsTooltipActive()) {
		ImGui::BeginTooltip();
		tagCategory_t *tagCategory = tagCategory_find(category->categoryName);
		if(tagCategory && tagCategory->tags.count) {
			sb_clear(&s_categoryTagNames);
			for(u32 i = 0; i < tagCategory->tags.count; ++i) {
				sb_va(&s_categoryTagNames, " %s", sb_get(tagCategory->tags.data + i));
			}
			ImGui::Text("Tags: %s", sb_get(&s_categoryTagNames));
			ImGui::Separator();
		}
		TooltipLevelText("VeryVerbose: %u", category->logCount[kBBLogLevel_VeryVerbose], kBBLogLevel_VeryVerbose);
		TooltipLevelText("Verbose: %u", category->logCount[kBBLogLevel_Verbose], kBBLogLevel_Verbose);
		TooltipLevelText("Logs: %u", category->logCount[kBBLogLevel_Log], kBBLogLevel_Log);
		TooltipLevelText("Display: %u", category->logCount[kBBLogLevel_Display], kBBLogLevel_Display);
		TooltipLevelText("Warnings: %u", category->logCount[kBBLogLevel_Warning], kBBLogLevel_Warning);
		TooltipLevelText("Errors: %u", category->logCount[kBBLogLevel_Error], kBBLogLevel_Error);
		TooltipLevelText("Fatal: %u", category->logCount[kBBLogLevel_Fatal], kBBLogLevel_Fatal);
		ImGui::EndTooltip();
	}
}

static void UITags_TagPopup(tag_t *tag, view_t *view)
{
	const char *tagName = sb_get(&tag->name);
	if(ImGui::BeginPopupContextItem(va("TagPopup%s", tagName))) {
		if(!tag->categories.count) {
			if(ImGui::MenuItem("Remove tag")) {
				tag_remove(tag);
				tags_write();
			}
		}

		if(ImGui::MenuItem("Show tag")) {
			view_set_category_collection_visiblity(&s_matching, true);
		}
		if(ImGui::MenuItem("Show only tag")) {
			view_set_category_collection_visiblity(&s_matching, true);
			view_set_category_collection_visiblity(&s_unmatching, false);
		}
		if(ImGui::MenuItem("Hide tag")) {
			view_set_category_collection_visiblity(&s_matching, false);
		}
		if(ImGui::MenuItem("Select tag categories")) {
			view_set_category_collection_selection(&s_matching, true);
		}
		if(ImGui::MenuItem("Select only tag categories")) {
			view_set_category_collection_selection(&s_matching, true);
			view_set_category_collection_selection(&s_unmatching, false);
		}
		if(ImGui::MenuItem("Unselect tag categories")) {
			view_set_category_collection_selection(&s_matching, false);
		}

		bool allEnabled = true;
		bool allDisabled = true;
		for(u32 i = 0; i < s_matching.viewCategories.count; ++i) {
			if(s_matching.viewCategories.data[i]->disabled) {
				allEnabled = false;
			} else {
				allDisabled = false;
			}
		}
		for(u32 i = 0; i < s_matching.configCategories.count; ++i) {
			if(s_matching.configCategories.data[i]->disabled) {
				allEnabled = false;
			} else {
				allDisabled = false;
			}
		}

		if(!allEnabled) {
			if(ImGui::MenuItem("Enable tag")) {
				view_set_category_collection_disabled(&s_matching, false);
			}
		}
		if(!allDisabled) {
			if(ImGui::MenuItem("Disable tag")) {
				view_set_category_collection_disabled(&s_matching, true);
			}
		}
		if(ImGui::MenuItem("Populate with visible categories")) {
			while(tag->categories.count) {
				tag_remove_category(tagName, sb_get(tag->categories.data));
			}
			for(u32 viewCategoryIndex = 0; viewCategoryIndex < view->categories.count; ++viewCategoryIndex) {
				view_category_t *vc = view->categories.data + viewCategoryIndex;
				if(vc->visible) {
					tag_add_category(tagName, vc->categoryName);
				}
			}
			for(u32 configCategoryIndex = 0; configCategoryIndex < view->config.configCategories.count; ++configCategoryIndex) {
				view_config_category_t *vc = view->config.configCategories.data + configCategoryIndex;
				if(vc->visible) {
					tag_add_category(tagName, sb_get(&vc->name));
				}
			}
			tags_write();
		}
		ImGui::EndPopup();
	}
}

static void UITags_Category_ClearSelection(view_t *view)
{
	for(u32 i = 0; i < view->categories.count; ++i) {
		view_category_t *viewCategory = view->categories.data + i;
		viewCategory->selected = false;
	}
	for(u32 i = 0; i < view->config.configCategories.count; ++i) {
		view_config_category_t *configCategory = view->config.configCategories.data + i;
		configCategory->selected = false;
	}
	view->lastCategoryClickIndex = ~0U;
}

static void UITags_Category_AddSelection(view_t *view, u32 viewCategoryIndex)
{
	view_category_t *viewCategory = view->categories.data + viewCategoryIndex;
	viewCategory->selected = true;
	view->lastCategoryClickIndex = viewCategoryIndex;
}

static void UITags_Category_ToggleSelection(view_t *view, u32 viewCategoryIndex)
{
	view_category_t *viewCategory = view->categories.data + viewCategoryIndex;
	viewCategory->selected = !viewCategory->selected;
	view->lastCategoryClickIndex = viewCategory->selected ? viewCategoryIndex : ~0u;
}

static void UITags_Category_HandleClick(view_t *view, u32 viewCategoryIndex)
{
	ImGuiIO &io = ImGui::GetIO();
	if(io.KeyAlt || (io.KeyCtrl && io.KeyShift))
		return;

	if(io.KeyCtrl) {
		UITags_Category_ToggleSelection(view, viewCategoryIndex);
	} else if(io.KeyShift) {
		if(view->lastCategoryClickIndex < view->categories.count) {
			u32 startIndex = view->lastCategoryClickIndex;
			u32 endIndex = viewCategoryIndex;
			view->lastCategoryClickIndex = viewCategoryIndex;
			if(endIndex < startIndex) {
				u32 tmp = endIndex;
				endIndex = startIndex;
				startIndex = tmp;
			}
			for(u32 i = startIndex; i <= endIndex; ++i) {
				view->categories.data[i].selected = true;
			}
		} else {
			UITags_Category_AddSelection(view, viewCategoryIndex);
		}
	} else {
		UITags_Category_ClearSelection(view);
		UITags_Category_AddSelection(view, viewCategoryIndex);
	}
}

static b32 UITags_SelectedCategories_AllHaveTag(view_t *view, tag_t *tag)
{
	for(u32 viewCategoryIndex = 0; viewCategoryIndex < view->categories.count; ++viewCategoryIndex) {
		view_category_t *viewCategory = view->categories.data + viewCategoryIndex;
		if(viewCategory->selected) {
			if(!sbs_contains(&tag->categories, sb_from_c_string_no_alloc(viewCategory->categoryName))) {
				return false;
			}
		}
	}
	return true;
}

static b32 UITags_SelectedCategories_AnyHaveTag(view_t *view, tag_t *tag)
{
	for(u32 viewCategoryIndex = 0; viewCategoryIndex < view->categories.count; ++viewCategoryIndex) {
		view_category_t *viewCategory = view->categories.data + viewCategoryIndex;
		if(viewCategory->selected) {
			if(sbs_contains(&tag->categories, sb_from_c_string_no_alloc(viewCategory->categoryName))) {
				return true;
			}
		}
	}
	return false;
}

static void UITags_SelectedCategories_AddTag(view_t *view, const char *tagName)
{
	for(u32 viewCategoryIndex = 0; viewCategoryIndex < view->categories.count; ++viewCategoryIndex) {
		view_category_t *viewCategory = view->categories.data + viewCategoryIndex;
		if(viewCategory->selected) {
			tag_add_category(tagName, viewCategory->categoryName);
		}
	}
	tags_write();
}

static void UITags_SelectedCategories_RemoveTag(view_t *view, const char *tagName)
{
	for(u32 viewCategoryIndex = 0; viewCategoryIndex < view->categories.count; ++viewCategoryIndex) {
		view_category_t *viewCategory = view->categories.data + viewCategoryIndex;
		if(viewCategory->selected) {
			tag_remove_category(tagName, viewCategory->categoryName);
		}
	}
	tags_write();
}

static void UITags_CategoryPopup(view_t *view, u32 viewCategoryIndex)
{
	view_category_t *viewCategory = view->categories.data + viewCategoryIndex;
	const char *categoryName = viewCategory->categoryName;
	if(ImGui::BeginPopupContextItem(va("%sContextMenu", categoryName))) {
		if(!viewCategory->selected) {
			UITags_Category_ClearSelection(view);
			UITags_Category_AddSelection(view, viewCategoryIndex);
		}

		if(ImGui::BeginMenu("Add Tag")) {
			for(u32 i = 0; i < g_tags.tags.count; ++i) {
				tag_t *tag = g_tags.tags.data + i;
				if(!UITags_SelectedCategories_AllHaveTag(view, tag)) {
					const char *tagName = sb_get(&tag->name);
					if(ImGui::MenuItem(tagName)) {
						UITags_SelectedCategories_AddTag(view, tagName);
					}
				}
			}
			ImGui::TextUnformatted("New: ");
			ImGui::SameLine();
			if(ImGui::InputText("##NewTag", &s_newTagName, 64, ImGuiInputTextFlags_EnterReturnsTrue)) {
				UITags_SelectedCategories_AddTag(view, sb_get(&s_newTagName));
				sb_reset(&s_newTagName);
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndMenu();
		}

		b32 bAnyHaveTags = false;
		for(u32 i = 0; i < g_tags.tags.count; ++i) {
			tag_t *tag = g_tags.tags.data + i;
			if(UITags_SelectedCategories_AnyHaveTag(view, tag)) {
				bAnyHaveTags = true;
				break;
			}
		}

		if(bAnyHaveTags) {
			if(ImGui::BeginMenu("Remove Tag")) {
				for(u32 i = 0; i < g_tags.tags.count; ++i) {
					tag_t *tag = g_tags.tags.data + i;
					if(UITags_SelectedCategories_AnyHaveTag(view, tag)) {
						const char *tagName = sb_get(&tag->name);
						if(ImGui::MenuItem(tagName)) {
							UITags_SelectedCategories_RemoveTag(view, tagName);
						}
					}
				}
				ImGui::EndMenu();
			}
		}

		if(viewCategory->disabled) {
			if(ImGui::MenuItem("Enable")) {
				view_set_category_collection_disabled(&s_matching, false);
			}
		} else {
			if(ImGui::MenuItem("Disable")) {
				view_set_category_collection_disabled(&s_matching, true);
			}
		}
		ImGui::EndPopup();
	}
}

static void UITags_Category_SetSelectedVisibility(view_t *view, b32 visible)
{
	view->visibleLogsDirty = true;
	for(u32 viewCategoryIndex = 0; viewCategoryIndex < view->categories.count; ++viewCategoryIndex) {
		view_category_t *viewCategory = view->categories.data + viewCategoryIndex;
		if(viewCategory->selected) {
			viewCategory->visible = visible;
		}
	}
}

static void UITags_CountCategoryVisibility(view_t *view, tag_t *tag, u32 *visibleCount, u32 *hiddenCount)
{
	u32 numVisible = 0;
	u32 numHidden = 0;
	for(u32 categoryIndex = 0; categoryIndex < tag->categories.count; ++categoryIndex) {
		sb_t *category = tag->categories.data + categoryIndex;
		const char *categoryName = sb_get(category);
		view_category_t *viewCategory = view_find_category_by_name(view, categoryName);
		view_config_category_t *configCategory = view_find_config_category(view, categoryName);

		if(viewCategory) {
			if(viewCategory->visible) {
				++numVisible;
			} else {
				++numHidden;
			}
		} else if(configCategory) {
			if(configCategory->visible) {
				++numVisible;
			} else {
				++numHidden;
			}
		} else {
			continue;
		}
	}
	*visibleCount = numVisible;
	*hiddenCount = numHidden;
}

void UITags_Update(view_t *view)
{
	ImGuiTreeNodeFlags tagNodeFlags = ImGuiTreeNodeFlags_OpenOnArrow |
	                                  ImGuiTreeNodeFlags_OpenOnDoubleClick;

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, 0.0f));
	if(ImGui::CollapsingHeader("Tags", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::PushID("TagsHeader");
		for(u32 tagIndex = 0; tagIndex < g_tags.tags.count; ++tagIndex) {
			tag_t *tag = g_tags.tags.data + tagIndex;
			const char *tagName = sb_get(&tag->name);
			bool bTagSelected = false;

			view_collect_categories_by_tag(view, &s_matching, &s_unmatching, tag);

			u32 numVisible = 0;
			u32 numHidden = 0;
			UITags_CountCategoryVisibility(view, tag, &numVisible, &numHidden);

			bool allChecked = numHidden == 0;
			if(numVisible && numHidden) {
				ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, true);
			}
			ImGui::PushID((int)tagIndex);
			if(ImGui::Checkbox("", &allChecked)) {
				if(ImGui::GetIO().KeyCtrl) {
					allChecked = true;
					view_set_all_category_visibility(view, false);
				}
				view_set_category_collection_visiblity(&s_matching, allChecked);
			}
			ImGui::PopID();
			if(numVisible && numHidden) {
				ImGui::PopItemFlag();
			}
			ImGui::SameLine();
			ImGui::BeginGroup();

			bool open = ImGui::TreeNodeEx(tagName, tagNodeFlags | (bTagSelected ? ImGuiTreeNodeFlags_Selected : 0));

			if(ImGui::IsItemClicked() && ImGui::GetIO().KeyCtrl) {
				view_set_all_category_visibility(view, false);
				view_set_category_collection_visiblity(&s_matching, true);
			}
			UITags_TagPopup(tag, view);

			if(open) {
				for(u32 categoryIndex = 0; categoryIndex < tag->categories.count; ++categoryIndex) {
					sb_t *category = tag->categories.data + categoryIndex;
					const char *categoryName = sb_get(category);

					view_category_t *viewCategory = view_find_category_by_name(view, categoryName);
					view_config_category_t *configCategory = view_find_config_category(view, categoryName);

					b32 *visible = NULL;
					b32 *selected = NULL;
					if(viewCategory) {
						visible = &viewCategory->visible;
						selected = &viewCategory->selected;
					} else if(configCategory) {
						visible = &configCategory->visible;
						selected = &configCategory->selected;
					} else {
						continue;
					}

					bool checked = *visible != 0;
					ImGui::PushID((int)categoryIndex);
					bool activated = ImGui::Checkbox("", &checked);
					if(activated) {
						if(!*selected) {
							UITags_Category_ClearSelection(view);
							*selected = true;
							view->lastCategoryClickIndex = ~0U;
						}
						view->visibleLogsDirty = true;
						for(u32 viewCategoryIndex = 0; viewCategoryIndex < view->categories.count; ++viewCategoryIndex) {
							view_category_t *vc = view->categories.data + viewCategoryIndex;
							if(vc->selected) {
								vc->visible = checked;
							}
						}
						for(u32 configCategoryIndex = 0; configCategoryIndex < view->config.configCategories.count; ++configCategoryIndex) {
							view_config_category_t *vc = view->config.configCategories.data + configCategoryIndex;
							if(vc->selected) {
								vc->visible = checked;
							}
						}
					}
					ImGui::SameLine();
					bb_log_level_e logLevel = kBBLogLevel_VeryVerbose;
					if(viewCategory) {
						u32 viewCategoryIndex = (u32)(viewCategory - view->categories.data);
						recorded_category_t *recordedCategory = view->session->categories.data + viewCategoryIndex;
						logLevel = GetLogLevelBasedOnCounts(recordedCategory->logCount);
					}
					LogLevelColorizer colorizer(logLevel);
					ImVec2 pos = ImGui::GetIconPosForText();
					ImGui::Selectable(categoryName, selected);
					b32 disabled = viewCategory ? viewCategory->disabled : configCategory ? configCategory->disabled : false;
					if(disabled) {
						ImVec4 color4 = GetTextColorForLogLevel(logLevel);
						color4.w *= 0.8f;
						ImColor color = ImGui::GetColorU32(color4);
						ImGui::DrawStrikethrough(categoryName, color, pos);
					}
					ImGui::PopID();
				}
				ImGui::TreePop();
			}

			ImGui::EndGroup();
		}
		ImGui::PopID();
	}

	if(ImGui::CollapsingHeader("Categories", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::PushID("CategoriesHeader");
		view_categories_t *viewCategories = &view->categories;
		if(view->categories.count) {
			view_collect_categories_by_selection(view, &s_matching, &s_unmatching);

			u32 checkedCount = 0;
			u32 uncheckedCount = 0;
			for(u32 index = 0; index < viewCategories->count; ++index) {
				view_category_t *category = viewCategories->data + index;
				if(category->visible) {
					++checkedCount;
				} else {
					++uncheckedCount;
				}
			}
			ImGui::PushID(-1);
			bool allChecked = uncheckedCount == 0;
			if(checkedCount && uncheckedCount) {
				ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, true);
			}
			if(ImGui::Checkbox("", &allChecked)) {
				view_set_all_category_visibility(view, allChecked);
			}
			if(checkedCount && uncheckedCount) {
				ImGui::PopItemFlag();
			}
			ImGui::SameLine();
			ImGui::TextUnformatted("All Categories");
			ImGui::PopID();
		}
		for(u32 viewCategoryIndex = 0; viewCategoryIndex < view->session->categories.count; ++viewCategoryIndex) {
			recorded_categories_t *recordedCategories = &view->session->categories;

			recorded_category_t *recordedCategory = recordedCategories->data + viewCategoryIndex;
			view_category_t *viewCategory = viewCategories->data + viewCategoryIndex;

			if(recordedCategory->id == 0) {
				if(!g_config.showEmptyCategories) {
					continue;
				}
			}

			bool checked = viewCategory->visible != 0;
			ImGui::PushID((int)viewCategoryIndex);
			bool activated = ImGui::Checkbox("", &checked);
			if(activated) {
				if(!viewCategory->selected) {
					UITags_Category_ClearSelection(view);
					UITags_Category_AddSelection(view, viewCategoryIndex);
				}
				UITags_Category_SetSelectedVisibility(view, checked);
			}
			bool bHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_None);
			ImGui::SameLine();
			{
				LogLevelColorizer colorizer(GetLogLevelBasedOnCounts(recordedCategory->logCount));
				ImVec2 pos = ImGui::GetIconPosForText();
				activated = ImGui::Selectable(viewCategory->categoryName, viewCategory->selected != 0);
				if(viewCategory->disabled) {
					ImVec4 color4 = GetTextColorForLogLevel(GetLogLevelBasedOnCounts(recordedCategory->logCount));
					color4.w *= 0.8f;
					ImColor color = ImGui::GetColorU32(color4);
					ImGui::DrawStrikethrough(viewCategory->categoryName, color, pos);
				}
				if(activated) {
					UITags_Category_HandleClick(view, viewCategoryIndex);
				}
			}
			bHovered = bHovered || ImGui::IsItemHovered(ImGuiHoveredFlags_None);
			ImGui::PopID();
			if(ImGui::IsWindowFocused() && bHovered) {
				if(ImGui::IsKeyPressed('A') && ImGui::GetIO().KeyCtrl) {
					for(u32 c = 0; c < view->categories.count; ++c) {
						view->categories.data[c].selected = true;
					}
					view->lastCategoryClickIndex = ~0U;
				} else if(ImGui::IsKeyPressed(ImGuiKey_Escape)) {
					for(u32 c = 0; c < view->categories.count; ++c) {
						view->categories.data[c].selected = false;
					}
					view->lastCategoryClickIndex = ~0U;
				}
			}
			UITags_CategoryToolTip(recordedCategory);
			UITags_CategoryPopup(view, viewCategoryIndex);
		}
		ImGui::PopID();
	}

	ImGui::PopStyleVar();
}
