// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#include "mc_imgui_example.h"
#include "bb_file.h"
#include "common.h"
#include "crt_leak_check.h"
#include "file_utils.h"
#include "fonts.h"
#include "imgui_core.h"
#include "imgui_core_windows.h"
#include "imgui_image.h"
#include "imgui_input_text.h"
#include "message_box.h"
#include "output.h"
#include "str.h"
#include "system_error_utils.h"
#include "tokenize.h"
#include "ui_loglevel_colorizer.h"
#include "va.h"

#include "bb_wrap_stdio.h"

static bool s_showImguiDemo;
static bool s_showImguiAbout;
static bool s_showImguiMetrics;
static bool s_showImguiUserGuide;
static bool s_showImguiStyleEditor;
static UserImageId s_imageId;

static const char* s_colorschemes[] = {
	"ImGui Dark",
	"Light",
	"Classic",
	"Visual Studio Dark",
	"Windows",
};

static void MC_Imgui_Example_BBClient_MessageBox_Success(const char* message)
{
	messageBox mb = {};
	sdict_add_raw(&mb.data, "title", "File Read Success");
	sdict_add_raw(&mb.data, "text", message);
	sdict_add_raw(&mb.data, "button1", "Ok");
	mb_queue(mb, nullptr);
}

static void MC_Imgui_Example_BBClient_MessageBox_Fail(const char* message)
{
	sb_t systemError = system_error_to_sb(GetLastError());

	messageBox mb = {};
	sdict_add_raw(&mb.data, "title", "File Read Error");
	sdict_add_raw(&mb.data, "text", va("%s\n\n%s", message, sb_get(&systemError)));
	sdict_add_raw(&mb.data, "button1", "Ok");
	mb_queue(mb, nullptr);

	sb_reset(&systemError);
}

static void MC_Imgui_Example_Read_BBClient_BBox_FileData(void)
{
	const char* path = R"(..\..\bbclient\vs\bbclient.bbox)";
	fileData_t fileData = fileData_read(path);
	if (fileData.bufferSize)
	{
		MC_Imgui_Example_BBClient_MessageBox_Success(va("Read %u bytes from %s.", fileData.bufferSize, path));
		fileData_reset(&fileData);
	}
	else
	{
		MC_Imgui_Example_BBClient_MessageBox_Fail(va("Failed to read %s.", path));
	}
}

static void MC_Imgui_Example_Read_BBClient_BBox_BBFile(void)
{
	const char* path = R"(..\..\bbclient\vs\bbclient.bbox)";

	if (bb_file_readable(path))
	{
		bb_file_handle_t handle = bb_file_open_for_read(path);
		if (handle == BB_INVALID_FILE_HANDLE)
		{
			MC_Imgui_Example_BBClient_MessageBox_Fail(va("Failed to open %s for read.", path));
		}
		else
		{
			u32 fileSize = bb_file_size(handle);
			char* buffer = (char*)malloc(fileSize);
			if (buffer)
			{
				u32 readSize = bb_file_read(handle, buffer, fileSize);
				free(buffer);

				if (readSize == fileSize)
				{
					MC_Imgui_Example_BBClient_MessageBox_Success(va("Read %u bytes from %s.", readSize, path));
				}
				else
				{
					MC_Imgui_Example_BBClient_MessageBox_Fail(va("Read %u/%d bytes from %s.", readSize, fileSize, path));
				}
			}
			else
			{
				MC_Imgui_Example_BBClient_MessageBox_Fail(va("Failed to allocate %u bytes to read %s.", fileSize, path));
			}

			bb_file_close(handle);
		}
	}
	else
	{
		MC_Imgui_Example_BBClient_MessageBox_Fail(va("%s is not readable.", path));
	}
}

static void MC_Imgui_Example_Read_BBClient_BBox_Stdio(void)
{
	const char* path = R"(..\..\bbclient\vs\bbclient.bbox)";

	FILE *handle = fopen(path, "rb");
	if (handle)
	{
		fseek(handle, 0, SEEK_END);
		u32 fileSize = (u32)ftell(handle);
		fseek(handle, 0, SEEK_SET);
		char* buffer = (char*)malloc(fileSize);
		if (buffer)
		{
			u32 readSize = (u32)fread(buffer, 1, fileSize, handle);
			free(buffer);

			if (readSize == fileSize)
			{
				MC_Imgui_Example_BBClient_MessageBox_Success(va("Read %u bytes from %s.", readSize, path));
			}
			else
			{
				MC_Imgui_Example_BBClient_MessageBox_Fail(va("Read %u/%d bytes from %s.", readSize, fileSize, path));
			}
		}
		else
		{
			MC_Imgui_Example_BBClient_MessageBox_Fail(va("Failed to allocate %u bytes to read %s.", fileSize, path));
		}

		fclose(handle);
	}
	else
	{
		MC_Imgui_Example_BBClient_MessageBox_Fail(va("Failed to open %s for read.", path));
	}
}

static void MC_Imgui_Example_MainMenuBar(void)
{
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Read bbclient bbox with fileData"))
			{
				MC_Imgui_Example_Read_BBClient_BBox_FileData();
			}
			if (ImGui::MenuItem("Read bbclient bbox with bbfile"))
			{
				MC_Imgui_Example_Read_BBClient_BBox_BBFile();
			}
			if (ImGui::MenuItem("Read bbclient bbox with stdio"))
			{
				MC_Imgui_Example_Read_BBClient_BBox_Stdio();
			}
			if (ImGui::MenuItem("Exit"))
			{
				Imgui_Core_RequestShutDown();
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Edit"))
		{
			if (ImGui::MenuItem("Config"))
			{
				BB_LOG("UI::Menu::Config", "UIConfig_Open");
				messageBox mb = {};
				sdict_add_raw(&mb.data, "title", "Config error");
				sdict_add_raw(&mb.data, "text", "Failed to open config UI.\nThis is an example that doesn't have config save/load.");
				sdict_add_raw(&mb.data, "button1", "Ok");
				mb_queue(mb, nullptr);
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Debug"))
		{
			if (ImGui::BeginMenu("Color schemes"))
			{
				const char* colorscheme = Imgui_Core_GetColorScheme();
				for (size_t i = 0; i < BB_ARRAYSIZE(s_colorschemes); ++i)
				{
					bool bSelected = !strcmp(colorscheme, s_colorschemes[i]);
					if (ImGui::MenuItem(s_colorschemes[i], nullptr, &bSelected))
					{
						Imgui_Core_SetColorScheme(s_colorschemes[i]);
						Style_ReadConfig(s_colorschemes[i]);
					}
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("DEBUG Scale"))
			{
				float dpiScale = Imgui_Core_GetDpiScale();
				if (ImGui::MenuItem("1", nullptr, dpiScale == 1.0f))
				{
					Imgui_Core_SetDpiScale(1.0f);
				}
				if (ImGui::MenuItem("1.25", nullptr, dpiScale == 1.25f))
				{
					Imgui_Core_SetDpiScale(1.25f);
				}
				if (ImGui::MenuItem("1.5", nullptr, dpiScale == 1.5f))
				{
					Imgui_Core_SetDpiScale(1.5f);
				}
				if (ImGui::MenuItem("1.75", nullptr, dpiScale == 1.75f))
				{
					Imgui_Core_SetDpiScale(1.75f);
				}
				if (ImGui::MenuItem("2", nullptr, dpiScale == 2.0f))
				{
					Imgui_Core_SetDpiScale(2.0f);
				}
				ImGui::EndMenu();
			}
			Fonts_Menu();
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Imgui Help"))
		{
			ImGui::MenuItem("Demo", nullptr, &s_showImguiDemo);
			ImGui::MenuItem("About", nullptr, &s_showImguiAbout);
			ImGui::MenuItem("Metrics", nullptr, &s_showImguiMetrics);
			ImGui::MenuItem("User Guide", nullptr, &s_showImguiUserGuide);
			ImGui::MenuItem("Style Editor", nullptr, &s_showImguiStyleEditor);
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}
}

static void MC_Imgui_Example_Image(void)
{
	if (s_imageId.id)
	{
		UserImageData image = ImGui_Image_Get(s_imageId);
		if (image.texture)
		{
			ImVec2 windowSize = ImGui::GetContentRegionAvail();
			if (windowSize.x >= (float)image.width)
			{
				windowSize.x = (float)image.width;
			}
			if (windowSize.y >= (float)image.height)
			{
				windowSize.y = (float)image.height;
			}
			ImVec2 constrainedSize = ImGui_Image_Constrain(image, windowSize);
			ImVec2 start = ImGui::GetWindowPos();
			start.x += 20.0f;
			start.y += 20.0f;
			ImVec2 end(start.x + constrainedSize.x, start.y + constrainedSize.y);
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			drawList->AddImage(image.texture, start, end, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), 0xFFFFFFFF);
		}
	}
}

static bool s_MCCommonTests = true;
static bool s_InputTest = false;
static bool s_DebugWindowTest = false;
static char s_inputBufferMulti[8192];
static char s_inputBufferSingle[8192];
void MC_Imgui_Example_Update(void)
{
	MC_Imgui_Example_MainMenuBar();
	MC_Imgui_Example_Image();

	if (ImGui::Begin("Tests", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Checkbox("mc_common tests", &s_MCCommonTests);
		ImGui::Checkbox("input test", &s_InputTest);
		ImGui::Checkbox("debug window test", &s_DebugWindowTest);
		ImGui::End();
	}

	if (s_MCCommonTests && ImGui::Begin("mc_common tests", &s_MCCommonTests, ImGuiWindowFlags_AlwaysAutoResize))
	{
#if defined(MC_COMMON_TESTS) && MC_COMMON_TESTS
		ImGui::TextUnformatted("mc_common tests");
		if (ImGui::Button("run") || true)
		{
			output_reset_buffer();
			if (test_tokenize())
			{
				output_log("[tokenize] pass");
			}
			else
			{
				output_error("[tokenize] FAIL");
			}
		}
		if (ImGui::Button("clear"))
		{
			output_reset_buffer();
		}

		for (u32 i = 0; i < g_output.count; ++i)
		{
			output_line_t *line = g_output.data + i;
			bb_log_level_e logLevel = kBBLogLevel_Verbose;
			switch (line->level)
			{
				case kOutput_Log:
					logLevel = kBBLogLevel_Log;
					break;
				case kOutput_Warning:
					logLevel = kBBLogLevel_Warning;
					break;
				case kOutput_Error:
					logLevel = kBBLogLevel_Error;
					break;
				default:
					break;
			}
			LogLevelColorizer colorizer(logLevel);
			ImGui::TextUnformatted(sb_get(&line->text));
		}
#else
		ImGui::TextUnformatted("No mc_common tests available");
#endif
		ImGui::End();
	}

	if (s_InputTest && ImGui::Begin("InputTest", &s_InputTest))
	{
		if (ImGui::Button("Copy Long Line 1"))
		{
			ImGui::SetClipboardText("This is a reasonably long line, which one might expect to wrap in a tooltip, even though it does not in the log view.  It *should* wrap at 600px in the tooltip.  At least, it did when this was written.  That might have changed by now, but the point remains - this is a long log line.");
		}
		ImGui::SameLine();
		if (ImGui::Button("Copy Long Line 2"))
		{
			ImGui::SetClipboardText("Really long log line to test exceeding normal blackbox packet length.  ^1Really long log line to test exceeding normal blackbox packet length.  ^2Really long log line to test exceeding normal blackbox packet length.  ^3Really long log line to test exceeding normal blackbox packet length.  ^4Really long log line to test exceeding normal blackbox packet length.  ^5Really long log line to test exceeding normal blackbox packet length.  ^6Really long log line to test exceeding normal blackbox packet length.  ^7Really long log line to test exceeding normal blackbox packet length.  ^8Really long log line to test exceeding normal blackbox packet length.  ^9Really long log line to test exceeding normal blackbox packet length.  ^:Really long log line to test exceeding normal blackbox packet length.  ^<Really long log line to test exceeding normal blackbox packet length.  ^0Really long log line to test exceeding normal blackbox packet length.  ^1Really long log line to test exceeding normal blackbox packet length.  ^2Really long log line to test exceeding normal blackbox packet length.  ^3Really long log line to test exceeding normal blackbox packet length.  ^4Really long log line to test exceeding normal blackbox packet length.  ^5Really long log line to test exceeding normal blackbox packet length.  ^6Really long log line to test exceeding normal blackbox packet length.  ^7Really long log line to test exceeding normal blackbox packet length.  ^8Really long log line to test exceeding normal blackbox packet length.  ^9Really long log line to test exceeding normal blackbox packet length.  ^:Really long log line to test exceeding normal blackbox packet length.  ^<Really long log line to test exceeding normal blackbox packet length.^F  ^0Really long log line to test exceeding normal blackbox packet length.  ^1Really long log line to test exceeding normal blackbox packet length.  ^2Really long log line to test exceeding normal blackbox packet length.  --------------------------------------------------> ^<(2k packet limit here)^3Really long log line to test exceeding normal blackbox packet length.  ^4Really long log line to test exceeding normal blackbox packet length.  ^5Really long log line to test exceeding normal blackbox packet length.  ^6Really long log line to test exceeding normal blackbox packet length.  ^7Really long log line to test exceeding normal blackbox packet length.  ^8Really long log line to test exceeding normal blackbox packet length.  ^9Really long log line to test exceeding normal blackbox packet length.  ^:Really long log line to test exceeding normal blackbox packet length.  ^<Really long log line to test exceeding normal blackbox packet length. ^F  ^0Really long log line to test exceeding normal blackbox packet length.  ^1Really long log line to test exceeding normal blackbox packet length.  ^2Really long log line to test exceeding normal blackbox packet length.  ^3Really long log line to test exceeding normal blackbox packet length.  ^4Really long log line to test exceeding normal blackbox packet length.  ^5Really long log line to test exceeding normal blackbox packet length.  ^6Really long log line to test exceeding normal blackbox packet length.  ^7Really long log line to test exceeding normal blackbox packet length.  ^8Really long log line to test exceeding normal blackbox packet length.  ^9Really long log line to test exceeding normal blackbox packet length.  ^:Really long log line to test exceeding normal blackbox packet length.  ^<Really long log line to test exceeding normal blackbox packet length.  End at 3596.");
		}
		ImGui::SameLine();
		if (ImGui::Button("Copy Tall Lines"))
		{
			ImGui::SetClipboardText("1\n2\n3\n4\n5\n6\n7\n8\n9\n0\n-------------\n");
		}

		ImGui::PushItemWidth(1000.0f);
		ImGui::InputTextScrolling("##InputTestSingle", s_inputBufferSingle, sizeof(s_inputBufferSingle), ImGuiInputTextFlags_None);
		ImGui::PopItemWidth();

		ImVec2 inputSize(1000.0f, -40.0f);
		ImGui::InputTextMultilineScrolling("##InputTestMulti", s_inputBufferMulti, sizeof(s_inputBufferMulti), inputSize, ImGuiInputTextFlags_None);
		ImGui::End();
	}

	if (s_showImguiDemo)
	{
		ImGui::ShowDemoWindow(&s_showImguiDemo);
	}
	if (s_showImguiAbout)
	{
		ImGui::ShowAboutWindow(&s_showImguiAbout);
	}
	if (s_showImguiMetrics)
	{
		ImGui::ShowMetricsWindow(&s_showImguiMetrics);
	}
	if (s_showImguiUserGuide)
	{
		ImGui::ShowUserGuide();
	}
	if (s_showImguiStyleEditor)
	{
		ImGui::ShowStyleEditor();
	}

	if (s_DebugWindowTest)
	{
		const char* testCommandLine = R"("-test \"_\\\" !" test2)";
		ImGui::TextUnformatted(testCommandLine);
		const char* cursor = testCommandLine;
		span_t token = tokenize(&cursor, " ");
		while (token.start)
		{
			ImGui::TextUnformatted(token.start, token.end);
			//BB_LOG("Tokens", "%.*s", token.end - token.start, token.start);

			char* tmp = va("%.*s", token.end - token.start, token.start);
			strunescape(tmp);
			ImGui::TextUnformatted(tmp);

			token = tokenize(&cursor, " ");
		}
	}
}

int CALLBACK WinMain(_In_ HINSTANCE /*Instance*/, _In_opt_ HINSTANCE /*PrevInstance*/, _In_ LPSTR CommandLine, _In_ int /*ShowCode*/)
{
	crt_leak_check_init();

	BB_INIT_WITH_FLAGS("mc_imgui_example", kBBInitFlag_None);
	BB_THREAD_SET_NAME("main");
	BB_LOG("Startup", "Arguments: %s", CommandLine);

	output_init(kOutputInit_ToBuffer);
	Imgui_Core_Init(CommandLine);

	mb_get_queue()->modal = true;

	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	//ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

	WINDOWPLACEMENT wp = { BB_EMPTY_INITIALIZER };
	if (Imgui_Core_InitWindow("mc_imgui_example_wndclass", "mc_imgui_example", LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_MAINICON)), wp))
	{
		s_imageId = ImGui_Image_CreateFromFile(R"(..\examples\mc_imgui_example.png)");

		Style_ReadConfig("ImGui Dark");

		while (!Imgui_Core_IsShuttingDown())
		{
			if (Imgui_Core_GetAndClearDirtyWindowPlacement())
			{
				// config_getwindowplacement();
			}
			if (Imgui_Core_BeginFrame())
			{
				MC_Imgui_Example_Update();

				ImVec4 clear_col = ImColor(34, 35, 34);
				Imgui_Core_EndFrame(clear_col);
			}
		}
		ImGui_Image_MarkForDestroy(s_imageId);
		Imgui_Core_ShutdownWindow();
	}

	Imgui_Core_Shutdown();
	output_shutdown();

	BB_SHUTDOWN();

	return 0;
}
