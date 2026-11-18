/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "core/Config.h"
#include "core/Task.h"
#include "core/Model.h"

#include "reader/Reader.h"

#include "Memory.h"

#include "util/os.h"
#include "util/string.h"
#include "util/signals.h"
#include "util/time.h"

#include <time.h>

#if defined (ENABLE_MT)
#include "util/mutex.h"
#endif // ENABLE_MT

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

FILE * simulation_log = NULL;
FILE * mcx_all_log    = NULL;
static int write_to_stdout = TRUE;
void * _mcx_malloc(size_t len, const char * funct) {
    void * obj = NULL;
#if defined(MEMORY_DEBUG)
    mcx_printf("alloc %zu from %s\n", len, funct);
#endif //defined(MEMORY_DEBUG)
    obj = malloc(len);
    if (len > 0 && !obj) {
        mcx_log(LOG_ERROR, "No Memory");
    }
    return obj;
}


void _mcx_free(void * obj, const char * funct) {
#if defined(MEMORY_DEBUG)
    mcx_printf("free %p from %s\n", obj, funct);
#endif //defined(MEMORY_DEBUG)
    free(obj);
}

void * _mcx_realloc(void * obj, size_t size, const char * funct) {
    void * newMemory = NULL;
#if defined(MEMORY_DEBUG)
    mcx_printf("realloc %p to length %zu from %s\n", obj, size, funct);
#endif //defined(MEMORY_DEBUG)
    newMemory = realloc(obj, size);
    if (size > 0 && !newMemory) {
        mcx_log(LOG_ERROR, "No Memory");
    }
    return newMemory;
}

void * _mcx_calloc(size_t num, size_t size, const char * funct) {
    void * obj = NULL;
#if defined(MEMORY_DEBUG)
    mcx_printf("calloc %zu * %zu from %s\n", num, size, funct);
#endif //defined(MEMORY_DEBUG)
    obj = calloc(num, size);
    if (num > 0 && size > 0 && !obj) {
        mcx_log(LOG_ERROR, "No Memory");
    }
    return obj;
}

McxStatus mcx_vlog(LogSeverity sev, const char *fmt, va_list args);
McxStatus mcx_log(LogSeverity sev, const char *fmt, ...);

#if defined (ENABLE_MT)
McxMutex logMutex;
#endif //ENABLE_MT

static void EnableAllLogFile(void) {
    mcx_all_log = mcx_os_fopen("mcx_all.log", "w");
}

static void InitLogFile(const char * logFileName) {
    simulation_log = mcx_os_fopen(logFileName, "w");

}

static void SetupLogFiles(const char * sim_log_file, int write_all_log_file) {
    InitLogFile(sim_log_file ? sim_log_file : "simulation.log");

    if (write_all_log_file) {
        EnableAllLogFile();
    }
}

static void CleanupLogFile(void) {
    if (simulation_log) {
        fclose(simulation_log);
        simulation_log = NULL;
    }
    if (mcx_all_log) {
        fclose(mcx_all_log);
        mcx_all_log = NULL;
    }
}

static const char * GetLogSeverityString(LogSeverity sev) {
    switch(sev) {
    case LOG_DEBUG:
        return "";
    case LOG_INFO:
        return "";
    case LOG_WARNING:
        return "Warning: ";
    case LOG_ERROR:
        return "ERROR: ";
    case LOG_FATAL:
        return "ERROR: ";
    }
    return "";
}

McxStatus mcx_write_log(LogSeverity sev, const char *fmt, size_t writeNewline, va_list args) {
#define MSG_MAX_SIZE 2048
    char msg[MSG_MAX_SIZE];
    char * msgCopy;
    char * line;
    char lastChar = '\0';
    char logLine_[MSG_MAX_SIZE];
    char * logLine = logLine_;
    char allLogLine_[MSG_MAX_SIZE];
    char * allLogLine = allLogLine_;
    int n = 0;
    int size = MSG_MAX_SIZE;
    static size_t startNewLine = 1;
    size_t len = strlen(fmt);

    msg[0] = '\0';

    if (0 == len) {
        return RETURN_OK;
    }

    n = vsnprintf(msg, MSG_MAX_SIZE, fmt, args);

    msgCopy = msg;

#if defined (ENABLE_MT)
    mcx_mutex_lock(&logMutex);
#endif //ENABLE_MT

    do {
        line = mcx_string_sep(&msgCopy, "\n"); //Get next line of msg

        if (1 == startNewLine) {
            n = snprintf(logLine, MSG_MAX_SIZE, "%s%s", GetLogSeverityString(sev), line);
            n = snprintf(allLogLine, MSG_MAX_SIZE, "[%10d] %s%s", (int)clock(), GetLogSeverityString(sev), line);
        }
        else {
            n = snprintf(logLine, MSG_MAX_SIZE, "%s", line);
            n = snprintf(allLogLine, MSG_MAX_SIZE, "%s", line);
        }

        if (write_to_stdout) {
            if (NULL == msgCopy && 0 == writeNewline) {  // last line of msg
                printf("%s", logLine);
            } else {
                printf("%s\n", logLine);
            }
            fflush(stdout);
        }

        if (simulation_log) {
            if (sev >= LOG_INFO) {
                if (NULL == msgCopy && 0 == writeNewline) { //last line of msg
                    fprintf(simulation_log, "%s", logLine);
                }
                else {
                    fprintf(simulation_log, "%s\n", logLine);
                }
                fflush(simulation_log);
            }
        }
        if (mcx_all_log) {
            if (NULL == msgCopy && 0 == writeNewline) { //last line of msg
                fprintf(mcx_all_log, "%s", allLogLine);
            }
            else {
                fprintf(mcx_all_log, "%s\n", allLogLine);
            }
            fflush(mcx_all_log);
        }
        if (NULL == msgCopy && '\0' != *line) {//in the last line of msg check if the last char written was \n, then startNewLine has to be 1 no matter what writeNewLine is.
            startNewLine = writeNewline;
        }
        else {
            startNewLine = 1;
        }
    } while (NULL != msgCopy);

#if defined (ENABLE_MT)
    mcx_mutex_unlock(&logMutex);
#endif //ENABLE_MT

    return RETURN_OK;
}

McxStatus mcx_vlog(LogSeverity sev, const char *fmt, va_list args) {
    return mcx_write_log(sev, fmt, 1, args);
}

McxStatus mcx_vlog_no_newline(LogSeverity sev, const char *fmt, va_list args) {
    return mcx_write_log(sev, fmt, 0, args);
}


McxStatus mcx_log(LogSeverity sev, const char *fmt, ...) {
    McxStatus retVal = RETURN_OK;

    va_list args;
    va_start(args, fmt);

    retVal = mcx_vlog(sev, fmt, args);

    va_end(args);

    return RETURN_OK;
}

McxStatus mcx_log_no_newline(LogSeverity sev, const char *fmt, ...) {
    McxStatus retVal = RETURN_OK;

    va_list args;
    va_start(args, fmt);

    retVal = mcx_vlog_no_newline(sev, fmt, args);

    va_end(args);

    return RETURN_OK;
}

static int mcx_log_printf(const char * fmt, ...) {
    McxStatus retVal = RETURN_OK;
    va_list args;
    va_start(args, fmt);

    retVal = mcx_vlog(LOG_INFO, fmt, args);

    va_end(args);

    return 0;
}

static McxStatus CompWriteDebugInfoAfterSimulation(Component * comp, void * param) {
    return comp->WriteDebugInfoAfterSimulation(comp);
}

static void model_stats(Model * model) {
    SubModel * subModel = model->subModel;

    subModel->LoopComponents(subModel, CompWriteDebugInfoAfterSimulation, NULL);
}



McxStatus RunMCX(int argc, char *argv[]) {
    Config * config = NULL;
    Model * model = NULL;
    Task * task = NULL;

    McxTime clock_begin;
    McxTime clock_read_begin, clock_read_end;
    McxTime clock_setup_begin, clock_setup_end;
    McxTime clock_sim_begin, clock_sim_end;
    McxTime clock_cleanup_begin, clock_cleanup_end;
    McxTime clock_init_begin, clock_init_end;
    double cpu_time_sec;
    McxTime cpu_time_used;

    McxTime time_begin;
    McxTime time_read_begin, time_read_end;
    McxTime time_setup_begin, time_setup_end;
    McxTime time_sim_begin, time_sim_end;
    McxTime time_cleanup_begin, time_cleanup_end;
    McxTime time_init_begin, time_init_end;
    McxTime time_diff;

    double wall_time_sec;

    int logInitialized = 0;

    McxStatus retVal = RETURN_OK;

    InputRoot * mcxInput = NULL;
    InputElement * element = NULL;

    Reader * reader = NULL;

    mcx_cpu_time_get(&clock_begin);
    mcx_time_get(&time_begin);

    config = (Config *) object_create(Config);
    if (!config) {
        mcx_log(LOG_ERROR, "Could not create config");
        retVal = RETURN_ERROR;
        goto cleanup;
    }

    mcx_signal_handler_set_function("ConfigSetupFromCmdLine");
    retVal = config->SetupFromCmdLine(config, argc, argv);
    mcx_signal_handler_unset_function();
    if (retVal == RETURN_ERROR) {
        goto cleanup;
    }

    mcx_signal_handler_set_function("SetupLogFiles");
    SetupLogFiles(config->logFile, config->writeAllLogFile);
    mcx_signal_handler_unset_function();
    logInitialized = 1;
    mcx_signal_handler_set_function("ConfigSetupFromEnvironment");
    retVal = config->SetupFromEnvironment(config);
    mcx_signal_handler_unset_function();
    if (RETURN_OK != retVal) {
        mcx_log(LOG_INFO, "Setting up configuration from environment failed");
        retVal = RETURN_ERROR;
        goto cleanup;
    }

    reader = (Reader*)object_create(Reader);
    if (!reader) {
        mcx_log(LOG_ERROR, "Could not create input file reader");
        retVal = RETURN_ERROR;
        goto cleanup;
    }

    mcx_signal_handler_set_function("ReaderSetup");
    retVal = reader->Setup(reader, config->modelFile, config);
    mcx_signal_handler_unset_function();
    if (retVal == RETURN_ERROR) {
        mcx_log(LOG_ERROR, "Input reader setup failed");
        retVal = RETURN_ERROR;
        goto cleanup;
    }

    mcx_signal_handler_set_function("ReaderRead");
    mcxInput = reader->Read(reader, config->modelFile);
    mcx_signal_handler_unset_function();
    if (!mcxInput) {
        mcx_log(LOG_ERROR, "Parsing of input file failed");
        retVal = RETURN_ERROR;
        goto cleanup;
    }
    element = (InputElement *) mcxInput;

    mcx_signal_handler_set_function("ConfigSetupFromInput");
    retVal = config->SetupFromInput(config, mcxInput->config);
    mcx_signal_handler_unset_function();
    if (RETURN_OK != retVal) {
        mcx_log(LOG_ERROR, "Setting up configuration from input file failed");
        retVal = RETURN_ERROR;
        goto cleanup;
    }

    PrintConfig(config);

    task = (Task *) object_create(Task);
    if (!task) {
        mcx_log(LOG_ERROR, "Could not create task settings");
        retVal = RETURN_ERROR;
        goto cleanup;
    }

    model = (Model *) object_create(Model);
    if (!model) {
        mcx_log(LOG_ERROR, "Could not create model");
        retVal = RETURN_ERROR;
        goto cleanup;
    }

    ComponentFactory * factory = object_create(ComponentFactory);

    task->SetConfig(task, config);
    model->SetConfig(model, config);
    model->SetTask(model, task);
    model->SetComponentFactory(model, factory);

    mcx_log(LOG_INFO, "******************** Read data: **************************************");
    mcx_log(LOG_INFO, " ");
    mcx_cpu_time_get(&clock_read_begin);
    mcx_time_get(&time_read_begin);

    mcx_signal_handler_set_function("TaskRead");
    retVal = task->Read(task, mcxInput->task);
    mcx_signal_handler_unset_function();
    if (RETURN_OK != retVal) {
        retVal = RETURN_ERROR;
        goto cleanup;
    }

    mcx_signal_handler_set_function("ModelRead");
    retVal = model->Read(model, mcxInput->model);
    mcx_signal_handler_unset_function();
    if (RETURN_OK != retVal) {
        retVal = RETURN_ERROR;
        goto cleanup;
    }

    object_destroy(mcxInput);
    mcx_signal_handler_set_function("ReaderCleanup");
    reader->Cleanup(reader);
    mcx_signal_handler_unset_function();
    object_destroy(reader);

    mcx_cpu_time_get(&clock_read_end);
    mcx_time_get(&time_read_end);
    mcx_log(LOG_INFO, "******************** Read done. **************************************");
    mcx_time_diff(&clock_read_begin, &clock_read_end, &cpu_time_used);
    cpu_time_sec = mcx_time_to_seconds(&cpu_time_used);
    mcx_log(LOG_INFO, "******************** Used CPU-Time:  %fs ***********************", cpu_time_sec);

    mcx_time_diff(&time_read_begin, &time_read_end, &time_diff);
    wall_time_sec = mcx_time_to_seconds(&time_diff);
    mcx_log(LOG_INFO, "******************** Used Wall-Time: %fs ***********************", wall_time_sec);
    mcx_log(LOG_INFO, " ");
    mcx_log(LOG_INFO, " ");

    mcx_log(LOG_INFO, "******************** Setup: ******************************************");
    mcx_log(LOG_INFO, " ");
    mcx_cpu_time_get(&clock_setup_begin);
    mcx_time_get(&time_setup_begin);

    mcx_signal_handler_set_function("TaskSetup");
    retVal = task->Setup(task, model);
    mcx_signal_handler_unset_function();
    if (RETURN_OK != retVal) {
        retVal = RETURN_ERROR;
        goto cleanup;
    }

    mcx_signal_handler_set_function("ModelSetup");
    retVal = model->Setup(model);
    mcx_signal_handler_unset_function();
    if (RETURN_OK != retVal) {
        retVal = RETURN_ERROR;
        goto cleanup;
    }

    mcx_signal_handler_set_function("TaskPrepareRun");
    retVal = task->PrepareRun(task, model);
    mcx_signal_handler_unset_function();
    if (RETURN_OK != retVal) {
        retVal = RETURN_ERROR;
        goto cleanup;
    }

    mcx_cpu_time_get(&clock_setup_end);
    mcx_time_get(&time_setup_end);
    mcx_log(LOG_INFO, "******************** Setup done. *************************************");
    mcx_time_diff(&clock_setup_begin, &clock_setup_end, &cpu_time_used);
    cpu_time_sec = mcx_time_to_seconds(&cpu_time_used);

    mcx_log(LOG_INFO, "******************** Used CPU-Time:  %fs ***********************", cpu_time_sec);
    mcx_time_diff(&time_setup_begin, &time_setup_end, &time_diff);
    wall_time_sec = mcx_time_to_seconds(&time_diff);
    mcx_log(LOG_INFO, "******************** Used Wall-Time: %fs ***********************", wall_time_sec);
    mcx_log(LOG_INFO, " ");
    mcx_log(LOG_INFO, " ");

    mcx_log(LOG_INFO, "******************** Initialization: *********************************");
    mcx_log(LOG_INFO, " ");
    mcx_cpu_time_get(&clock_init_begin);
    mcx_time_get(&time_init_begin);

    mcx_signal_handler_set_function("TaskInitialize");
    retVal = task->Initialize(task, model);
    mcx_signal_handler_unset_function();
    if (RETURN_OK != retVal) {
        retVal = RETURN_ERROR;
        goto cleanup;
    }

    mcx_cpu_time_get(&clock_init_end);
    mcx_time_get(&time_init_end);
    mcx_log(LOG_INFO, "******************** Initialization done. ****************************");
    mcx_time_diff(&clock_init_begin, &clock_init_end, &cpu_time_used);
    cpu_time_sec = mcx_time_to_seconds(&cpu_time_used);

    mcx_log(LOG_INFO, "******************** Used CPU-Time:  %fs ***********************", cpu_time_sec);

    mcx_time_diff(&time_init_begin, &time_init_end, &time_diff);
    wall_time_sec = mcx_time_to_seconds(&time_diff);
    mcx_log(LOG_INFO, "******************** Used Wall-Time: %fs ***********************", wall_time_sec);
    mcx_log(LOG_INFO, " ");
    mcx_log(LOG_INFO, " ");

    mcx_log(LOG_INFO, "******************** Simulation: *************************************");
    mcx_log(LOG_INFO, " ");
    mcx_cpu_time_get(&clock_sim_begin);
    mcx_time_get(&time_sim_begin);
    mcx_signal_handler_set_function("TaskRun");
    retVal = task->Run(task, model);
    mcx_signal_handler_unset_function();
    if (RETURN_OK != retVal) {
        retVal = RETURN_ERROR;
        goto cleanup;
    }
    mcx_cpu_time_get(&clock_sim_end);
    mcx_time_get(&time_sim_end);
    mcx_log(LOG_INFO, "******************** Simulation done. ********************************");
    mcx_time_diff(&clock_sim_begin, &clock_sim_end, &cpu_time_used);
    cpu_time_sec = mcx_time_to_seconds(&cpu_time_used);
    mcx_log(LOG_INFO, "******************** Used CPU-Time:  %fs ***********************", cpu_time_sec);

    mcx_time_diff(&time_sim_begin, &time_sim_end, &time_diff);
    wall_time_sec = mcx_time_to_seconds(&time_diff);
    mcx_log(LOG_INFO, "******************** Used Wall-Time: %fs ***********************", wall_time_sec);
    mcx_log(LOG_INFO, " ");
    mcx_log(LOG_INFO, " ");

    mcx_log(LOG_INFO, "******************** Summary: ****************************************");
    mcx_log(LOG_INFO, " ");
    model_stats(model);
    mcx_log(LOG_INFO, "**********************************************************************");
    mcx_log(LOG_INFO, " ");
    mcx_log(LOG_INFO, " ");


    mcx_log(LOG_INFO, "******************** Clean-up: ***************************************");
    mcx_log(LOG_INFO, " ");
    mcx_cpu_time_get(&clock_cleanup_begin);
    mcx_time_get(&time_cleanup_begin);

    mcx_signal_handler_set_function("RunMCX:Cleanup");
    object_destroy(task);
    object_destroy(model);
    object_destroy(config);

    mcx_cpu_time_get(&clock_cleanup_end);
    mcx_time_get(&time_cleanup_end);
    mcx_log(LOG_INFO, "******************** Clean-up done. **********************************");
    mcx_time_diff(&clock_cleanup_begin, &clock_cleanup_end, &cpu_time_used);
    cpu_time_sec = mcx_time_to_seconds(&cpu_time_used);
    mcx_log(LOG_INFO, "******************** Used CPU-Time:  %fs ***********************", cpu_time_sec);

    mcx_time_diff(&time_cleanup_begin, &time_cleanup_end, &time_diff);
    wall_time_sec = mcx_time_to_seconds(&time_diff);
    mcx_log(LOG_INFO, "******************** Used Wall-Time: %fs ***********************", wall_time_sec);
    mcx_log(LOG_INFO, " ");
    mcx_log(LOG_INFO, " ");

    mcx_time_diff(&clock_begin, &clock_cleanup_end, &cpu_time_used);
    cpu_time_sec = mcx_time_to_seconds(&cpu_time_used);

    mcx_time_diff(&time_begin, &time_cleanup_end, &time_diff);
    wall_time_sec = mcx_time_to_seconds(&time_diff);
    mcx_log(LOG_INFO, "******************** Statistics: *************************************");
    mcx_log(LOG_INFO, " ");

    mcx_log(LOG_INFO, "Program finished successfully");
    mcx_log(LOG_INFO, "Total used CPU-Time: %fs", cpu_time_sec);
    mcx_log(LOG_INFO, "Total used Wall-Time: %fs", wall_time_sec);
    mcx_log(LOG_INFO, " ");


cleanup:
    if (reader) {
        reader->Cleanup(reader);
        object_destroy(reader);
    }
    if (mcxInput) { object_destroy(mcxInput); }

    if (model) { object_destroy(model); }
    if (task) { object_destroy(task); }
    if (config) { object_destroy(config); }

    if (logInitialized) {
        mcx_log(LOG_INFO, "**********************************************************************");
    }

    mcx_signal_handler_unset_function(); // RunMCX:Cleanup

    return retVal;
}

int main(int argc, char *argv[])
{
    McxStatus retVal = RETURN_OK;

    char ** argList;
    int nArgs = 0;

    mcx_os_get_args(&argList, &nArgs);
    if (!argList) {
        argList = argv;
        nArgs = argc;
    }

#if defined (ENABLE_MT)
    mcx_mutex_create(&logMutex);
#endif //ENABLE_MT

    InitMemory();

    mcx_signal_handler_enable();
    retVal = RunMCX(nArgs, argList);
    mcx_signal_handler_disable();

    if (argList != argv) {
        int i = 0;
        for (i = 0; i < nArgs; i++) {
            mcx_free(argList[i]);
        }
        mcx_free(argList);
    }

    CleanupLogFile();

    return (RETURN_ERROR == retVal);
}

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */