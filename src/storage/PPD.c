/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "storage/PPD.h"
#include "util/string.h"
#include "util/os.h"
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

static const int INDENT = 4;

static int PpdElement_fprintf(FILE * stream, int level, const char * fmt, ...) {
    int ret;
    va_list args;

    size_t i = 0;

    fprintf(stream, "%*.s", level * INDENT, "");

    va_start(args, fmt);
    ret = vfprintf(stream, fmt, args);
    va_end(args);

    fprintf(stream, "%s", "\n");

    return ret;
}


static void PpdElementDestructor(PpdElement * element) {

}

static PpdElement * PpdElementCreate(PpdElement * element) {
    element->GetName = NULL;
    element->PrintIndented = NULL;

    return element;
}

OBJECT_CLASS(PpdElement, Object);


static const char * PpdTypeToString(PpdType type) {
    switch (type) {
    case PPD_ELEMENT:
        return "element";
    case PPD_SUBSYSTEM:
        return "subsystem";
    case PPD_NO_TYPE:
        return "";
    default:
        return "";
    }
}

static const char * PpdIconToString(PpdIcon icon) {
    switch (icon) {
    case PPD_ICON_INPORT:
        return "ai-mc-inport";
    case PPD_NO_ICON:
        return "";
    default:
        return "";
    }
}

static McxStatus PpdFolderPrintIndented(PpdElement * element, int level, FILE * stream) {
    PpdFolder * folder = (PpdFolder *) element;

    ObjectContainer * elements = folder->elements;

    size_t i = 0;

    PpdElement_fprintf(stream, level,     "FOLDER \"%s\" \"%s\"", folder->id, folder->name);

    if (PPD_NO_TYPE != folder->type) {
        PpdElement_fprintf(stream, level + 1,   "type = %s", PpdTypeToString(folder->type));
    }

    if (PPD_NO_ICON != folder->icon) {
        PpdElement_fprintf(stream, level + 1,   "icon = \"%s\"", PpdIconToString(folder->icon));
    }

    for (i = 0; i < elements->Size(elements); i++) {
        PpdElement * element = (PpdElement *) elements->At(elements, i);

        element->PrintIndented(element, level + 1, stream);
    }

    PpdElement_fprintf(stream, level,     "..");

    return RETURN_OK;
}

static char * PpdFolderEncodeNameString(PpdFolder * folder, const char * str) {
    return mcx_string_encode(str, '%', "\"%");
}

static McxStatus PpdFolderSetup(PpdFolder * folder, const char * id, const char * name) {
    if (!(folder->id   = folder->EncodeNameString(folder, id)))   {
        mcx_log(LOG_ERROR, "Results: Could not encode folder id %s", id);
        return RETURN_ERROR;
    }
    if (!(folder->name = folder->EncodeNameString(folder, name))) {
        mcx_log(LOG_ERROR, "Results: Could not encode folder name %s", name);
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

static McxStatus PpdFolderSetType(PpdFolder * folder, PpdType type) {
    if (PPD_NO_TYPE != folder->type) {
        if (folder->name) {
            mcx_log(LOG_ERROR, "Results: Type of folder %s already set", folder->name);
        }
        else {
            mcx_log(LOG_ERROR, "Results: Type of folder already set");
        }
        return RETURN_ERROR;
    }

    folder->type = type;

    return RETURN_OK;
}

static McxStatus PpdFolderSetIcon(PpdFolder * folder, PpdIcon icon) {
    if (PPD_NO_ICON != folder->icon) {
        if (folder->name) {
            mcx_log(LOG_ERROR, "Results: Icon of folder %s already set", folder->name);
        }
        else {
            mcx_log(LOG_ERROR, "Results: Icon of folder already set");
        }
        return RETURN_ERROR;
    }

    folder->icon = icon;

    return RETURN_OK;
}

static McxStatus PpdFolderAddElement(PpdFolder * folder, PpdElement * element) {
    ObjectContainer * elements = folder->elements;

    McxStatus retVal = RETURN_OK;

    retVal =  elements->PushBack(elements, (Object *) element);
    if (RETURN_OK != retVal) {
        if (folder->name) {
            mcx_log(LOG_ERROR, "Results: Could not add element to folder %s", folder->name);
        }
        else {
            mcx_log(LOG_ERROR, "Results: Could not add element to folder");
        }
        return RETURN_ERROR;
    }

    return elements->SetElementName(elements, elements->Size(elements) - 1, element->GetName(element));
}

static char PPD_SEP = '#';

static PpdFolder * PpdFolderInsertComponent(PpdFolder * folder, const char * linkType, const char * compName, const char * fileNameOut, const char * fileNameIn, const char * fileNameLocal, const char * fileNameRTFactor) {
    const char * sep = strchr(compName, PPD_SEP);

    if (!sep) {
        PpdFolder * compFolder   = PpdFolderMake(compName, compName);
        PpdFolder * compFolderIn = PpdFolderMake("In", "In");

        PpdLink * compLink         = fileNameOut      ? PpdLinkMake(linkType, fileNameOut)      : NULL;
        PpdLink * compLinkIn       = fileNameIn       ? PpdLinkMake(linkType, fileNameIn)       : NULL;
        PpdLink * compLinkLocal    = fileNameLocal    ? PpdLinkMake(linkType, fileNameLocal)    : NULL;
        PpdLink * compLinkRTFactor = fileNameRTFactor ? PpdLinkMake(linkType, fileNameRTFactor) : NULL;

        McxStatus retVal = RETURN_OK;

        if (!compFolder
            || !compFolderIn
            || (fileNameOut && !compLink)
            || (fileNameIn && !compLinkIn)) {
            mcx_log(LOG_ERROR, "Results: Could not setup folder structure");
            return NULL;
        }

        retVal = compFolder->SetType(compFolder, PPD_ELEMENT);
        if (RETURN_OK != retVal) { return NULL; }


        /*
         * Create the structure:
         * - FOLDER Comp
         *   - LINK file_ref.csv
         *   - LINK file_internal.csv
         *   - FOLDER In
         *     - LINK file_in.csv
         */

        if (compLink) {
            retVal = compFolder->AddElement(compFolder, (PpdElement *) compLink);
            if (RETURN_OK != retVal) { return NULL; }
        }

        if (compLinkLocal) {
            retVal = compFolder->AddElement(compFolder, (PpdElement *) compLinkLocal);
            if (RETURN_OK != retVal) { return NULL; }
        }

        if (compLinkRTFactor) {
            retVal = compFolder->AddElement(compFolder, (PpdElement *) compLinkRTFactor);
            if (RETURN_OK != retVal) { return NULL; }
        }

        if (compLinkIn) {
            retVal = compFolderIn->SetIcon(compFolderIn, PPD_ICON_INPORT);
            if (RETURN_OK != retVal) { return NULL; }

            retVal = compFolderIn->AddElement(compFolderIn, (PpdElement *) compLinkIn);
            if (RETURN_OK != retVal) { return NULL; }

            retVal = compFolder->AddElement(compFolder, (PpdElement *) compFolderIn);
            if (RETURN_OK != retVal) { return NULL; }
        } else {
            object_destroy(compFolderIn);
        }

        /*
         * Add new component folder into parent
         */
        retVal = folder->AddElement(folder, (PpdElement *) compFolder);
        if (RETURN_OK != retVal) { return NULL; }

        return compFolder;
    } else {
        char * folderName = NULL;
        PpdElement * element = NULL;
        PpdFolder * compFolder = NULL;

        McxStatus retVal = RETURN_OK;

        /*
         * Get name of first subsystem
         */
        folderName = (char *) mcx_calloc(sep - compName + 1, sizeof(char));
        if (!folderName) { return NULL; }
        strncpy(folderName, compName, sep - compName);

        /*
         * Get the folder for the first subsystem if it exists, otherwise create one
         */
        element = folder->GetElementByName(folder, folderName);
        if (object_same_type(PpdFolder, element)) {
            compFolder = (PpdFolder *) element;
        }
        if (!compFolder) {
            compFolder = PpdFolderMake(folderName, folderName);
            if (!compFolder) { return NULL; }

            retVal = compFolder->SetType(compFolder, PPD_SUBSYSTEM);
            if (RETURN_OK != retVal) { return NULL; }

            /*
             * Add to parent
             */
            retVal = folder->AddElement(folder, (PpdElement *) compFolder);
            if (RETURN_OK != retVal) { return NULL; }
        }

        mcx_free(folderName);

        /*
         * Insert component into subfolder recursively
         */
        return compFolder->InsertComponent(compFolder, linkType, sep + 1, fileNameOut, fileNameIn, fileNameLocal, fileNameRTFactor);
    }
}

static PpdElement * PpdFolderGetElementByName(PpdFolder * folder, const char * name) {
    ObjectContainer * elements = folder->elements;

    size_t pos = elements->GetNameIndex(elements, name);

    return (PpdElement *) elements->At(elements, pos);
}

static const char * PpdFolderGetName(PpdElement * element) {
    PpdFolder * folder = (PpdFolder *) element;

    return folder->name;
}

static void PpdFolderDestructor(PpdFolder * folder) {
    ObjectContainer * elements = folder->elements;

    mcx_free(folder->id);
    mcx_free(folder->name);

    elements->DestroyObjects(elements);
    object_destroy(elements);
}

static PpdFolder * PpdFolderCreate(PpdFolder * folder) {
    PpdElement * element = (PpdElement *) folder;

    element->GetName       = PpdFolderGetName;
    element->PrintIndented = PpdFolderPrintIndented;

    folder->Setup           = PpdFolderSetup;
    folder->AddElement      = PpdFolderAddElement;
    folder->InsertComponent = PpdFolderInsertComponent;

    folder->EncodeNameString = PpdFolderEncodeNameString;
    folder->GetElementByName = PpdFolderGetElementByName;

    folder->SetType = PpdFolderSetType;
    folder->SetIcon = PpdFolderSetIcon;


    folder->type = PPD_NO_TYPE;
    folder->icon = PPD_NO_ICON;

    folder->id   = NULL;
    folder->name = NULL;

    if (!(folder->elements = (ObjectContainer *) object_create(ObjectContainer))) { return NULL; }

    return folder;
}

OBJECT_CLASS(PpdFolder, PpdElement);


static McxStatus PpdRootFolderPrintIndented(PpdElement * element, int level, FILE * stream) {
    PpdFolder * folder = (PpdFolder *) element;
    PpdRootFolder * root = (PpdRootFolder *) element;

    ObjectContainer * elements = folder->elements;

    size_t i = 0;

    PpdElement_fprintf(stream, level,     "FOLDER \"%s\" \"%s\"", folder->id, folder->name);
    PpdElement_fprintf(stream, level + 1,   "client = \"%s\"", root->client);
    PpdElement_fprintf(stream, level + 1,   "version = \"%s\"", root->version);

    for (i = 0; i < elements->Size(elements); i++) {
        PpdElement * element = (PpdElement *) elements->At(elements, i);

        element->PrintIndented(element, level + 1, stream);
    }

    PpdElement_fprintf(stream, level,     "..");

    return RETURN_OK;
}


static McxStatus PpdRootFolderSetup(PpdRootFolder * root, const char * id, const char * name, const char * client, const char * version) {
    PpdFolder * folder = (PpdFolder *) root;

    if (RETURN_OK != folder->Setup(folder, id, name))      { return RETURN_ERROR; }
    if (RETURN_OK != folder->SetType(folder, PPD_ELEMENT)) { return RETURN_ERROR; }

    if (!(root->client  = mcx_string_copy(client)))  { return RETURN_ERROR; }
    if (!(root->version = mcx_string_copy(version))) { return RETURN_ERROR; }

    return RETURN_OK;
}

static McxStatus PpdRootFolderPrint(PpdRootFolder * root, const char * filename) {
    PpdElement * element = (PpdElement *) root;

    FILE * file = mcx_os_fopen(filename, "w");

    if (!file) {
        mcx_log(LOG_ERROR, "Results: Could not open ppd file \"%s\" for writing", filename);
        return RETURN_ERROR;
    }

    element->PrintIndented(element, 0, file);

    if (mcx_os_fclose(file)) {
        int errsv = errno;
        if (ENOSPC == errsv) {
            mcx_log(LOG_ERROR, "Results: Could not close result file \"%s\": No space left on device", filename);
        } else {
            mcx_log(LOG_ERROR, "Results: Could not close result file \"%s\"", filename);
        }
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

static void PpdRootFolderDestructor(PpdRootFolder * root) {
    mcx_free(root->client);
    mcx_free(root->version);
}

static PpdRootFolder * PpdRootFolderCreate(PpdRootFolder * root) {
    PpdElement * element = (PpdElement *) root;

    element->PrintIndented = PpdRootFolderPrintIndented;

    root->Setup = PpdRootFolderSetup;
    root->Print = PpdRootFolderPrint;

    root->client  = NULL;
    root->version = NULL;

    return root;
}

OBJECT_CLASS(PpdRootFolder, PpdFolder);


static unsigned PPD_PRECISION = 13;

static McxStatus PpdLinkPrintIndented(PpdElement * element, int level, FILE * stream) {
    PpdLink * link = (PpdLink *) element;

    size_t i = 0;

    PpdElement_fprintf(stream, level,     "LINK");
    if (link->type) {
        PpdElement_fprintf(stream, level + 1,   "link_to_file = %s \"%s\"", link->type, link->location);
    } else {
        PpdElement_fprintf(stream, level + 1,   "link_to_file = \"%s\"", link->location);
    }

    if (link->offset) {
        mcx_log(LOG_DEBUG, "PpdLinkPrintIndented: xshift=%e xmin=%e", link->offset->xshift, link->offset->xmin);
        PpdElement_fprintf(stream, level + 1,   "xshift = %*.*E", PPD_PRECISION, PPD_PRECISION, link->offset->xshift);
        PpdElement_fprintf(stream, level + 1,   "xmin = %*.*E", PPD_PRECISION, PPD_PRECISION, link->offset->xmin);
    }

    PpdElement_fprintf(stream, level,     "..");

    return RETURN_OK;
}

static McxStatus PpdLinkSetup(PpdLink * link, const char * type, const char * location) {
    if (type && !(link->type     = mcx_string_copy(type)))     { return RETURN_ERROR; }
    if (        !(link->location = mcx_string_copy(location))) { return RETURN_ERROR; }

    return RETURN_OK;
}

static McxStatus PpdLinkSetTimeOffset(PpdLink * link, double xshift, double xmin) {
    PpdElement * element = (PpdElement *) link;
    if (link->offset) {
        mcx_log(LOG_ERROR, "Results: Time offset of element %s already set",
            element->GetName(element));
        return RETURN_ERROR;
    }

    // only check if xshift != 0.0
    // xmin == 0. has an effect because the truncation is done after the shift
    if (xshift == 0.0) {
        return RETURN_OK;
    }

    link->offset = (PpdTimeOffset *) mcx_malloc(sizeof(PpdTimeOffset));
    if (!link->offset) {
        mcx_log(LOG_ERROR, "Results: Could not allocate memory for time offset of element %s",
            element->GetName(element));
        return RETURN_ERROR;
    }

    link->offset->xshift = xshift;
    link->offset->xmin   = xmin;

    return RETURN_OK;
}

static const char * PpdLinkGetName(PpdElement * element) {
    return NULL;
}

static void PpdLinkDestructor(PpdLink * link) {
    mcx_free(link->type);
    mcx_free(link->location);

    if (link->offset) {
        mcx_free(link->offset);
    }
}

static PpdLink * PpdLinkCreate(PpdLink * link) {
    PpdElement * element = (PpdElement *) link;

    element->GetName       = PpdLinkGetName;
    element->PrintIndented = PpdLinkPrintIndented;

    link->Setup = PpdLinkSetup;
    link->SetTimeOffset = PpdLinkSetTimeOffset;

    link->type     = NULL;
    link->location = NULL;

    link->offset = NULL;

    return link;
}

OBJECT_CLASS(PpdLink, PpdElement);


PpdRootFolder * PpdRootFolderMake(const char * id, const char * name, const char * client, const char * version) {
    PpdRootFolder * root = NULL;
    McxStatus retVal = RETURN_OK;

    root = (PpdRootFolder *) object_create(PpdRootFolder);
    if (!root) { return NULL; }

    retVal = root->Setup(root, id, name, client, version);
    if (RETURN_OK != retVal) { return NULL; }

    return root;
}

PpdFolder * PpdFolderMake(const char * id, const char * name) {
    PpdFolder * folder = NULL;
    McxStatus retVal = RETURN_OK;

    folder = (PpdFolder *) object_create(PpdFolder);
    if (!folder) { return NULL; }

    retVal = folder->Setup(folder, id, name);
    if (RETURN_OK != retVal) { return NULL; }

    return folder;
}

PpdLink * PpdLinkMake(const char * type, const char * location) {
    PpdLink * link = NULL;
    McxStatus retVal = RETURN_OK;

    link = (PpdLink *) object_create(PpdLink);
    if (!link) { return NULL; }

    retVal = link->Setup(link, type, location);
    if (RETURN_OK != retVal) { return NULL; }

    return link;
}

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */