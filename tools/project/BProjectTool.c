#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "BProject.h"
#include "BProjectGenerator.h"

static void PrintUsage(const char* executable)
{
    printf(
        "Usage: %s create <display-name> <identifier> <parent-directory> "
        "[--language c|cpp|mixed] [--c-standard N] [--cpp-standard N]\n",
        executable
    );
}

static bool ParseStandard(const char* value, int* outStandard)
{
    char* end = 0;
    long parsed = strtol(value, &end, 10);

    if (value[0] == '\0' || end == 0 || end[0] != '\0' || parsed < 0 || parsed > 9999)
        return false;

    *outStandard = (int)parsed;
    return true;
}

int main(int argumentCount, char** arguments)
{
    if (argumentCount < 5 || strcmp(arguments[1], "create") != 0)
    {
        PrintUsage(arguments[0]);
        return 2;
    }

    BProject project = BProject_Default(arguments[2], arguments[3]);
    BProjectError error;

    for (int i = 5; i < argumentCount; i += 2)
    {
        if (i + 1 >= argumentCount)
        {
            PrintUsage(arguments[0]);
            return 2;
        }

        if (strcmp(arguments[i], "--language") == 0)
        {
            if (!BProject_LanguageModeFromString(arguments[i + 1], &project.languageMode))
            {
                fprintf(stderr, "Unknown language mode: %s\n", arguments[i + 1]);
                return 2;
            }
        }
        else if (strcmp(arguments[i], "--c-standard") == 0)
        {
            if (!ParseStandard(arguments[i + 1], &project.cStandard))
            {
                fprintf(stderr, "Invalid C standard: %s\n", arguments[i + 1]);
                return 2;
            }
        }
        else if (strcmp(arguments[i], "--cpp-standard") == 0)
        {
            if (!ParseStandard(arguments[i + 1], &project.cppStandard))
            {
                fprintf(stderr, "Invalid C++ standard: %s\n", arguments[i + 1]);
                return 2;
            }
        }
        else
        {
            fprintf(stderr, "Unknown option: %s\n", arguments[i]);
            PrintUsage(arguments[0]);
            return 2;
        }
    }

    if (!BProjectGenerator_Create(&project, arguments[4], &error))
    {
        fprintf(stderr, "Project creation failed: %s\n", error.message);
        return 1;
    }

    printf("Created project '%s' at %s/%s\n", project.name, arguments[4], project.identifier);
    return 0;
}
