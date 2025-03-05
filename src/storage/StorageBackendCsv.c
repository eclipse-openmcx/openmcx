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

#include "core/channels/ChannelInfo.h"
#include "storage/StorageBackendCsv.h"
#include "storage/StorageBackendText_impl.h"
#include "storage/PPD.h"
#include "storage/ChannelStorage.h"
#include "core/connections/ConnectionInfoFactory.h"

#include "util/string.h"
#include "util/os.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define TOOL_NAME "OpenMCx"
#define TOOL_VERSION ""
typedef struct StorageBackendCsv {
    StorageBackendText _;
} StorageBackendCsv;

static char * LocalPath(const char * filename) {
    char * path = (char *) mcx_calloc(strlen(filename) + 3, sizeof(char));
    if (!path) { return NULL; }

    strcpy(path, "./");
    strcat(path, filename);

    return path;
}

static McxStatus WritePPDFileCsv(StorageBackend * backend) {
    StorageBackendText * textBackend = (StorageBackendText *) backend;
    ResultsStorage * storage = backend->storage;
    char * filename = NULL;
    size_t compIdx = 0;

    PpdRootFolder * root = NULL;

    McxStatus retVal = RETURN_OK;

    filename = (char *) mcx_calloc(strlen(textBackend->path) + strlen("results.ppd") + 1 + 1, sizeof(char));
    if (!filename) {
        mcx_log(LOG_ERROR, "Results: Memory allocation for filename of result file \"%s/%s\" failed", textBackend->path, "results.ppd");
        return RETURN_ERROR;
    }
    sprintf(filename, "%s/%s", textBackend->path, "results.ppd");

    root = PpdRootFolderMake(
        TOOL_NAME " Simulation Results",
        TOOL_NAME " Simulation Results",
        TOOL_NAME ,
        TOOL_VERSION);

    for (compIdx = 0; compIdx < textBackend->numComponents; compIdx++) {
        ComponentStorage * compStore = storage->componentStorage[compIdx];
        Component * comp = (Component *) compStore->comp;
        TextComponent * textComponent = &(textBackend->comps[compIdx]);
        const char * compName = comp->GetName(comp);

        TextFile * outFile = &(textComponent->files[CHANNEL_STORE_OUT]);
        TextFile * inFile  = &(textComponent->files[CHANNEL_STORE_IN]);
        TextFile * localFile  = &(textComponent->files[CHANNEL_STORE_LOCAL]);
        TextFile * rtfactorFile = &(textComponent->files[CHANNEL_STORE_RTFACTOR]);

        /*
         * out/inFile->name do not include directory, add "./"
         */
        char * outPath      = outFile->name      ? LocalPath(outFile->name)      : NULL;
        char * inPath       = inFile->name       ? LocalPath(inFile->name)       : NULL;
        char * localPath    = localFile->name    ? LocalPath(localFile->name)    : NULL;
        char * rtfactorPath = rtfactorFile->name ? LocalPath(rtfactorFile->name) : NULL;

        PpdFolder * folder = (PpdFolder *) root;

        PpdFolder *  compFolder = NULL;
        PpdLink *    link       = NULL;

        if ((outFile->name && !outPath)
            || (inFile->name && !inPath)
            || (localFile->name && !localPath)
            || (rtfactorFile->name && !rtfactorPath)) {
            mcx_log(LOG_ERROR, "Results: Could not setup directory file");
            return RETURN_ERROR;
        }

        compFolder = folder->InsertComponent(
            folder,
            "CSVInfo",
            compName,
            outPath,
            inPath,
            localPath,
            rtfactorPath);
        if (!compFolder) {
            mcx_log(LOG_ERROR, "Results: Could not add directory file");
            return RETURN_ERROR;
        }

        if (comp->GetPPDLink) {
            link = comp->GetPPDLink(comp);
            if (!link) {
                mcx_log(LOG_ERROR, "Results: Could not setup detailed results");
                return RETURN_ERROR;
            }
            retVal = compFolder->AddElement(compFolder, (PpdElement *)link);
            if (RETURN_OK != retVal) {
                mcx_log(LOG_ERROR, "Results: Could not add detailed results");
                return RETURN_ERROR;
            }
        }

        mcx_free(outPath);
        mcx_free(inPath);
        if (localPath) {
            mcx_free(localPath);
        }
        if (rtfactorPath) {
            mcx_free(rtfactorPath);
        }
    }

    retVal = root->Print(root, filename);

    mcx_free(filename);
    object_destroy(root);

    if (RETURN_OK != retVal) { return RETURN_ERROR; }

    return RETURN_OK;
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

    newStr[i] = '\0';  // to be safe if mcx_calloc did not set memory to zero

    return newStr;
}

static McxStatus SetupComponentFilesCsv(StorageBackend * backend) {
    StorageBackendText * textBackend = (StorageBackendText *) backend;
    ResultsStorage * storage = backend->storage;
    size_t compIdx = 0;
    size_t numCompsOld = textBackend->numComponents;

    MCX_DEBUG_LOG("Setting up component CSV files (%zu -> %zu)", numCompsOld, storage->numComponents);
    textBackend->numComponents = storage->numComponents;
    textBackend->comps = (TextComponent *) mcx_realloc(textBackend->comps, textBackend->numComponents * sizeof(TextComponent));
    if (textBackend->numComponents && !textBackend->comps) {
        mcx_log(LOG_ERROR, "SetupComponentFilesCsv: TextComponents: Not enough memory");
        return RETURN_ERROR;
    }

    // initialize new TextComponents to 0
    memset(textBackend->comps + numCompsOld, 0, (textBackend->numComponents - numCompsOld) * sizeof(TextComponent));

    for (compIdx = numCompsOld; compIdx < textBackend->numComponents; compIdx++) {
        ComponentStorage * compStore = storage->componentStorage[compIdx];
        Component * comp = (Component *) compStore->comp;
        TextComponent * textComponent = &(textBackend->comps[compIdx]);
        size_t j = 0;

        char * localName = mcx_string_encode_filename(comp->GetName(comp));
        size_t numberOfPorts[CHANNEL_STORE_NUM] = { 0 };  //avoid empty result files
        numberOfPorts[CHANNEL_STORE_IN] = comp->GetNumWriteInChannels(comp);
        numberOfPorts[CHANNEL_STORE_OUT] = comp->GetNumWriteOutChannels(comp);
        numberOfPorts[CHANNEL_STORE_LOCAL] = comp->GetNumWriteLocalChannels(comp);
        numberOfPorts[CHANNEL_STORE_RTFACTOR] = comp->GetNumWriteRTFactorChannels(comp);

        for (j = 0; j < CHANNEL_STORE_NUM; j++) {
            TextFile * textFile = &(textComponent->files[j]);
            ChannelStorage * chStore = compStore->channels[j];
            size_t chNum = chStore->GetChannelNum(chStore);
            size_t chIdx = 0;
            char * buffer = NULL;
            const size_t nameLen = strlen(localName) + strlen(ChannelStoreSuffix[j]) + 6;

            /* do not create a file if channel store is not enabled */
            if (!compStore->storage->channelStoreEnabled[j]) {
                continue;
            }

            /* do not create a file if no channel (beside the time) is present */
            if (numberOfPorts[j] < 1) {
                continue;
            }

            textFile->name = (char *) mcx_calloc(nameLen, sizeof(char));
            snprintf(textFile->name, nameLen, "%s_%s.csv", localName, ChannelStoreSuffix[j]);

            buffer = (char *) mcx_calloc(strlen(textBackend->path) + strlen(textFile->name) + 1 + 1, sizeof(char));
            if (!buffer) {
                mcx_log(LOG_ERROR, "Results: Memory allocation for result file name \"%s/%s\" failed", textBackend->path, textFile->name);
                mcx_free(localName);
                return RETURN_ERROR;
            }
            sprintf(buffer, "%s/%s", textBackend->path, textFile->name);
            textFile->fp = mcx_os_fopen(buffer, "w");
            if (!textFile->fp) {
                mcx_log(LOG_ERROR, "Results: Could not open result file \"%s\" for writing", buffer);
                mcx_free(buffer);
                mcx_free(localName);
                return RETURN_ERROR;
            }
            mcx_free(buffer);

            mcx_os_fprintf(textFile->fp, "sep=%s\n", textBackend->separator);

            for (chIdx = 0; chIdx < chNum; chIdx++) {
                ChannelInfo * info = chStore->GetChannelInfo(chStore, chIdx);
                const char * channelName = ChannelInfoGetName(info);
                ChannelType * type = info->type;

                if (ChannelTypeIsArray(type)) {
                    if (type->ty.a.numDims > 1) {
                        // multi-dim not yet supported
                        continue;
                    }
                    ChannelDimension * dimension = info->dimension;
                    size_t array_start = dimension->startIdxs[0];
                    size_t array_end = dimension->endIdxs[0];

                    size_t array_idx = 0;

                    char ** names = ExpandedChannelNames(channelName, array_start, array_end);
                    const char * sep = textBackend->separator;
                    while (names[array_idx]) {
                        char * quotedChannelName = QuoteString(names[array_idx]);

                        if(quotedChannelName){
                            mcx_os_fprintf(textFile->fp, "%s\"%s\"", sep, quotedChannelName);
                            mcx_free(quotedChannelName);
                        } else {
                            mcx_os_fprintf(textFile->fp, "%s", sep);
                        }
                        ++array_idx;
                    }
                    FreeExpandedChannelNames(names);
                } else {
                    char * quotedChannelName = QuoteString(channelName);
                    const char * sep = textBackend->separator;

                    if (chIdx == 0) {
                        sep = "";
                    }
                    if(quotedChannelName){
                        mcx_os_fprintf(textFile->fp, "%s\"%s\"", sep, quotedChannelName);
                        mcx_free(quotedChannelName);
                    } else {
                        mcx_os_fprintf(textFile->fp, "%s", sep);
                    }
                }
            }
            mcx_os_fprintf(textFile->fp, "\n");

            for (chIdx = 0; chIdx < chNum; chIdx++) {
                ChannelInfo * info = chStore->GetChannelInfo(chStore, chIdx);
                ChannelType * type = info->type;
                const char * channelUnit = info->unitString;

                if (ChannelTypeIsArray(type)) {
                    if (type->ty.a.numDims > 1) {
                        // multi-dim not yet supported
                        continue;
                    }
                    ChannelDimension * dimension = info->dimension;
                    size_t array_start = dimension->startIdxs[0];
                    size_t array_end = dimension->endIdxs[0];

                    size_t array_idx = 0;

                    const char * sep = textBackend->separator;
                    for (array_idx = array_start; array_idx <= array_end; array_idx++) {
                        mcx_os_fprintf(textFile->fp, "%s%s", sep, channelUnit);
                    }
                } else {
                    const char * sep = textBackend->separator;
                    if (chIdx == 0) {
                        sep = "";
                    }
                    mcx_os_fprintf(textFile->fp, "%s%s", sep, channelUnit);
                }
            }
            mcx_os_fprintf(textFile->fp, "\n");
        }
        mcx_free(localName);
    }

    return RETURN_OK;
}


static void StorageBackendCsvDestructor(StorageBackendCsv * csvBackend) {
}

static StorageBackendCsv * StorageBackendCsvCreate(StorageBackendCsv * csvBackend) {
    StorageBackendText * textBackend = (StorageBackendText *) csvBackend;

    textBackend->separator = ",";
    textBackend->WritePPDFile = WritePPDFileCsv;
    textBackend->SetupComponentFiles = SetupComponentFilesCsv;

    return csvBackend;
}

OBJECT_CLASS(StorageBackendCsv, StorageBackendText);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif //ENABLE_STORAGE