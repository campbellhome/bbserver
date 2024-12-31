// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#include "mc_callstack/callstack_utils.h"
#include "bb_array.h"
#include "bb_criticalsection.h"
#include "bb_defines.h"

#if BB_USING(BB_PLATFORM_WINDOWS)

BB_WARNING_PUSH(4820)
#include "stackwalker/Main/StackWalker/StackWalker.h"
BB_WARNING_POP

class BBStackWalker : public StackWalker
{
public:
	s32 linesToSkip = 0;
	b32 done = false;
	b32 loadedModules = false;

	sb_t* sb = nullptr;
	sbs_t* sbs = nullptr;

	bb_critical_section cs = { BB_EMPTY_INITIALIZER };

	void SetSymbolServer(b32 bSymbolServer)
	{
		if (bSymbolServer)
		{
			m_options |= (StackWalker::StackWalkOptions::SymUseSymSrv | StackWalker::StackWalkOptions::SymBuildPath);
		}
		else
		{
			m_options &= (~StackWalker::StackWalkOptions::SymUseSymSrv | StackWalker::StackWalkOptions::SymBuildPath);
		}
		m_options = StackWalker::StackWalkOptions::RetrieveNone;
	}

protected:
	virtual void OnOutput(LPCSTR /*szText*/) override {}
	virtual void OnCallstackEntry(CallstackEntryType eType, CallstackEntry& entry) override final
	{
		if (linesToSkip)
		{
			--linesToSkip;
		}
		else if (!done)
		{
			if (!strcmp(entry.name, "invoke_main") ||
			    !strcmp(entry.name, "BaseThreadInitThunk") ||
			    !strcmp(entry.name, "RtlUserThreadStart"))
			{
				done = true;
			}
			else
			{
				BB_UNUSED(eType);
				sb_t line = { BB_EMPTY_INITIALIZER };
				sb_va(&line, "0x%llx ", entry.offset);
				const char* moduleNameSep = strrchr(entry.loadedImageName, '\\');
				if (moduleNameSep)
				{
					sb_va(&line, "%s!", moduleNameSep + 1);
				}
				sb_va(&line, "%s() [", entry.undFullName);
				if (entry.lineFileName[0])
				{
					sb_va(&line, "%s:%d", entry.lineFileName, entry.lineNumber);
				}
				sb_append(&line, "]\n");
				if (sb)
				{
					sb_append(sb, sb_get(&line));
				}
				if (sbs)
				{
					bba_push(*sbs, line);
					memset(&line, 0, sizeof(line));
				}
				sb_reset(&line);
			}
		}
	}
};

static BBStackWalker s_stackwalker;

extern "C" void callstack_init(b32 bSymbolServer)
{
	bb_critical_section_init(&s_stackwalker.cs);
	s_stackwalker.SetSymbolServer(bSymbolServer);
}

extern "C" void callstack_shutdown(void)
{
	bb_critical_section_shutdown(&s_stackwalker.cs);
}

extern "C" sb_t callstack_generate_sb(int linesToSkip)
{
	sb_t sb = { BB_EMPTY_INITIALIZER };
	bb_critical_section_lock(&s_stackwalker.cs);
	if (!s_stackwalker.loadedModules)
	{
		s_stackwalker.loadedModules = true;
		s_stackwalker.LoadModules();
	}
	s_stackwalker.linesToSkip = linesToSkip + 2;
	s_stackwalker.done = false;
	s_stackwalker.sb = &sb;
	s_stackwalker.sbs = nullptr;
	s_stackwalker.ShowCallstack();
	bb_critical_section_unlock(&s_stackwalker.cs);
	return sb;
}

extern "C" sbs_t callstack_generate_sbs(int linesToSkip)
{
	sbs_t sbs = { BB_EMPTY_INITIALIZER };
	bb_critical_section_lock(&s_stackwalker.cs);
	if (!s_stackwalker.loadedModules)
	{
		s_stackwalker.loadedModules = true;
		s_stackwalker.LoadModules();
	}
	s_stackwalker.linesToSkip = linesToSkip + 2;
	s_stackwalker.done = false;
	s_stackwalker.sb = nullptr;
	s_stackwalker.sbs = &sbs;
	s_stackwalker.ShowCallstack();
	bb_critical_section_unlock(&s_stackwalker.cs);
	return sbs;
}

extern "C" sb_t callstack_generate_crash_sb(void)
{
	sbs_t lines = callstack_generate_sbs(0);
	sb_t sb = { BB_EMPTY_INITIALIZER };
	b32 sawDispatcher = false;
	for (u32 i = 0; i < lines.count; ++i)
	{
		const char* line = sb_get(lines.data + i);
		if (strstr(line, "KiUserExceptionDispatcher"))
		{
			sawDispatcher = true;
		}
		else if (sawDispatcher)
		{
			sb_append(&sb, line);
		}
	}
	sbs_reset(&lines);
	return sb;
}

#endif // #if BB_USING(BB_PLATFORM_WINDOWS)
