// Copyright (c) 2012-2021 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "common.h"
#include "wrap_imgui.h"

typedef struct sb_s sb_t;
typedef struct tag_tooltipConfig tooltipConfig;

namespace ImGui
{
	const ImGuiTreeNodeFlags DefaultOpenTreeNodeFlags = ImGuiTreeNodeFlags_OpenOnArrow |
	                                                    ImGuiTreeNodeFlags_OpenOnDoubleClick |
	                                                    ImGuiTreeNodeFlags_DefaultOpen;

	const ImGuiTreeNodeFlags DefaultClosedTreeNodeFlags = ImGuiTreeNodeFlags_OpenOnArrow |
	                                                      ImGuiTreeNodeFlags_OpenOnDoubleClick;

	void PushStyleColor(ImGuiCol idx, const ImColor &col);
	bool Checkbox(const char *label, b8 *v);
	bool Checkbox(const char *label, b32 *v);
	bool Selectable(const char *label, b32 *p_selected, ImGuiSelectableFlags flags = 0, const ImVec2 &size = ImVec2(0, 0));
	bool TreeNodeEx(const char *label, ImGuiTreeNodeFlags flags, b32 *v, void *ptr_id = nullptr);
	bool MenuItem(const char *label, const char *shortcut, b32 *p_selected, bool enabled = true);
	bool Begin(const char *name, b32 *p_open, ImGuiWindowFlags flags = 0);
	bool Begin(const char *name, b8 *p_open, ImGuiWindowFlags flags = 0);
	bool IsKeyPressed(ImGuiKey_ key, bool repeat = true);
	bool IsCurrentWindowNavWindowRoot(void);
	bool IsCurrentWindowNavWindow(void);
	bool IsCurrentWindowMoving(void);
	bool IsAnyWindowMoving(void);
	bool InputText(const char *label, sb_t *buf, u32 buf_size, ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = NULL, void *user_data = NULL);
	bool InputTextMultiline(const char *label, sb_t *buf, u32 buf_size, const ImVec2 &size = ImVec2(0, 0), ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = NULL, void *user_data = NULL);

	enum verticalScrollDir_e {
		kVerticalScroll_None,
		kVerticalScroll_PageUp,
		kVerticalScroll_PageDown,
		kVerticalScroll_Up,
		kVerticalScroll_Down,
		kVerticalScroll_Start,
		kVerticalScroll_End,
	};
	verticalScrollDir_e GetVerticalScrollDir();

	void SelectableTextUnformattedMultiline(const char *label, const char *text, ImVec2 size = ImVec2(0.0f, 0.0f));
	void SelectableTextUnformatted(const char *label, const char *text);
	void SelectableText(const char *label, const char *fmt, ...);
	bool SelectableWithBackground(const char *label, bool selected, const ImColor bgColor, ImGuiSelectableFlags flags = 0, const ImVec2 &size_arg = ImVec2(0.0f, 0.0f));

	void SetTextShadowColor(ImColor shadowColor);
	void TextShadow(const char *text, bool bWrapped = false, bool bHideLabel = false);
	void TextShadowed(const char *text);
	void TextWrappedShadowed(const char *text);
	void DrawStrikethrough(const char *text, ImColor color, ImVec2 pos);

	enum buttonType_e {
		kButton_Normal,
		kButton_Disabled,
		kButton_TabActive,
		kButton_TabInactive,
		kButton_ColumnHeader,
		kButton_ColumnHeaderNoSort,
		kButton_ResizeBar,
	};
	void PushButtonColors(buttonType_e colors);
	void PopButtonColors(void);
	bool Button(const char *label, buttonType_e colors, const ImVec2 &size = ImVec2(0.0f, 0.0f));

	void BeginTabButtons(void);
	bool TabButton(const char *label, sb_t *active, const char *name, const ImVec2 &size = ImVec2(0.0f, 0.0f));
	bool TabButton(const char *label, u32 *active, u32 id, const ImVec2 &size = ImVec2(0.0f, 0.0f));
	bool TabButtonIconColored(const char *icon, ImColor iconColor, const char *label, u32 *active, u32 id, const ImVec2 &size = ImVec2(0.0f, 0.0f));
	void EndTabButtons(void);

	bool BeginTabChild(sb_t *active, const char *str_id, const ImVec2 &size = ImVec2(0, 0), bool border = true, ImGuiWindowFlags extra_flags = 0);
	bool BeginTabChild(u32 *active, u32 id, const char *str_id, const ImVec2 &size = ImVec2(0, 0), bool border = true, ImGuiWindowFlags extra_flags = 0);
	void EndTabChild();

	struct columnDrawResult {
		b32 sortChanged;
		b32 active;
	};

	struct columnDrawData {
		float *columnWidths;
		float *columnOffsets;
		const char **columnNames;
		b32 *sortDescending;
		u32 *sortColumn;
		u32 numColumns;
		u8 pad[4];
	};
	void PushColumnHeaderClipRect(float offset, float width);
	void DrawColumnText(columnDrawData h, u32 columnIndex, const char *text, const char *end = nullptr);
	void DrawColumnHeaderText(float offset, float width, const char *text, const char *end = nullptr, const char *contextMenuName = nullptr);
	columnDrawResult DrawColumnHeader(columnDrawData h, u32 columnIndex, const char *contextMenuName = nullptr);

	void PushSelectableColors(b32 selected, b32 viewActive);
	void PopSelectableColors(b32 selected, b32 viewActive);

	bool BeginContextMenu(const char *name);
	void EndContextMenu();

	void IconOverlay(const char *icon, const char *overlay, ImColor overlayColor);
	void IconOverlayColored(ImColor iconColor, const char *icon, ImColor overlayColor, const char *overlay);

	void Icon(const char *icon);
	void IconColored(ImColor color, const char *icon);

	void DrawIconAtPos(ImVec2 pos, const char *icon, ImColor color, bool align = false, float scale = 1.0f);
	ImVec2 GetIconPosForButton();
	ImVec2 GetIconPosForText();

	void SetActiveSelectables(const void *data);
	bool IsActiveSelectables(const void *data);

	ImGuiViewport *GetViewportForWindow(const char *windowName);

} // namespace ImGui
