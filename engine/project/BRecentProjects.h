#ifndef BASIL_ENGINE_RECENT_PROJECTS_H
#define BASIL_ENGINE_RECENT_PROJECTS_H

#include "BProject.h"

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BRECENT_PROJECTS_MAX 12

typedef struct BRecentProjects
{
    size_t count;
    char paths[BRECENT_PROJECTS_MAX][BPROJECT_PATH_MAX];
} BRecentProjects;

void BRecentProjects_Clear(BRecentProjects* recentProjects);
bool BRecentProjects_Add(BRecentProjects* recentProjects, const char* manifestPath);
bool BRecentProjects_Remove(BRecentProjects* recentProjects, size_t index);
bool BRecentProjects_Load(const char* path, BRecentProjects* recentProjects, BProjectError* error);
bool BRecentProjects_Save(const char* path, const BRecentProjects* recentProjects, BProjectError* error);

#ifdef __cplusplus
}
#endif

#endif
