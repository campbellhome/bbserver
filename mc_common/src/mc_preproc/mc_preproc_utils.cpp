// Copyright (c) 2012-2024 Matt Campbell
// MIT license (see License.txt)

#include "mc_preproc.h"

#include "bb.h"

enum
{
	kVA_NumSlots = 16
};
typedef struct
{
	size_t next;
	std::string buffer[kVA_NumSlots];
} va_data_t;
static va_data_t va_data;

std::string& va(std::string& dst, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	size_t chars = vsnprintf(nullptr, 0, fmt, args);
	if (chars)
	{
		char* buffer = new char[chars + 1];
		vsnprintf(buffer, chars + 1, fmt, args);
		buffer[chars] = '\0';
		dst += buffer;
		delete[] buffer;
	}
	va_end(args);
	return dst;
}

const char* va(const char* fmt, ...)
{
	std::string& dst = va_data.buffer[va_data.next++ % kVA_NumSlots];
	dst.clear();

	va_list args;
	va_start(args, fmt);
	size_t chars = vsnprintf(nullptr, 0, fmt, args);
	if (chars)
	{
		char* buffer = new char[chars + 1];
		vsnprintf(buffer, chars + 1, fmt, args);
		buffer[chars] = '\0';
		dst += buffer;
		delete[] buffer;
	}
	va_end(args);
	return dst.c_str();
}

std::string ReplaceChar(const std::string& src, char test, const char* replacement)
{
	std::string dst;
	for (char c : src)
	{
		if (c == test)
		{
			dst += replacement;
		}
		else
		{
			dst.push_back(c);
		}
	}
	return dst;
}

const char* GetPathFilename(const char* src)
{
	const char* sep1 = strrchr(src, '/');
	const char* sep2 = strrchr(src, '\\');
	const char* sep = sep1 > sep2 ? sep1 : sep2;
	if (sep)
	{
		return sep + 1;
	}
	else
	{
		return src;
	}
}

const char* GetPathFilename(const std::string& src)
{
	return GetPathFilename(src.c_str());
}

std::vector<std::string> TokenizePath(const std::string& src)
{
	std::vector<std::string> components;
	std::string component;
	for (char c : src)
	{
		if (c == '/' || c == '\\')
		{
			if (!component.empty())
			{
				components.push_back(std::move(component));
			}
			component.clear();
		}
		else
		{
			component.push_back(c);
		}
	}
	if (!component.empty())
	{
		components.push_back(std::move(component));
	}
	return components;
}

void AddPathComponent(std::string& path, const std::string& component, char separator)
{
	if (path.length() > 0 && path.back() != '/' && path.back() != '\\')
	{
		path.push_back(separator ? separator : '\\');
	}
	path += component;
}

std::string ResolvePath(const std::string& src, char separator)
{
	std::vector<std::string> components = TokenizePath(src);
	std::string dst;

	if (src.length() > 0 && (src[0] == '/' || src[0] == '\\'))
	{
		dst.push_back(separator ? separator : src[0]);
		if (src.length() > 1 && (src[1] == '/' || src[1] == '\\'))
		{
			dst.push_back(separator ? separator : src[1]);
		}
	}

	size_t i = 0;
	while (i < components.size())
	{
		const std::string& cur = components[i];
		if (cur == ".")
		{
			components.erase(components.begin() + i);
			if (i > 0)
			{
				--i;
			}
		}
		else if (i < components.size() - 1 && cur != ".." && components[i + 1] == "..")
		{
			components.erase(components.begin() + i);
			components.erase(components.begin() + i);
			if (i > 0)
			{
				--i;
			}
		}
		else
		{
			++i;
		}
	}

	for (const std::string& component : components)
	{
		AddPathComponent(dst, component, separator);
	}

	return dst;
}

void TestResolvePath()
{
	const char* testPathStr = "../../../../program/some//other\\path/../mc_im/././gui/submodules/../../mc_common/include/./..";
	std::string testPath = ResolvePath(testPathStr, '/');
	bool bSuccess = !strcmp(testPath.c_str(), "../../../../program/some/other/mc_im/mc_common");
	if (!bSuccess)
	{
		BB_ERROR("Path", "Path failed to resolve:\n%s\n%s", testPathStr, testPath.c_str());
	}
}

std::string ReadFileContents(const std::string& path)
{
	std::string contents;
	FILE* fp = fopen(path.c_str(), "rb");
	if (fp)
	{
		long curPos = ftell(fp);
		fseek(fp, 0, SEEK_END);
		long fileSize32 = ftell(fp);
		fseek(fp, curPos, SEEK_SET);

		char* buffer = new char[fileSize32 + 1];
		size_t bytesRead = fread(buffer, 1, fileSize32, fp);
		buffer[bytesRead] = 0;

		contents = buffer;
		delete[] buffer;

		fclose(fp);
	}
	return contents;
}

void WriteAndReportFileData(const std::string& data, const char* srcDir, const char* prefix, const char* suffix)
{
	std::string filename = va("%s%s", prefix, suffix);

	std::string path = srcDir;
	AddPathComponent(path, filename);
	const char* pathName = path.c_str();

	std::string existing = ReadFileContents(path);
	if (existing == data)
	{
		BB_LOG("preproc", "Skipped updating %s", pathName);
	}
	else
	{
		FILE* fp = fopen(pathName, "wb");
		if (fp)
		{
			size_t bytes = data.length();
			size_t written = fwrite(data.c_str(), 1, bytes, fp);
			fclose(fp);
			if (written == bytes)
			{
				BB_LOG("preproc", "updated %s", pathName);
				printf("updated %s\n", pathName);
			}
			else
			{
				BB_ERROR("preproc", "Failed to write %s", pathName);
				printf("Failed to write %s\n", pathName);
			}
		}
		else
		{
			BB_ERROR("preproc", "Failed to open %s for writing", pathName);
			printf("Failed to open %s for writing\n", pathName);
		}
	}
}

void InitialComments(std::string& s)
{
	va(s, "// Copyright (c) 2012-2024 Matt Campbell\n");
	va(s, "// MIT license (see License.txt)\n");
	va(s, "\n");
	va(s, "// AUTOGENERATED FILE - DO NOT EDIT\n");
	va(s, "\n");
	va(s, "// clang-format off\n");
}

void ForwardDeclarations(std::string& s)
{
	for (const struct_s& o : g_structs)
	{
		if (!o.typedefBaseName.empty())
		{
			va(s, "struct %s;\n", o.typedefBaseName.c_str());
		}
	}
	va(s, "\n");
	for (const struct_s& o : g_structs)
	{
		if (o.typedefBaseName.empty())
		{
			va(s, "struct %s;\n", o.name.c_str());
		}
		else
		{
			va(s, "typedef struct %s %s;\n", o.typedefBaseName.c_str(), o.name.c_str());
		}
	}
}
