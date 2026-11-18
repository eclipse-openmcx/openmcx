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

#if defined (ENABLE_STORAGE)

#include "storage/StorageBackendText_impl.h"

#include "util/paths.h"
#include "util/os.h"

#include <locale.h>     /* struct lconv, setlocale, localeconv */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


//declare storage functions
static McxStatus StoreChannelValues(StorageBackend * backend, ChannelStoreType chType, size_t comp, ChannelValue * values, size_t num);
static McxStatus Store(StorageBackend * backend, ChannelStoreType chType, size_t comp, size_t row);
static McxStatus Finished(StorageBackend * backend);
static McxStatus StoreFull(StorageBackend * backend, ChannelStoreType chType, size_t comp, size_t row);
static McxStatus FinishedFull(StorageBackend * backend);

static McxStatus FlushTextFile(TextFile * textFile) {
    if (fflush(textFile->fp)) {
        int errsv = errno;
        mcx_log(LOG_DEBUG, "Results: Flushing result file \"%s\"!", textFile->name);
        if (ENOSPC == errsv) {
            mcx_log(LOG_ERROR, "Results: Could not write results to file \"%s\": No space left on device", textFile->name);
        } else {
            mcx_log(LOG_ERROR, "Results: Could not write results to file \"%s\"", textFile->name);
        }
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

static McxStatus CloseTextFiles(StorageBackendText * backend, size_t comp) {
    size_t i = 0;
    McxStatus retVal = RETURN_OK;
    if (comp >= backend->numComponents) {
        mcx_log(LOG_ERROR, "Results: Could not close result file: No result file for element %d", comp);
        return RETURN_ERROR;
    }
    for (i = 0; i < CHANNEL_STORE_NUM; i++) {
        TextFile * textFile = &(backend->comps[comp].files[i]);
        if (textFile && textFile->fp) {
            mcx_log(LOG_DEBUG, "Results: Closing result file \"%s\"!", textFile->name);
            if (mcx_os_fclose(textFile->fp)) {
                int errsv = errno;
                if (ENOSPC == errsv) {
                    mcx_log(LOG_ERROR, "Results: Could not close result file \"%s\": No space left on device", textFile->name);
                } else {
                    mcx_log(LOG_ERROR, "Results: Could not close result file \"%s\"", textFile->name);
                }
                retVal = RETURN_ERROR;
            }
        }
        textFile->fp = NULL;
    }
    return RETURN_OK;
}

static McxStatus Configure(StorageBackend * backend, ResultsStorage * storage, const char * path, int flushEveryStore, int storeAtRuntime) {
    StorageBackendText * textBackend = (StorageBackendText *) backend;

    if (NULL == textBackend->WritePPDFile) {
        mcx_log(LOG_ERROR, "Results: Configure: Virtual function StorageBackendText::WritePPDFile needs to be implemented");
        return RETURN_ERROR;
    }
    if (NULL == textBackend->SetupComponentFiles) {
        mcx_log(LOG_ERROR, "Results: Configure: Virtual function StorageBackendText::SetupComponentFiles needs to be implemented");
        return RETURN_ERROR;
    }

    textBackend->flushEveryStore = flushEveryStore;
    if (FALSE == storeAtRuntime) {
        backend->needsFullStorage = 1;
        backend->Store = StoreFull;
        backend->Finished = FinishedFull;
    } else {
        backend->needsFullStorage = 0;
        backend->Store = Store;
        backend->StoreChannelValues = StoreChannelValues;
        backend->Finished = Finished;
    }

    backend->storage = storage;

    textBackend->path = (char *) mcx_calloc(strlen(path) + 1, sizeof(char));
    if(!textBackend->path) {
        mcx_log(LOG_ERROR, "Results: Configure: No path given");
        return RETURN_ERROR;
    }
    strcpy(textBackend->path, path);

    return RETURN_OK;
}


static McxStatus Setup(StorageBackend * backend) {
    StorageBackendText * textBackend = (StorageBackendText *) backend;
    char buffer[SIZE];
    struct stat sb;
    int resVal = 0;
    int n = 0;
    McxStatus retVal = RETURN_OK;

    if (!textBackend->path) {
        mcx_log(LOG_ERROR, "Results: No path given");
        return RETURN_ERROR;
    }

    {
        struct lconv * lc;
        mcx_log(LOG_DEBUG, "Locale (LC_NUMERIC) is: %s", setlocale(LC_NUMERIC, NULL));
        setlocale(LC_NUMERIC, "C");

        // avoid using same symbol for decimal point and column separator
        lc = localeconv();
        if (0 == strcmp(textBackend->separator, lc->decimal_point)) {
            if (strcmp(";", lc->decimal_point)) {
                textBackend->separator = ";";
            } else {
                textBackend->separator = " ";  // separator is already ";"
            }
        }
    }

    // make directory
    n = snprintf(buffer, SIZE, "%s", textBackend->path);

    resVal = stat(buffer, &sb);
    if (resVal != 0) {
        mcx_path_to_platform(buffer);
        resVal = mcx_os_mkdir_recursive(buffer);
        if (RETURN_ERROR == resVal) {
            mcx_log(LOG_ERROR, "Results: Could not make results directory \"%s\"", buffer);
            return RETURN_ERROR;
        }
    }

    retVal = textBackend->SetupComponentFiles(backend);
    if (RETURN_ERROR == retVal) {
        return RETURN_ERROR;
    }

    return textBackend->WritePPDFile(backend);
}

static char * QuoteString(const char * _str) {
    char * str = (char *)_str;
    char * newStr = NULL;

    size_t i = 0;

    if (!str) {
        return NULL;
    }

    newStr = (char *) mcx_calloc(strlen(str) * 2 + 1, sizeof(char));

    if (!newStr) {
        return NULL;
    }

    /* copy _str quoting '"':
     *
     * From https://tools.ietf.org/html/rfc4180#section-2
     *
     *    7.  If double-quotes are used to enclose fields, then a double-quote
     *        appearing inside a field must be escaped by preceding it with
     *        another double quote.  For example:
     *
     *    "aaa","b""bb","ccc"
     */
    while (*str) {
        if (*str == '\"') {
            newStr[i++] = '\"';
            newStr[i++] = '\"';
        } else {
            newStr[i++] = *str;
        }
        str++;
    }

    return newStr;
}

static McxStatus WriteRow(FILE * file, ChannelValue * values, size_t numChannels, const char * separator) {
    size_t channel = 0;
    char staticBuffer[32];
    int storedLen = 0;

    if (!file) {
        mcx_log(LOG_ERROR, "Results: null-file pointer!");
        return RETURN_ERROR;
    }

    for (channel = 0; channel < numChannels; channel++) {
        McxStatus retVal = RETURN_OK;
        ChannelValue val = values[channel];
        const char * sep = separator;
        if (channel == 0) { // leave out separator at the beginning
            sep = "";
        }
        switch (ChannelValueType(&val)) {
        case CHANNEL_DOUBLE:
        case CHANNEL_INTEGER:
        case CHANNEL_BOOL:
            /* fixed length values */

            retVal = ChannelValueToStringBuffer(&val, staticBuffer, 32);
            if (RETURN_OK == retVal) {
                mcx_os_fprintf(file, "%s%s", sep, staticBuffer);
            } else {
                mcx_os_fprintf(file, "%s\"\"", sep);
            }
            break;
        case CHANNEL_BINARY:
        case CHANNEL_BINARY_REFERENCE:
        case CHANNEL_STRING: {
            /* variable length values */

            char * str = ChannelValueToString(&val);
            char * quotedStr = QuoteString(str);

            if (quotedStr) {
                mcx_os_fprintf(file, "%s\"%s\"", sep, quotedStr);
                mcx_free(quotedStr);
                mcx_free(str);
            } else {
                mcx_os_fprintf(file, "%s", sep);
            }
            break;
        }
        default:
            mcx_os_fprintf(file, "%s\"\"", sep);
            break;
        }
    }
    storedLen = mcx_os_fprintf(file, "\n");

    if (storedLen <= 0) {
        mcx_log(LOG_ERROR, "Results: fprintf failed!");
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

static McxStatus StoreChannelValues(StorageBackend * backend, ChannelStoreType chType, size_t comp, ChannelValue * values, size_t num) {
    StorageBackendText * textBackend = (StorageBackendText *) backend;
    ResultsStorage * storage = backend->storage;
    ComponentStorage * compStore = storage->componentStorage[comp];
    TextFile * textFile = NULL;
    McxStatus retVal;

    if (comp >= textBackend->numComponents) {
        mcx_log(LOG_ERROR, "Results: No result file for element %d", comp);
        return RETURN_ERROR;
    }

    textFile = &(textBackend->comps[comp].files[chType]);

    retVal = WriteRow(textFile->fp, values, num, textBackend->separator);
    if (RETURN_OK != retVal) {
        mcx_log(LOG_ERROR, "Results: Could not write result row for \"%s\"", textFile->name);
        return RETURN_ERROR;
    }
    if (textBackend->flushEveryStore) {
        if (RETURN_ERROR == FlushTextFile(textFile)) {
            return RETURN_ERROR;
        }
    }
    return RETURN_OK;

}


static McxStatus Store(StorageBackend * backend, ChannelStoreType chType, size_t comp, size_t row) {
    ResultsStorage * storage = backend->storage;
    ComponentStorage * compStore = storage->componentStorage[comp];
    ChannelStorage * chStore = compStore->channels[chType];

    return backend->StoreChannelValues(backend, chType, comp, chStore->GetValuesAtRow(chStore, row), chStore->GetChannelNum(chStore));
}


static McxStatus Finished(StorageBackend * backend) {
    StorageBackendText * textBackend = (StorageBackendText *) backend;

    size_t i = 0;
    McxStatus retVal = RETURN_OK;
    McxStatus finishedStatus = RETURN_OK;

    for (i = 0; i < textBackend->numComponents; i++) {
        if (RETURN_ERROR == CloseTextFiles(textBackend, i)) {
            finishedStatus = RETURN_ERROR;
        }
    }

    // re-write the ppd file because the offset might have changed
    mcx_log(LOG_INFO, "Re-writing the result.ppd file");
    retVal = textBackend->WritePPDFile(backend);
    if (RETURN_ERROR == retVal) {
        mcx_log(LOG_ERROR, "Re-writing the result.ppd file");
        return RETURN_ERROR;
    }

    return finishedStatus;
}

static McxStatus StoreFull(StorageBackend * backend, ChannelStoreType chType, size_t comp, size_t row) {
    UNUSED(backend);
    UNUSED(chType);
    UNUSED(comp);
    UNUSED(row);

    return RETURN_OK;
}

static McxStatus FinishedFull(StorageBackend * backend) {
    StorageBackendText * textBackend = (StorageBackendText *) backend;
    ResultsStorage * storage = backend->storage;
    size_t compIdx = 0, chIdx = 0, chType = 0;
    McxStatus finishedStatus = RETURN_OK;
    McxStatus retVal = RETURN_OK;

    for (compIdx = 0; compIdx < textBackend->numComponents; compIdx++) {
        ComponentStorage * compStore = storage->componentStorage[compIdx];
        for (chType = 0; chType < CHANNEL_STORE_NUM; chType++) {
            TextFile * textFile = &(textBackend->comps[compIdx].files[chType]);
            ChannelStorage * chStore = compStore->channels[chType];

            if (storage->channelStoreEnabled[chType] && textFile) {
                for (chIdx = 0; chIdx < chStore->Length(chStore); chIdx++) {
                    McxStatus retVal = WriteRow(textFile->fp, chStore->GetValuesAtRow(chStore, chIdx), chStore->GetChannelNum(chStore), textBackend->separator);
                    if (RETURN_OK != retVal) {
                        mcx_log(LOG_ERROR, "Results: Could not write result row for %s", textFile->name);
                        finishedStatus = RETURN_ERROR;
                        break;
                    }
                }
            }
        }

        if (RETURN_ERROR == CloseTextFiles(textBackend, compIdx)) {
            finishedStatus = RETURN_ERROR;
        }
    }

    // re-write the ppd file because the offset might have changed
    mcx_log(LOG_INFO, "Re-writing the result.ppd file");
    retVal = textBackend->WritePPDFile(backend);
    if (RETURN_ERROR == retVal) {
        mcx_log(LOG_ERROR, "Re-writing the result.ppd failed failed");
        return RETURN_ERROR;
    }

    return finishedStatus;
}


static void StorageBackendTextDestructor(StorageBackendText * textBackend) {
    if (NULL != textBackend->comps) {
        size_t compIdx = 0, chType = 0;
        for (compIdx = 0; compIdx < textBackend->numComponents; compIdx++) {
            for (chType = 0; chType < CHANNEL_STORE_NUM; chType++) {
                char * name = textBackend->comps[compIdx].files[chType].name;
                if (name) {
                    mcx_free(name);
                }
            }
        }
        mcx_free(textBackend->comps);
    }

    if (NULL != textBackend->path) {
        mcx_free(textBackend->path);
        textBackend->path = NULL;
    }
}


static StorageBackendText * StorageBackendTextCreate(StorageBackendText * textBackend) {
    StorageBackend * backend = (StorageBackend *) textBackend;

    textBackend->path = NULL;
    textBackend->comps = NULL;
    textBackend->numComponents = 0;
    textBackend->flushEveryStore = FALSE;

    // map to local functions
    backend->Configure = Configure;
    backend->Setup = Setup;

    backend->needsFullStorage = FALSE;

    textBackend->separator = "";
    textBackend->WritePPDFile = NULL;
    textBackend->SetupComponentFiles = NULL;

    backend->Store = NULL;     // will be set in configure
    backend->Finished = NULL;  // will be set in configure

    return textBackend;
}

OBJECT_CLASS(StorageBackendText, StorageBackend);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif //ENABLE_STORAGE