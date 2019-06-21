// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#include "recorder_thread.h"
#include "bb_structs_generated.h"
#include "message_queue.h"
#include "recordings.h"

#include "bbclient/bb_log.h"
#include "bbclient/bb_packet.h"
#include "bbclient/bb_string.h"
#include "bbclient/bb_time.h"
#include "uuid_rfc4122/uuid.h"

#include "bbclient/bb_wrap_stdio.h"
#include <string.h>

#if BB_USING(BB_PLATFORM_WINDOWS)
BB_WARNING_DISABLE(4710) // snprintf not inlined - can't push/pop because it happens later
#endif                   // #if BB_USING( BB_PLATFORM_WINDOWS )

#if BB_USING(BB_PLATFORM_WINDOWS)
// warning C4820 : 'StructName' : '4' bytes padding added after data member 'MemberName'
// warning C4255: 'FuncName': no function prototype given: converting '()' to '(void)'
BB_WARNING_PUSH(4820 4255)
#include <ShlObj.h>
#include <direct.h>
#include <errno.h>
b32 mkdir_recursive(const char *path)
{
	b32 success = true;
	char *temp = _strdup(path);
	char *s = temp;
	while(*s) {
		if(*s == '/' || *s == '\\') {
			char c = *s;
			*s = '\0';
			if(s - temp > 2) {
				if(_mkdir(temp) == -1) {
					if(errno != EEXIST) {
						success = false;
					}
				}
			}
			*s = c;
		}
		++s;
	}
	free(temp);
	if(_mkdir(path) == -1) {
		if(errno != EEXIST) {
			success = false;
		}
	}
	return success;
}
void get_appdata_folder(char *buffer, size_t bufferSize)
{
	size_t len;
	char appData[_MAX_PATH] = "C:";
	SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, appData);
	len = strlen(appData);
	bb_strncpy(buffer, appData, bufferSize);
	bb_strncpy(buffer + len, "\\bb", bufferSize - len);
	mkdir_recursive(buffer);
}
BB_WARNING_POP;
#else
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
const char *errno_str(int e)
{
	switch(e) {
#define CASE(x) \
	case x:     \
		return #x
		CASE(EACCES);
		CASE(EEXIST);
		CASE(ELOOP);
		CASE(EMLINK);
		CASE(ENAMETOOLONG);
		CASE(ENOENT);
		CASE(ENOSPC);
		CASE(ENOTDIR);
		CASE(EROFS);
	case 0:
		return "success";
	default:
		return "???";
	}
}

void mkdir_recursive(const char *path)
{
	mode_t process_mask = umask(0);
	int ret = mkdir(path, S_IRWXU);
	umask(process_mask);
	int e = errno;
	bb_log("mkdir '%s' returned %d (errno %d %s)\n", path, ret, e, errno_str(e));
}
void get_appdata_folder(char *buffer, size_t bufferSize)
{
	char temp[1024] = "~";
	struct passwd pwd;
	char *home = getenv("HOME");
	if(!home) {
		struct passwd *ppwd = NULL;
		getpwuid_r(getuid(), &pwd, temp, sizeof(temp), &ppwd);
		if(ppwd) {
			home = ppwd->pw_dir;
		}
	}

	bb_snprintf(buffer, bufferSize, "%s/.bb", home);
	buffer[bufferSize - 1] = '\0';
	mkdir_recursive(buffer);
}
#endif

void sanitize_app_filename(const char *applicationName, char *applicationFilename, size_t applicationFilenameLen)
{
	const char *invalidCharacters = "<>:\"/\\|?*";
	const char *src = applicationName;
	char *dest = applicationFilename;
	while(*src && dest - applicationFilename < (ptrdiff_t)(applicationFilenameLen - 1)) {
		if(*src >= 32 && (strchr(invalidCharacters, *src) == NULL)) {
			*dest++ = *src;
		}
		++src;
	}
	*dest = 0;
}

bb_thread_return_t recorder_thread(void *args)
{
	FILE *fp;
	bb_server_connection_data_t *data = (bb_server_connection_data_t *)args;
	bb_connection_t *con = &data->con;
	char path[1024];
	char dir[1024];
	rfc_uuid uuid;
	char uuidBuffer[64];
	char applicationName[kBBSize_ApplicationName];
	sanitize_app_filename(data->applicationName, applicationName, sizeof(applicationName));
	if(bb_snprintf(dir, sizeof(dir), "%s recorder %p", data->applicationName, con) < 0) {
		dir[sizeof(dir) - 1] = '\0';
	}

	BB_THREAD_START(dir);
	bbthread_set_name(dir);

	get_appdata_folder(dir, sizeof(dir));
	if(bb_snprintf(path, sizeof(path), "%s/%s", dir, applicationName) < 0) {
		path[sizeof(path) - 1] = '\0';
	}
	mkdir_recursive(path);
	uuid_create(&uuid);
	format_uuid(&uuid, uuidBuffer, sizeof(uuidBuffer));
	if(bb_snprintf(path, sizeof(path), "%s\\%s\\{%s}%s.bbox", dir, applicationName, uuidBuffer, applicationName) < 0) {
		path[sizeof(path) - 1] = '\0';
	}
	path[sizeof(path) - 1] = '\0';
	BB_LOG("bb::recorder", "recorder con %p using path %s", con, path);
	fp = fopen(path, "wb");
	if(fp) {
		b32 sentRecordingStart = false;
		b32 dirty = false;
		u64 lastFlush = bb_current_time_ms();
		new_recording_t recording;
		recording.applicationName = sb_from_c_string(data->applicationName);
		recording.applicationFilename = sb_from_c_string(applicationName);
		recording.path = sb_from_c_string(path);
		recording.openView = false;
		recording.recordingType = kRecordingType_Normal;
		recording.mqId = mq_invalid_id();
		recording.platform = kBBPlatform_Unknown;
		GetSystemTimeAsFileTime(&recording.filetime);
		while(!*data->shutdownRequest) {
			if(bbcon_is_connected(con)) {
				bb_decoded_packet_t decoded;

				if(recording.mqId != mq_invalid_id()) {
					const message_queue_message_t *outgoingMessage = mq_peek(recording.mqId);
					if(outgoingMessage) {
						b32 valid = false;
						bb_decoded_packet_t outgoing;
						memset(&outgoing, 0, sizeof(outgoing));
						if(outgoingMessage->command == kBBPacketType_ConsoleCommand) {
							valid = true;
							bb_strncpy(outgoing.packet.consoleCommand.text, outgoingMessage->text, sizeof(decoded.packet.consoleCommand.text));
						}
						if(valid) {
							outgoing.type = outgoingMessage->command;
							if(bbcon_try_send(con, &outgoing)) {
								mq_consume_peek_result(recording.mqId);
							}
						} else {
							mq_consume_peek_result(recording.mqId);
						}
					}
				}

				bbcon_tick(con);
				while(bbcon_decodePacket(con, &decoded)) {
					// #TODO: return buffer, not decoded packets...
					u8 buf[BB_MAX_PACKET_BUFFER_SIZE];
					u16 serializedLen = bbpacket_serialize(&decoded, buf + 2, sizeof(buf) - 2);
					if(serializedLen) {
						serializedLen += 2;
						buf[0] = (u8)(serializedLen >> 8);
						buf[1] = (u8)(serializedLen & 0xFF);
						fwrite(buf, serializedLen, 1, fp);
					}
					if(bbpacket_is_app_info_type(decoded.type)) {
						fflush(fp);
						lastFlush = bb_current_time_ms();
						if(!sentRecordingStart) {
							sentRecordingStart = true;
							if(decoded.type == kBBPacketType_AppInfo_v1 ||
							   ((decoded.packet.appInfo.initFlags & kBBInitFlag_NoOpenView) == 0)) {
								recording.openView = true;
							}
							if((decoded.packet.appInfo.initFlags & kBBInitFlag_ConsoleCommands) != 0) {
								recording.mqId = mq_acquire();
							}
							recording.platform = decoded.packet.appInfo.platform;
							to_ui(kToUI_RecordingStart, "%s", recording_build_start_identifier(recording));
						}
					} else {
						dirty = true;
					}
				}
				if(dirty) {
					u64 now = bb_current_time_ms();
					if(now - lastFlush > 100) {
						fflush(fp);
						lastFlush = now;
					}
				}
			} else if(bbcon_is_connecting(con)) {
				bbcon_tick_connecting(con);
			} else {
				break;
			}
		}

		fclose(fp);
		if(!sentRecordingStart) {
			sentRecordingStart = true;
			to_ui(kToUI_RecordingStart, "%s", recording_build_start_identifier(recording));
		}
		to_ui(kToUI_RecordingStop, "%s\n%s", data->applicationName, path);
		mq_releaseref(recording.mqId);
		new_recording_reset(&recording);
	}

	bbnet_gracefulclose(&con->socket);

	bb_thread_exit(0);
}
