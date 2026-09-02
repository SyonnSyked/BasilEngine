#include "BRecentProjects.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <process.h>
#define GET_PROCESS_ID() _getpid()
#else
#include <unistd.h>
#define GET_PROCESS_ID() getpid()
#endif

static int Check(bool condition, const char* message)
{
    if (condition)
        return 0;

    fprintf(stderr, "FAILED: %s\n", message);
    return 1;
}

int main(void)
{
    int failures = 0;
    BRecentProjects recent;
    BRecentProjects_Clear(&recent);

    failures += Check(recent.count == 0, "clear produces an empty list");
    failures += Check(BRecentProjects_Add(&recent, "C:/projects/First/First.basilproject"), "first project can be added");
    failures += Check(BRecentProjects_Add(&recent, "C:/projects/Second/Second.basilproject"), "second project can be added");
    failures += Check(recent.count == 2, "two projects are retained");
    failures += Check(strstr(recent.paths[0], "Second") != 0, "most recent project is first");
    failures += Check(BRecentProjects_Add(&recent, "C:/projects/First/First.basilproject"), "existing project can be promoted");
    failures += Check(recent.count == 2, "promoting a project does not duplicate it");
    failures += Check(strstr(recent.paths[0], "First") != 0, "existing project is promoted to first");
    failures += Check(BRecentProjects_Remove(&recent, 1), "project can be removed");
    failures += Check(recent.count == 1, "removal updates the count");

    char path[BPROJECT_PATH_MAX];
    snprintf(path, sizeof(path), "recent-projects-%ld-%d.json", (long)time(0), (int)GET_PROCESS_ID());
    BProjectError error;
    failures += Check(BRecentProjects_Save(path, &recent, &error), "recent projects can be saved");

    BRecentProjects loaded;
    failures += Check(BRecentProjects_Load(path, &loaded, &error), "recent projects can be loaded");
    failures += Check(loaded.count == recent.count, "loaded count matches");
    failures += Check(strcmp(loaded.paths[0], recent.paths[0]) == 0, "loaded path matches");
    remove(path);

    BRecentProjects missing;
    failures += Check(BRecentProjects_Load("missing-recent-projects.json", &missing, &error), "missing storage starts empty");
    failures += Check(missing.count == 0, "missing storage has no entries");

    if (failures == 0)
        printf("BRecentProjectsTests passed.\n");

    return failures == 0 ? 0 : 1;
}
