/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "CentralParts.h"

#include "core/Config.h"
#include "util/os.h"
#include "util/paths.h"
#include "util/string.h"
#include <time.h>
#include "version.h"
#include "optparse.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#if defined(MCX_DEBUG)
#if defined(OS_WINDOWS)
#include "Windows.h"
#endif // OS_WINDOWS
#endif // MCX_DEBUG

#define  TIMESTAMPLENGTH   30
#define  CFGSTRINGLENGTH   512
#ifdef OS_64
#define CPU_ARCHITECTURE "64 bit"
#else
#define CPU_ARCHITECTURE "32 bit"
#endif
#ifdef OS_WINDOWS
#define OS_NAME "Windows"
#else
#define OS_NAME "Unix"
#endif

#define MAX_NUM_MSGS 5

void CreateLogHeader(Config * config, LogSeverity sev) {
    char timeString[TIMESTAMPLENGTH];
    char compileTimeString[CFGSTRINGLENGTH];
    char * currentWorkingDirectory;

    time(&config->timeStamp);
    strftime(timeString, TIMESTAMPLENGTH-1, "%a, %Y-%m-%d %H:%M:%S", localtime(&config->timeStamp));
    strcpy(compileTimeString, __DATE__ );
    strcat(compileTimeString, "-");
    strcat(compileTimeString, __TIME__ );

    mcx_log(sev, "**********************************************************************");
    mcx_log(sev, "*********                                                    *********");
    mcx_log(sev, "*********     OpenMCx                                        *********");
    mcx_log(sev, "*********                                                    *********");
    mcx_log(sev, "********* Program started: %s          *********", timeString);
    mcx_log(sev, "*********                                                    *********");
    mcx_log(sev, "********* Build:           %s              *********", compileTimeString);
    mcx_log(sev, "*********                  mcx: %7s                      *********", __mcx_git_repository_id__);
    mcx_log(sev, "*********                                                    *********");
    mcx_log(sev, "********* OS Name:         %-7s                           *********", OS_NAME);
    mcx_log(sev, "*********                                                    *********");
    mcx_log(sev, "********* Architecture:    %s                            *********",CPU_ARCHITECTURE);
    mcx_log(sev, "*********                                                    *********");
    mcx_log(sev, "**********************************************************************");
    mcx_log(sev, " ");
    mcx_log(sev, " ");

    if (config) {
        mcx_log(sev, "******************** Locations: **************************************");
        mcx_log(sev, " ");
        mcx_log(sev, "Executable: %s", config->executable);

        currentWorkingDirectory = mcx_os_get_current_dir();
        mcx_log(sev, "Working directory: %s", currentWorkingDirectory);
        mcx_free(currentWorkingDirectory);

        mcx_log(sev, "Model input file: %s", config->modelFile);
        mcx_log(sev, "Given (relative) path to model: %s", config->relPathToInputFile);
        mcx_log(sev, "Absolute path to model: %s", config->absPathToInputFile);
    }
}

static struct optparse_long long_options[] =
{
    {"tempdir", 't', OPTPARSE_REQUIRED},
    {"resultdir", 'r', OPTPARSE_REQUIRED},
    {"log", 'L', OPTPARSE_REQUIRED},
    {"enablegraphs", 'g', OPTPARSE_NONE},
    {"verbose", 'v', OPTPARSE_NONE},
    {NULL, 0, 0}
};

struct DisplayOption {
    const char * description;
    const char * label;
};

static struct DisplayOption display_options[] =
{
    {"Temporary directory", "TEMPDIR"},
    {"Result directory", "RESULTDIR"},
    {"Log file", "LOGFILE"},
    {"Create graph representation of the model", NULL},
    {"Enable debug logging", NULL},
    {NULL, NULL}
};

static void LogUsage(char ** argv) {
    struct optparse_long * option = long_options;
    size_t i = 0;

    mcx_log(LOG_INFO, "Usage: %s [OPTIONS] INPUTFILE", argv[0]);

    mcx_log(LOG_INFO, "Positional arguments:");
    mcx_log(LOG_INFO, "  INPUTFILE %68s", "Model file");

    mcx_log(LOG_INFO, "Optional arguments:");
    for (i = 0; option->longname; i++, option++) {
        switch (option->argtype) {
            case OPTPARSE_NONE:
                mcx_log(LOG_INFO, "  -%c, --%s", option->shortname, option->longname);
                break;
            case OPTPARSE_REQUIRED:
                mcx_log(LOG_INFO, "  -%c %s, --%s %s", option->shortname, display_options[i].label, option->longname, display_options[i].label);
                break;
            case OPTPARSE_OPTIONAL:
                mcx_log(LOG_INFO, "  -%c [%s], --%s [%s]", option->shortname, display_options[i].label, option->longname, display_options[i].label);
                break;
        }

        mcx_log(LOG_INFO, "%80s", display_options[i].description);
    }
}

static McxStatus ConfigSetupFromCmdLine(Config * config, int argc, char ** argv) {
    char * pos_arg = NULL;
    char * tmp = NULL;
    int option = 0;
    int num_pos_args = 0;

    struct optparse options;

    // init parser
    optparse_init(&options, argc, argv);

    // parse optional arguments
    while ((option = optparse_long(&options, long_options, NULL)) != -1) {
        switch (option) {
            case 'g':
                config->outputModel = TRUE;
                break;
            case 't':
                config->tempDir = mcx_path_get_absolute(options.optarg);
                break;
            case 'v':
                config->writeAllLogFile = TRUE;
                break;
            case 'r':
                config->resultDir = mcx_string_copy(options.optarg);
                break;
            case 'L':
                config->logFile = mcx_string_copy(options.optarg);
                break;
            case '?':
                mcx_log(LOG_ERROR, "%s: %s", argv[0], options.errmsg);
                LogUsage(argv);
                return RETURN_ERROR;
        }
    }

    // parse positional arguments
    while ((tmp = optparse_arg(&options))) {
        num_pos_args++;
        pos_arg = tmp;
    }

    // check that only one model file was given
    if (num_pos_args == 0) {
        mcx_log(LOG_ERROR, "%s: no input file provided", argv[0]);
        LogUsage(argv);
        return RETURN_ERROR;
    } else if (num_pos_args > 1) {
        mcx_log(LOG_ERROR, "%s: too many arguments", argv[0]);
        LogUsage(argv);
        return RETURN_ERROR;
    }

    config->modelFile = mcx_string_copy(pos_arg);
    if (!config->modelFile) {
        mcx_log(LOG_ERROR, "Memory allocation for input file path failed");
        return RETURN_ERROR;
    }
    mcx_path_to_platform(config->modelFile);

    config->executable = mcx_string_copy(argv[0]);
    if (!config->executable) {
        mcx_log(LOG_ERROR, "Memory allocation for executable path failed");
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

static int is_on(const char * value) {
    return 0 == strcmp(value, "1") ||
           0 == mcx_string_cmpi(value, "ON") ||
           0 == mcx_string_cmpi(value, "true");
}

static int is_off(const char * value) {
    return 0 == strcmp(value, "0") ||
           0 == mcx_string_cmpi(value, "OFF") ||
           0 == mcx_string_cmpi(value, "false");
}

static McxStatus ConfigSetupFromEnvironment(Config * config) {
    config->relPathToInputFile = mcx_path_dir_name(config->modelFile);
    config->absPathToInputFile = mcx_path_get_absolute(config->relPathToInputFile);

    CreateLogHeader(config, LOG_INFO);

#if defined(MCX_DEBUG)
#if defined(OS_WINDOWS)
    {
        char * buffer = mcx_os_get_env_var("MC_WAIT_FOR_DEBUGGER_ATTACHED");
        if (buffer) {
            if (is_on(buffer)) {
                while (!IsDebuggerPresent()) ;
            }
        }
    }
#endif // OS_WINDOWS
#endif // MCX_DEBUG

    {
        const char * currDir = mcx_os_get_current_dir();
        const char * pathList[] = {currDir, "temp"};
        mcx_path_merge(pathList, 2, &config->localTempDir);
        mcx_free((char *) currDir);
    }

    if (NULL == config->tempDir) {
        char * buffer = mcx_os_get_env_var("USE_TEMP_DIR");

        if (buffer) {
            config->tempDir = mcx_path_get_absolute(buffer);
            mcx_free(buffer);
        }
        if (NULL == config->tempDir) {
            // use a relative local path
        } else {
            config->tempDirComandLine = FALSE;
        }
    } else {
        config->tempDirComandLine = TRUE;
    }

    {
        char * enableGraphs = mcx_os_get_env_var("MC_ENABLE_GRAPHS");
        if (enableGraphs) {
            if (is_on(enableGraphs)) {
                config->outputModel = TRUE;
            }
            mcx_free(enableGraphs);
        }
    }

    {
        char * flushStore = mcx_os_get_env_var("FLUSH_STORE");
        if (flushStore) {
            if (is_on(flushStore)) {
                config->flushEveryStore = TRUE;
            }
            mcx_free(flushStore);
        }
    }

    {
        char * sumTime = mcx_os_get_env_var("SUM_TIME");
        if (sumTime) {
            config->sumTimeDefined = TRUE;
            if (is_off(sumTime)) {
                config->sumTime = FALSE;
                mcx_log(LOG_INFO, "SUM_TIME = 0");
            } else {
                config->sumTime = TRUE;
            }
            mcx_free(sumTime);
        }
    }

    {
        char * profilingMode = mcx_os_get_env_var("MC_PROFILING");
        if (profilingMode) {
            if (!is_off(profilingMode)) {
                mcx_log(LOG_INFO, "Development mode turned on");
                config->profilingMode = TRUE;
            }

            mcx_free(profilingMode);
        }
    }

    {
        char * str = mcx_os_get_env_var("MC_INTERPOLATION_BUFFER_SIZE");
        if (str) {
            int size = atoi(str);
            if (size <= 0) {
                mcx_log(LOG_WARNING, "Invalid interpolation filter buffer size (%d). Falling back to the default (%zu)",
                        size, config->interpolationBuffSize);
            } else {
                config->interpolationBuffSize = (size_t) size;
                mcx_log(LOG_INFO, "Interpolation filter buffer size: %zu", config->interpolationBuffSize);
            }
            mcx_free(str);
        }
    }

    {
        char * str = mcx_os_get_env_var("MC_OVERRIDE_INTERPOLATION_BUFFER_SIZE");
        if (str) {
            int size = atoi(str);
            if (size <= 0) {
                mcx_log(LOG_WARNING, "Invalid interpolation filter override buffer size (%d)", size);
            } else {
                config->overrideInterpolationBuffSize = (size_t) size;
                mcx_log(LOG_INFO, "Interpolation filter override buffer size: %zu", config->overrideInterpolationBuffSize);
            }
            mcx_free(str);
        }
    }

    {
        char * str = mcx_os_get_env_var("MC_INTERPOLATION_BUFFER_SIZE_LIMIT");
        if (str) {
            int size = atoi(str);
            if (size <= 0) {
                mcx_log(LOG_WARNING, "Invalid interpolation filter buffer size limit (%d)", size);
            } else {
                config->interpolationBuffSizeLimit = (size_t) size;
                mcx_log(LOG_INFO, "Interpolation filter buffer size limit: %zu", config->interpolationBuffSizeLimit);
            }
            mcx_free(str);
        }
    }

    {
        char * str = mcx_os_get_env_var("MC_INTERPOLATION_BUFFER_SIZE_SAFE_EXT");
        if (str) {
            int size = atoi(str);
            if (size <= 0) {
                mcx_log(LOG_WARNING, "Invalid interpolation filter buffer size safety extension (%d)", size);
            } else {
                config->interpolationBuffSizeSafetyExt = (size_t) size;
                mcx_log(LOG_INFO, "Interpolation filter buffer size safety extension: %zu", config->interpolationBuffSizeSafetyExt);
            }
            mcx_free(str);
        }
    }

    {
        char * disableMemFilter = NULL;

        disableMemFilter = mcx_os_get_env_var("MC_DISABLE_MEM_FILTER");
        if (disableMemFilter) {
            if (is_on(disableMemFilter)) {
                config->useMemFilter = FALSE;
            }
            mcx_free(disableMemFilter);
        }
    }

    {
        char * str = mcx_os_get_env_var("MC_MEM_FILTER_HISTORY_LIMIT");
        if (str) {
            int size = atoi(str);
            if (size <= 0) {
                mcx_log(LOG_WARNING, "Invalid memory filter history size limit (%s)", str);
            } else {
                config->memFilterHistoryLimit = (size_t) size;
                mcx_log(LOG_INFO, "Memory filter history size limit: %zu", config->memFilterHistoryLimit);
            }
            mcx_free(str);
        }
    }

    {
        char * str = mcx_os_get_env_var("MC_MEM_FILTER_HISTORY_EXTRA");
        if (str) {
            int size = atoi(str);
            if (size <= 0) {
                mcx_log(LOG_WARNING, "Invalid memory filter extra history size (%s)", str);
            } else {
                config->memFilterHistoryExtra = (size_t) size;
                mcx_log(LOG_INFO, "Memory filter extra history size: %zu", config->memFilterHistoryExtra);
            }
            mcx_free(str);
        }
    }

    {
        char * cosimInitEnabled = NULL;

        cosimInitEnabled = mcx_os_get_env_var("MC_COSIM_INIT");
        if (cosimInitEnabled) {
            if (is_on(cosimInitEnabled)) {
                config->cosimInitEnabled = TRUE;
            }
            mcx_free(cosimInitEnabled);
        }
    }

    {
        char * wrongInitBehaviorDisabled = NULL;

        wrongInitBehaviorDisabled = mcx_os_get_env_var("MCX_WRONG_INIT_BEHAVIOR");
        if (wrongInitBehaviorDisabled) {
            if (is_on(wrongInitBehaviorDisabled)) {
                config->patchWrongInitBehavior = FALSE;
            }
            mcx_free(wrongInitBehaviorDisabled);
        }
    }

    {
        char * numWarnings = mcx_os_get_env_var("NUM_TIME_SNAP_WARNINGS");
        if (numWarnings) {
            int num = atoi(numWarnings);
            config->maxNumTimeSnapWarnings = (num > 0) ? (size_t) num : 0;
            mcx_log(LOG_INFO, "Environment variable NUM_TIME_SNAP_WARNINGS = %d", config->maxNumTimeSnapWarnings);
            mcx_free(numWarnings);
        }
    }

    {
        char * str = mcx_os_get_env_var("MC_NAN_CHECK");

        /* default */
        config->nanCheck = NAN_CHECK_ALWAYS;

        if (str) {
            if (0 == strcmp(str, "2")) {
                mcx_log(LOG_INFO, "NaN-check enabled");
                config->nanCheck = NAN_CHECK_ALWAYS;
            } else if (0 == strcmp(str, "1")) {
                mcx_log(LOG_INFO, "NaN-check enabled for connected");
                config->nanCheck = NAN_CHECK_CONNECTED;
            } else if (0 == strcmp(str, "0")) {
                mcx_log(LOG_INFO, "NaN-check disabled");
                config->nanCheck = NAN_CHECK_NEVER;
            } else {
                mcx_log(LOG_INFO, "Invalid value \"%s\" for MC_NAN_CHECK", str);
                return RETURN_ERROR;
            }
            mcx_free(str);
        }
    }

    {
        char * str = mcx_os_get_env_var("MC_NUM_NAN_CHECK_MESSAGES");
        if (str) {
            config->nanCheckNumMessages = atoi(str);

            mcx_log(LOG_INFO, "NaN-check limited to %d messages", config->nanCheckNumMessages);
            mcx_free(str);
        }
    }

    return RETURN_OK;
}

static McxStatus ConfigSetupFromInput(Config * config, ConfigInput * configInput) {
    if (configInput == NULL) {
        mcx_log(LOG_WARNING, "ConfigSetupFromInput: no input provided (null)");
        return RETURN_WARNING;
    }

    return RETURN_OK;
}

void PrintConfig(const Config * config) {
    if (NULL == config) {
        return;
    }

    if (NULL == config->tempDir) {
        mcx_log(LOG_INFO, "No temporary directory given, using %s", config->localTempDir);
    } else {
        mcx_log(LOG_INFO, "Temporary directory: %s", config->tempDir);
        if (TRUE == config->tempDirComandLine) {
            mcx_log(LOG_DEBUG, "Temporary directory (command-line): %s", config->tempDir);
        } else {
            mcx_log(LOG_DEBUG, "Temporary directory (environment): %s", config->tempDir);
        }
    }
    if (TRUE == config->flushEveryStore) {
        mcx_log(LOG_DEBUG, "Config: Enabled flush after each store");
    }

    mcx_log(LOG_INFO, " ");
    mcx_log(LOG_INFO, " ");
}

char * ConfigGetTempDir(const Config * config) {
    if (!config) {
        return NULL;
    }

    return config->tempDir;
}

char * ConfigGetLocalTempDir(const Config * config) {
    if (!config) {
        return NULL;
    }

    return config->localTempDir;
}

static void ConfigDestructor(void * self) {
    Config * config = (Config *) self;

    if (config->relPathToInputFile) {
        mcx_free(config->relPathToInputFile);
    }
    if (config->absPathToInputFile) {
        mcx_free(config->absPathToInputFile);
    }
    if (config->tempDir) {
        mcx_free(config->tempDir);
    }
    if (config->localTempDir) {
        mcx_free(config->localTempDir);
    }
    if (config->resultDir) {
        mcx_free(config->resultDir);
    }
    if (config->logFile) {
        mcx_free(config->logFile);
    }
    if (config->modelFile) {
        mcx_free(config->modelFile);
    }
    if (config->executable) {
        mcx_free(config->executable);
    }
}

static Config * ConfigCreate(Config * config) {
    config->SetupFromCmdLine = ConfigSetupFromCmdLine;
    config->SetupFromEnvironment = ConfigSetupFromEnvironment;
    config->SetupFromInput = ConfigSetupFromInput;
    time(&config->timeStamp);

    config->relPathToInputFile = NULL;
    config->absPathToInputFile = NULL;

    config->outputModel = FALSE;

    config->tempDir = NULL;
    config->localTempDir = NULL;
    config->tempDirComandLine = FALSE;

    config->resultsEnabled = 1;
    config->resultDir = NULL;

    config->logFile = NULL;

    config->modelFile = NULL;

    config->executable = NULL;

    config->flushEveryStore = FALSE;
    config->sumTimeDefined = FALSE;
    config->sumTime = TRUE;

    config->writeAllLogFile = FALSE;

    config->cosimInitEnabled = FALSE;
    config->patchWrongInitBehavior = TRUE;

    config->maxNumTimeSnapWarnings = MAX_NUM_MSGS;

    config->nanCheck = NAN_CHECK_ALWAYS;
    config->nanCheckNumMessages = MAX_NUM_MSGS;

    config->interpolationBuffSize = 1000;
    config->overrideInterpolationBuffSize = 0;
    config->interpolationBuffSizeLimit = 100000;
    config->interpolationBuffSizeSafetyExt = 1;

    config->useMemFilter = TRUE;
    config->memFilterHistoryLimit = 10000000;
    config->memFilterHistoryExtra = 1;

    config->profilingMode = FALSE;

    return config;
}

OBJECT_CLASS(Config, Object);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */