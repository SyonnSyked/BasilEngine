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

static bool ReadLine(const char* prompt, char* output, size_t outputSize)
{
    printf("%s", prompt);
    fflush(stdout);

    if (fgets(output, (int)outputSize, stdin) == 0)
        return false;

    output[strcspn(output, "\r\n")] = '\0';
    return true;
}

static void WaitForEnter(void)
{
    char input[8];
    printf("Press Enter to close...");
    fflush(stdout);
    (void)fgets(input, sizeof(input), stdin);
}

static int RunInteractive(void)
{
    char name[BPROJECT_NAME_MAX];
    char identifier[BPROJECT_IDENTIFIER_MAX];
    char parentDirectory[BPROJECT_PATH_MAX];
    char language[16];
    char standard[16];

    printf("BasilEngine Project Creator\n\n");

    if (!ReadLine("Project name: ", name, sizeof(name)) || name[0] == '\0' ||
        !ReadLine("Project identifier (letters, numbers, underscore): ", identifier, sizeof(identifier)) ||
        identifier[0] == '\0' ||
        !ReadLine("Parent directory: ", parentDirectory, sizeof(parentDirectory)) ||
        parentDirectory[0] == '\0' ||
        !ReadLine("Language [mixed/c/cpp] (default mixed): ", language, sizeof(language)))
    {
        fprintf(stderr, "Project creation cancelled: required input was not provided.\n");
        WaitForEnter();
        return 1;
    }

    BProject project = BProject_Default(name, identifier);

    if (language[0] != '\0' &&
        !BProject_LanguageModeFromString(language, &project.languageMode))
    {
        fprintf(stderr, "Unknown language mode: %s\n", language);
        WaitForEnter();
        return 1;
    }

    if (project.languageMode != BPROJECT_LANGUAGE_CPP)
    {
        if (!ReadLine("C standard (default 11): ", standard, sizeof(standard)))
        {
            WaitForEnter();
            return 1;
        }

        if (standard[0] != '\0' && !ParseStandard(standard, &project.cStandard))
        {
            fprintf(stderr, "Invalid C standard: %s\n", standard);
            WaitForEnter();
            return 1;
        }
    }

    if (project.languageMode != BPROJECT_LANGUAGE_C)
    {
        if (!ReadLine("C++ standard (default 26): ", standard, sizeof(standard)))
        {
            WaitForEnter();
            return 1;
        }

        if (standard[0] != '\0' && !ParseStandard(standard, &project.cppStandard))
        {
            fprintf(stderr, "Invalid C++ standard: %s\n", standard);
            WaitForEnter();
            return 1;
        }
    }

    BProjectError error;

    if (!BProjectGenerator_Create(&project, parentDirectory, &error))
    {
        fprintf(stderr, "Project creation failed: %s\n", error.message);
        WaitForEnter();
        return 1;
    }

    printf("\nCreated project '%s' at %s/%s\n", project.name, parentDirectory, project.identifier);
    WaitForEnter();
    return 0;
}

int main(int argumentCount, char** arguments)
{
    if (argumentCount == 1)
        return RunInteractive();

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
