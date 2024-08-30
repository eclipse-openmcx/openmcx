/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "reader/ssp/Config.h"
#include "reader/ssp/Schema.h"
#include "util/paths.h"
#include "util/os.h"
#include "units/Units.h"

#include <string.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


static DcpConfigInput * SSDReadDcpConfig(xmlNodePtr dcpNode) {
    DcpConfigInput * configInput = (DcpConfigInput*)object_create(DcpConfigInput);
    InputElement * element = (InputElement *)configInput;

    McxStatus retVal = RETURN_OK;

    if (!configInput) {
        return NULL;
    }

    if (!dcpNode) {
        mcx_log(LOG_ERROR, "SSDReadDcpConfig: No node provided");
        retVal = RETURN_ERROR;
        goto cleanup;
    }

    element->type = INPUT_SSD;
    element->context = (void*)dcpNode;

    retVal = xml_opt_attr_size_t(dcpNode, "masterPortFrom", &configInput->portFrom);
    if (retVal == RETURN_ERROR) {
        goto cleanup;
    }

    retVal = xml_opt_attr_size_t(dcpNode, "masterPortTo", &configInput->portTo);
    if (retVal == RETURN_ERROR) {
        goto cleanup;
    }

    retVal = xml_attr_string(dcpNode, "masterIp", &configInput->masterIp, SSD_OPTIONAL);
    if (retVal == RETURN_ERROR) {
        goto cleanup;
    }

cleanup:
    if (retVal == RETURN_ERROR) {
        object_destroy(configInput);
        return NULL;
    }

    return configInput;
}

ConfigInput * SSDReadConfig(xmlNodePtr annotationNode, xmlNodePtr elementsNode) {
    ConfigInput * configInput = (ConfigInput*)object_create(ConfigInput);
    InputElement * element = (InputElement *)configInput;

    McxStatus retVal = RETURN_OK;

    if (!configInput) {
        return NULL;
    }

    element->type = INPUT_SSD;

    if (annotationNode) {
        xmlNodePtr configNode = xml_child(annotationNode, "Config");

        element->context = (void*)configNode;

        if (!configNode) {
            retVal = xml_error_missing_child(annotationNode, "Config");
            goto cleanup;
        }

        retVal = xml_validate_node(configNode, "com.avl.model.connect.ssp.config");
        if (retVal == RETURN_ERROR) {
            goto cleanup;
        }

        {
            xmlNodePtr dcpNode = xml_child(configNode, "Dcp");
            if (dcpNode) {
                configInput->dcp = SSDReadDcpConfig(dcpNode);
                if (!configInput->dcp) {
                    retVal = RETURN_ERROR;
                    goto cleanup;
                }
            }
        }
    }

cleanup:
    if (retVal == RETURN_ERROR) {
        object_destroy(configInput);
        return NULL;
    }

    return configInput;
}


#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */