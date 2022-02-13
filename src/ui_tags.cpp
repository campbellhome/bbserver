// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#include "ui_tags.h"
#include "bb_array.h"
#include "bb_colors.h"
#include "imgui_text_shadows.h"
#include "imgui_tooltips.h"
#include "imgui_utils.h"
#include "recorded_session.h"
#include "tags.h"
#include "ui_loglevel_colorizer.h"
#include "ui_view.h"
#include "va.h"
#include "view.h"
#include "view_config.h"
#include "wrap_imgui_internal.h"

static sb_t s_newTagName;
static sb_t s_categoryTagNames;
static view_category_collection_t s_matching;
static view_category_collection_t s_unmatching;
static view_category_collection_t s_matchingAll;
static view_category_collection_t s_unmatchingAll;

void UITags_Shutdown()
{
	sb_reset(&s_newTagName);
	sb_reset(&s_categoryTagNames);
	bba_free(s_matching.viewCategories);
	bba_free(s_unmatching.viewCategories);
	bba_free(s_matchingAll.viewCategories);
	bba_free(s_unmatchingAll.viewCategories);
}

static void UITags_CategoryToolTip(recorded_category_t *category)
{
	if(ImGui::IsTooltipActive()) {
		ImGui::BeginTooltip();
		ImGui::PushStyleColor(ImGuiCol_Text, MakeColor(kStyleColor_kBBColor_Default));
		ImGui::TextUnformatted(category->categoryName);
		tagCategory_t *tagCategory = tagCategory_find(category->categoryName);
		if(tagCategory && tagCategory->tags.count) {
			sb_clear(&s_categoryTagNames);
			for(u32 i = 0; i < tagCategory->tags.count; ++i) {
				const char *tagName = sb_get(tagCategory->tags.data + i);
				if(strchr(tagName, ' ')) {
					sb_va(&s_categoryTagNames, " \"%s\"", tagName);
				} else {
					sb_va(&s_categoryTagNames, " %s", tagName);
				}
			}
			ImGui::Text("Tags: %s", sb_get(&s_categoryTagNames));
		}
		ImGui::Separator();
		ImGui::PopStyleColor(1);
		UIRecordedView_TooltipLevelText("VeryVerbose: %u", category->logCount[kBBLogLevel_VeryVerbose], kBBLogLevel_VeryVerbose);
		UIRecordedView_TooltipLevelText("Verbose: %u", category->logCount[kBBLogLevel_Verbose], kBBLogLevel_Verbose);
		UIRecordedView_TooltipLevelText("Logs: %u", category->logCount[kBBLogLevel_Log], kBBLogLevel_Log);
		UIRecordedView_TooltipLevelText("Display: %u", category->logCount[kBBLogLevel_Display], kBBLogLevel_Display);
		UIRecordedView_TooltipLevelText("Warnings: %u", category->logCount[kBBLogLevel_Warning], kBBLogLevel_Warning);
		UIRecordedView_TooltipLevelText("Errors: %u", category->logCount[kBBLogLevel_Error], kBBLogLevel_Error);
		UIRecordedView_TooltipLevelText("Fatal: %u", category->logCount[kBBLogLevel_Fatal], kBBLogLevel_Fatal);
		ImGui::EndTooltip();
	}
}

static void UITags_TagToolTip(tag_t *tag)
{
	if(ImGui::IsTooltipActive()) {
		ImGui::BeginTooltip();
		ImGui::PushStyleColor(ImGuiCol_Text, MakeColor(kStyleColor_kBBColor_Default));
		ImGui::TextUnformatted(sb_get(&tag->name));
		if(tag->categories.count) {
			ImGui::Text("Categories:");
			for(u32 i = 0; i < tag->categories.count; ++i) {
				ImGui::Text("  %s", sb_get(tag->categories.data + i));
			}
		}
		ImGui::PopStyleColor(1);
		ImGui::EndTooltip();
	}
}

static void UITags_TagPopup(tag_t *tag, view_t *view)
{
	const char *tagName = sb_get(&tag->name);
	if(ImGui::BeginPopupContextItem(va("TagPopup%s", tagName))) {
		if(ImGui::BeginMenu("Remove tag...")) {
			const char *menuText = tag->categories.count ? "Remove non-empty tag" : "Remove empty tag";
			if(ImGui::MenuItem(menuText)) {
				tag_remove(tag);
				tags_write();
			}
			ImGui::EndMenu();
		}

		if(tag->visibility == kTagVisibility_Normal && ImGui::MenuItem("Show tag")) {
			view_set_category_collection_visiblity(&s_matchingAll, true);
		}
		if(tag->visibility != kTagVisibility_AlwaysHidden && ImGui::MenuItem("Show only tag")) {
			view_set_category_collection_visiblity(&s_matchingAll, true);
			view_set_category_collection_visiblity(&s_unmatchingAll, false);
		}
		if(tag->visibility == kTagVisibility_Normal && ImGui::MenuItem("Hide tag")) {
			view_set_category_collection_visiblity(&s_matchingAll, false);
		}

		if(ImGui::BeginMenu("Visibility")) {
			const tag_visibility_t oldVisibility = tag->visibility;
			if(ImGui::RadioButton("Normal", tag->visibility == kTagVisibility_Normal)) {
				tag->visibility = kTagVisibility_Normal;
			}
			if(ImGui::RadioButton("Always visible", tag->visibility == kTagVisibility_AlwaysVisible)) {
				tag->visibility = kTagVisibility_AlwaysVisible;
			}
			if(ImGui::RadioButton("Always hidden", tag->visibility == kTagVisibility_AlwaysHidden)) {
				tag->visibility = kTagVisibility_AlwaysHidden;
			}
			if(oldVisibility != tag->visibility) {
				tags_write();
				tag_apply_tag_visibility_to_all_views();
			}
			ImGui::EndMenu();
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

		if(!allEnabled) {
			if(ImGui::MenuItem("Enable tag categories")) {
				view_set_category_collection_disabled(&s_matching, false);
			}
		}
		if(!allDisabled) {
			if(ImGui::MenuItem("Disable tag categories")) {
				view_set_category_collection_disabled(&s_matching, true);
			}
		}
		if(ImGui::MenuItem("Replace with selected categories")) {
			while(tag->categories.count) {
				tag_remove_category(tagName, sb_get(tag->categories.data));
			}
			for(u32 viewCategoryIndex = 0; viewCategoryIndex < view->categories.count; ++viewCategoryIndex) {
				view_category_t *vc = view->categories.data + viewCategoryIndex;
				if(vc->selected && !view_category_treat_as_empty(vc)) {
					tag_add_category(tagName, vc->categoryName);
				}
			}
			tags_write();
		}
		if(ImGui::MenuItem("Replace with visible categories")) {
			while(tag->categories.count) {
				tag_remove_category(tagName, sb_get(tag->categories.data));
			}
			for(u32 viewCategoryIndex = 0; viewCategoryIndex < view->categories.count; ++viewCategoryIndex) {
				view_category_t *vc = view->categories.data + viewCategoryIndex;
				if(vc->visible && !view_category_treat_as_empty(vc)) {
					tag_add_category(tagName, vc->categoryName);
				}
			}
			tags_write();
		}
		ImGui::EndPopup();
	}
}

void UITags_Category_ClearSelection(view_t *view)
{
	for(u32 i = 0; i < view->categories.count; ++i) {
		view_category_t *viewCategory = view->categories.data + i;
		viewCategory->selected = false;
	}
	view->lastCategoryClickIndex = ~0U;
}

void UITags_Category_SetSelected(view_t *view, u32 viewCategoryIndex, b32 selected)
{
	view_category_t *viewCategory = view->categories.data + viewCategoryIndex;
	viewCategory->selected = selected;
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
				view_category_t *viewCategory = view->categories.data + i;
				if(!view_category_treat_as_empty(viewCategory)) {
					viewCategory->selected = true;
				}
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
	tag_apply_tag_visibility_to_all_views();
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

		if(viewCategory->id == 0) {
			if(ImGui::MenuItem("Remove Unreferenced Categories")) {
				view_remove_unreferenced_categories(&s_matching);
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

static void UITags_Category_SetSelectedVisibility(view_t *view, b32 visible, bool exclusive)
{
	view->visibleLogsDirty = true;
	for(u32 viewCategoryIndex = 0; viewCategoryIndex < view->categories.count; ++viewCategoryIndex) {
		view_category_t *viewCategory = view->categories.data + viewCategoryIndex;
		if(viewCategory->selected) {
			viewCategory->visible = visible;
		} else if(exclusive) {
			viewCategory->visible = false;
		}
	}
	view_apply_tag_visibility(view);
}

static void UITags_CountCategoryVisibility(view_t *view, tag_t *tag, u32 *visibleCount, u32 *hiddenCount)
{
	u32 numVisible = 0;
	u32 numHidden = 0;
	for(u32 categoryIndex = 0; categoryIndex < tag->categories.count; ++categoryIndex) {
		sb_t *category = tag->categories.data + categoryIndex;
		const char *categoryName = sb_get(category);
		view_category_t *viewCategory = view_find_category_by_name(view, categoryName);
		if(viewCategory && !view_category_treat_as_empty(viewCategory)) {
			if(viewCategory->visible) {
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

	if(ImGui::CollapsingHeader("Tags", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::PushID("TagsHeader");
		for(u32 tagIndex = 0; tagIndex < g_tags.tags.count; ++tagIndex) {
			tag_t *tag = g_tags.tags.data + tagIndex;
			const char *tagName = sb_get(&tag->name);
			bool bTagSelected = false;

			view_collect_categories_by_tag(view, &s_matching, &s_unmatching, tag, false);
			view_collect_categories_by_tag(view, &s_matchingAll, &s_unmatchingAll, tag, true);

			u32 numVisible = 0;
			u32 numHidden = 0;
			UITags_CountCategoryVisibility(view, tag, &numVisible, &numHidden);

			bool allChecked = numVisible > 0 && numHidden == 0;
			if(numVisible && numHidden) {
				ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, true);
			}
			ImGui::PushID((int)tagIndex);
			if(ImGui::Checkbox("", &allChecked)) {
				if(ImGui::GetIO().KeyCtrl) {
					allChecked = true;
					view_set_all_category_visibility(view, false);
				}
				view_set_category_collection_visiblity(&s_matchingAll, allChecked);
			}
			ImGui::PopID();
			if(numVisible && numHidden) {
				ImGui::PopItemFlag();
			}
			ImGui::SameLine();
			ImGui::BeginGroup();

			if(!numVisible && !numHidden) {
				ImGui::PushStyleColor(ImGuiCol_Text, GetTextColorForLogLevel(kBBLogLevel_Verbose));
			}
			const char *resolvedTagName = tagName;
			if(tag->visibility == kTagVisibility_AlwaysVisible) {
				resolvedTagName = va("%s (visible)##%s", tagName, tagName);
			} else if(tag->visibility == kTagVisibility_AlwaysHidden) {
				resolvedTagName = va("%s (hidden)##%s", tagName, tagName);
			}
			bool open = ImGui::TreeNodeEx(resolvedTagName, tagNodeFlags | (bTagSelected ? ImGuiTreeNodeFlags_Selected : 0));
			if(!numVisible && !numHidden) {
				ImGui::PopStyleColor();
			}

			if(ImGui::IsItemClicked() && ImGui::GetIO().KeyCtrl) {
				view_set_all_category_visibility(view, false);
				view_set_category_collection_visiblity(&s_matchingAll, true);
			}
			UITags_TagToolTip(tag);
			UITags_TagPopup(tag, view);

			if(open) {
				for(u32 categoryIndex = 0; categoryIndex < tag->categories.count; ++categoryIndex) {
					sb_t *category = tag->categories.data + categoryIndex;
					const char *categoryName = sb_get(category);

					view_category_t *viewCategory = view_find_category_by_name(view, categoryName);
					b32 *visible = NULL;
					b32 *selected = NULL;
					if(viewCategory && !view_category_treat_as_empty(viewCategory)) {
						visible = &viewCategory->visible;
						selected = &viewCategory->selected;
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
						view_apply_tag_visibility(view);
					}
					ImGui::SameLine();
					u32 viewCategoryIndex = (u32)(viewCategory - view->categories.data);
					recorded_category_t *recordedCategory = view->session->categories.data + viewCategoryIndex;
					bb_log_level_e logLevel = GetLogLevelBasedOnCounts(recordedCategory->logCount);
					LogLevelColorizer colorizer(logLevel);
					ScopedTextShadows shadows(logLevel);
					ImVec2 pos = ImGui::GetIconPosForText();
					ImGui::TextShadow(categoryName);
					ImGui::Selectable(categoryName, selected);
					if(viewCategory->disabled) {
						ImVec4 color4 = GetTextColorForLogLevel(logLevel);
						color4.w *= 0.8f;
						ImColor color = ImGui::GetColorU32(color4);
						ImGui::DrawStrikethrough(categoryName, color, pos);
					}
					UITags_CategoryToolTip(recordedCategory);
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

			if(ImGui::BeginPopupContextItem("AllCategoriesPopup")) {
				if(g_config.showEmptyCategories) {
					if(ImGui::MenuItem("Hide unused categories")) {
						g_config.showEmptyCategories = false;
					}
				} else {
					if(ImGui::MenuItem("Show unused categories")) {
						g_config.showEmptyCategories = true;
					}
				}
				ImGui::EndPopup();
			}
		}
		for(u32 viewCategoryIndex = 0; viewCategoryIndex < view->session->categories.count; ++viewCategoryIndex) {
			recorded_categories_t *recordedCategories = &view->session->categories;

			recorded_category_t *recordedCategory = recordedCategories->data + viewCategoryIndex;
			view_category_t *viewCategory = viewCategories->data + viewCategoryIndex;
			if(view_category_treat_as_empty(viewCategory)) {
				continue;
			}

			bool checked = viewCategory->visible != 0;
			ImGui::PushID((int)viewCategoryIndex);
			bool activated = ImGui::Checkbox("", &checked);
			if(activated) {
				if(!viewCategory->selected) {
					UITags_Category_ClearSelection(view);
					UITags_Category_AddSelection(view, viewCategoryIndex);
				}
				UITags_Category_SetSelectedVisibility(view, checked || ImGui::GetIO().KeyCtrl, ImGui::GetIO().KeyCtrl);
			}
			bool bHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_None);
			ImGui::SameLine();
			{
				bb_log_level_e logLevel = GetLogLevelBasedOnCounts(recordedCategory->logCount);
				LogLevelColorizer colorizer(logLevel);
				ScopedTextShadows shadows(logLevel);
				ImVec2 pos = ImGui::GetIconPosForText();
				ImGui::TextShadow(viewCategory->categoryName);
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
				if(ImGui::IsKeyPressed(ImGuiKey_A) && ImGui::GetIO().KeyCtrl) {
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
}
