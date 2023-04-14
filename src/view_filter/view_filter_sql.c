// Copyright (c) 2012-2021 Matt Campbell
// MIT license (see License.txt)

#include "view_filter_sql.h"
#include "recorded_session.h"
#include "view.h"
#include <sqlite/wrap_sqlite3.h>

static b32 sqlite3_bind_u32_and_log(sqlite3* db, sqlite3_stmt* stmt, int index, u32 value)
{
	int rc = sqlite3_bind_int(stmt, index, value);
	if (rc == SQLITE_OK)
	{
		return true;
	}
	else
	{
		BB_ERROR("sqlite", "SQL bind error: index:%d value:%u result:%d error:%s\n", index, value, rc, sqlite3_errmsg(db));
		return false;
	}
}

static b32 sqlite3_bind_text_and_log(sqlite3* db, sqlite3_stmt* stmt, int index, const char* value)
{
	int rc = sqlite3_bind_text(stmt, index, value, -1, SQLITE_STATIC);
	if (rc == SQLITE_OK)
	{
		return true;
	}
	else
	{
		BB_ERROR("sqlite", "SQL bind error: index:%d value:%s result:%d error:%s\n", index, value, rc, sqlite3_errmsg(db));
		return false;
	}
}

static int test_sql_callback(void* userData, int argc, char** argv, char** colNames)
{
	BB_UNUSED(argc);
	BB_UNUSED(argv);
	BB_UNUSED(colNames);
	int* count = (int*)userData;
	*count += 1;
	return 0;
}

static b32 view_sqlWhere_visible_internal(view_t* view, recorded_log_t* log)
{
	if (!view->db || !view->sqlSelect.count)
		return false;

	bb_decoded_packet_t* decoded = &log->packet;
	const view_category_t* viewCategory = view_find_category(view, decoded->packet.logText.categoryId);
	const char* level = bb_get_log_level_name(decoded->packet.logText.level, "Unknown");

	int rc;
	const char* insert_sql = "INSERT INTO logs (line, category, level, pie, text) VALUES(?, ?, ?, ?, ?)";
	sqlite3_stmt* insert_stmt = NULL;
	rc = sqlite3_prepare_v2(view->db, insert_sql, -1, &insert_stmt, NULL);
	if (rc != SQLITE_OK)
	{
		return false;
	}

	sqlite3_bind_u32_and_log(view->db, insert_stmt, 1, log->sessionLogIndex);
	if (viewCategory)
	{
		sqlite3_bind_text_and_log(view->db, insert_stmt, 2, viewCategory->categoryName);
	}
	sqlite3_bind_text_and_log(view->db, insert_stmt, 3, level);
	sqlite3_bind_u32_and_log(view->db, insert_stmt, 4, decoded->packet.logText.pieInstance);
	sqlite3_bind_text_and_log(view->db, insert_stmt, 5, decoded->packet.logText.text);

	rc = sqlite3_step(insert_stmt);
	if (SQLITE_DONE != rc)
	{
		BB_ERROR("sqlite", "SQL error running INSERT INTO: result:%d error:%s\n", rc, sqlite3_errmsg(view->db));
	}
	sqlite3_clear_bindings(insert_stmt);
	sqlite3_reset(insert_stmt);
	sqlite3_finalize(insert_stmt);

	sb_clear(&view->sqlSelectError);
	int count = 0;
	char* errorMessage = NULL;
	rc = sqlite3_exec(view->db, view->sqlSelect.data, test_sql_callback, &count, &errorMessage);
	if (rc != SQLITE_OK)
	{
		sb_va(&view->sqlSelectError, "SQL error: %s", errorMessage);
		sqlite3_free(errorMessage);
	}

	return count > 0;
}

b32 view_filter_visible_sql(view_t* view, recorded_log_t* log)
{
	if (!view->db || !view->sqlSelect.count)
		return true;

	int rc;
	char* errorMessage;

	rc = sqlite3_exec(view->db, "BEGIN TRANSACTION", NULL, NULL, &errorMessage);
	if (rc != SQLITE_OK)
	{
		BB_ERROR("sqlite", "SQL error running BEGIN TRANSACTION ret:%d error:%s\n", rc, errorMessage);
		sqlite3_free(errorMessage);
		return true;
	}

	b32 result = view_sqlWhere_visible_internal(view, log);

	rc = sqlite3_exec(view->db, "ROLLBACK", NULL, NULL, &errorMessage);
	if (rc != SQLITE_OK)
	{
		BB_ERROR("sqlite", "SQL error running ROLLBACK ret:%d error:%s\n", rc, errorMessage);
		sqlite3_free(errorMessage);
	}

	return result;
}
