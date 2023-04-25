// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "sb.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct buildCommands_s buildCommands_t;

AUTOSTRUCT AUTOFROMLOC typedef struct sourceTimestampEntry
{
	sb_t key;
	u64 timestamp;
} sourceTimestampEntry;

AUTOSTRUCT AUTOFROMLOC typedef struct sourceTimestampChain
{
	u32 count;
	u32 allocated;
	sourceTimestampEntry* data;
} sourceTimestampChain;

AUTOSTRUCT AUTOFROMLOC AUTOSTRINGHASH typedef struct sourceTimestampTable
{
	u32 count;
	u32 allocated;
	sourceTimestampChain* data;
} sourceTimestampTable;

AUTOSTRUCT AUTOFROMLOC typedef struct buildDependencyEntry
{
	sb_t key;
	sbs_t deps;
} buildDependencyEntry;

AUTOSTRUCT AUTOFROMLOC typedef struct buildDependencyChain
{
	u32 count;
	u32 allocated;
	buildDependencyEntry* data;
} buildDependencyChain;

AUTOSTRUCT AUTOFROMLOC AUTOSTRINGHASH typedef struct buildDependencyTable
{
	u32 count;
	u32 allocated;
	buildDependencyChain* data;
} buildDependencyTable;

buildDependencyTable buildDependencyTable_init(u32 buckets);
sourceTimestampTable sourceTimestampTable_init(u32 buckets);

typedef enum buildDepRebuild
{
	kBuildDep_NoRebuild,
	kBuildDep_Rebuild,
} buildDepRebuild;

typedef enum buildDepDebug
{
	kBuildDep_NoDebug,
	kBuildDep_Debug,
	kBuildDep_Reasons,
} buildDepDebug;

typedef enum buildDepTraversal
{
	kBuildDep_NoRecurse,
	kBuildDep_Recurse,
} buildDepTraversal;

typedef enum buildDepFileTypes
{
	kBuildDep_CSourceFiles = 0x1,
	kBuildDep_CppSourceFiles = 0x2,
	kBuildDep_AllSourceFiles = kBuildDep_CSourceFiles | kBuildDep_CppSourceFiles,
	kBuildDep_HeaderFiles = 0x4,
	kBuildDep_ObjectFiles = 0x8,
	kBuildDep_NoSourceFiles = kBuildDep_HeaderFiles | kBuildDep_ObjectFiles,
	kBuildDep_AllFiles = kBuildDep_AllSourceFiles | kBuildDep_HeaderFiles | kBuildDep_ObjectFiles,
} buildDepFileTypes;

void buildDependencyTable_insertDir(buildDependencyTable* depTable, sourceTimestampTable* timeTable, sbs_t* sourcePaths, const char* sourceDir, const char* objectDir, buildDepTraversal traversal, buildDepFileTypes fileTypes, buildDepDebug debug);
void buildDependencyTable_insertFile(buildDependencyTable* depTable, sourceTimestampTable* timeTable, sbs_t* sourcePaths, const char* sourcePath, const char* objectDir, buildDepDebug debug);
void buildDependencyTable_addDeps(buildDependencyTable* depTable, sourceTimestampTable* timeTable, const char* objectPath, sbs_t* sourcePaths);

b32 buildDependencyTable_checkDeps(buildDependencyTable* deps, sourceTimestampTable* times, const char* path, buildDepDebug debug);
u32 buildDependencyTable_queueCommands(buildCommands_t* commands, buildDependencyTable* deps, sourceTimestampTable* times, sbs_t* sourcePaths, const char* objectDir, buildDepDebug debug, buildDepRebuild rebuild, const char* dir, const char* parameterizedCommand);

void buildDependencyTable_dump(buildDependencyTable* table);
void sourceTimestampTable_dump(sourceTimestampTable* table);
void buildSources_dump(sbs_t* table, const char* name);

#if defined(__cplusplus)
} // extern "C"
#endif
