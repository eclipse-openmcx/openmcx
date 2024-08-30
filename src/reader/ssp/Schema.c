/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "reader/ssp/Schema.h"

#include <string.h>

#include "reader/ssp/schema/Task.h"
#include "reader/ssp/schema/Results.h"
#include "reader/ssp/schema/Config.h"
#include "reader/ssp/schema/Common.h"
#include "reader/ssp/schema/Component.h"
#include "reader/ssp/schema/ComponentResults.h"
#include "reader/ssp/schema/Connection.h"
#include "reader/ssp/schema/Decoupling.h"
#include "reader/ssp/schema/InterExtrapolation.h"
#include "reader/ssp/schema/Ports.h"
#include "reader/ssp/schema/components/Fmu.h"
#include "reader/ssp/schema/components/Integrator.h"
#include "reader/ssp/schema/components/VectorIntegrator.h"
#include "reader/ssp/schema/components/Constant.h"
#include "reader/ssp/schema/components/Dcp.h"
#include "reader/ssp/schema/Parameters.h"
#include "reader/ssp/schema/FmuParameters.h"
#include "reader/ssp/schema/standard/SystemStructureCommon.h"
#include "reader/ssp/schema/standard/SystemStructureDescription.h"
#include "reader/ssp/schema/standard/SystemStructureParameterValues.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <libxml/xmlschemas.h>


typedef struct {
    const char * id;
    const char * content;
    xmlSchemaPtr schema;                // must be present otherwise validation via context fails
    xmlSchemaValidCtxtPtr context;      // stored so that we don't have to create it whenever we validate annotations
} CatalogueEntry;


typedef struct {
    const char * name;
    const char * content;
    int read_bytes;
} SchemaLookup;


static SchemaLookup _schemaLookup[] = {
    {"Common.xsd", commonSchemaStr, 0},
    {"SystemStructureCommon.xsd", systemStructureCommonSchemaStr, 0},
    {NULL, NULL, 0}
};


static CatalogueEntry _schemaCatalogue[] = {
    {"com.avl.model.connect.ssp.task", taskSchemaStr, NULL, NULL},
    {"com.avl.model.connect.ssp.results", resultsSchemaStr, NULL, NULL},
    {"com.avl.model.connect.ssp.config", configSchemaStr, NULL, NULL},
    {"com.avl.model.connect.ssp.port", portsSchemaStr, NULL, NULL},
    {"com.avl.model.connect.ssp.connection", connectionSchemaStr, NULL, NULL},
    {"com.avl.model.connect.ssp.connection.decoupling", decouplingSchemaStr, NULL, NULL},
    {"com.avl.model.connect.ssp.connection.inter_extrapolation", interExtrapolationSchemaStr, NULL, NULL},
    {"com.avl.model.connect.ssp.component", componentSchemaStr, NULL, NULL},
    {"com.avl.model.connect.ssp.component.results", componentResultsSchemaStr, NULL, NULL},
    {"com.avl.model.connect.ssp.component.fmu", fmuSchemaStr, NULL, NULL},
    {"com.avl.model.connect.ssp.component.integrator", integratorSchemaStr, NULL, NULL},
    {"com.avl.model.connect.ssp.component.vector_integrator", vectorIntegratorSchemaStr, NULL, NULL},
    {"com.avl.model.connect.ssp.component.constant", constantSchemaStr, NULL, NULL},
    {"com.avl.model.connect.ssp.parameter", parametersSchemaStr, NULL, NULL},
    {"com.avl.model.connect.ssp.parameter.fmu", fmuParametersSchemaStr, NULL, NULL},
    {"SystemStructureDescription.xsd", systemStructureDescriptionSchemaStr, NULL, NULL},
    {"SystemStructureParameterValues.xsd", systemStructureParameterValuesSchemaStr, NULL, NULL},
    {"com.avl.model.connect.ssp.component.dcp", dcpSchemaStr, NULL, NULL},
    {NULL, NULL, NULL, NULL}
};


static int SchemaClose(void * context) {
    SchemaLookup * schema = (SchemaLookup*)context;

    if (!schema) {
        return -1;
    }

    mcx_log(LOG_DEBUG, "Closing resource: %s", schema->name);
    schema->read_bytes = 0;

    return 0;
}


static int SchemaRead(void * context, char * buffer, int len) {
    SchemaLookup * schema = (SchemaLookup*)context;

    int leftover_bytes = 0;
    int bytes_to_read = 0;

    if (!schema || !buffer) {
        return -1;
    }

    leftover_bytes = xmlStrlen(schema->content) - schema->read_bytes;
    bytes_to_read = leftover_bytes < len ? leftover_bytes : len - 1;

    mcx_log(LOG_DEBUG, "Reading %d bytes from resource: %s", bytes_to_read, schema->name);
    if (bytes_to_read < 0) {
        return -1;
    }

    strncpy(buffer, schema->content + schema->read_bytes, (size_t)bytes_to_read);
    buffer[bytes_to_read] = 0;
    schema->read_bytes += bytes_to_read;

    return bytes_to_read;
}


static void * SchemaOpen(const char * filename) {
    size_t i = 0;

    mcx_log(LOG_DEBUG, "Opening resource: %s", filename);
    for (i = 0; _schemaLookup[i].name != NULL; i++) {
        if (!strcmp(_schemaLookup[i].name, filename)) {
            return &_schemaLookup[i];
        }
    }

    return NULL;
}


static int SchemaMatch(const char * filename) {
    size_t i = 0;

    mcx_log(LOG_DEBUG, "Searching for a resource handler: %s", filename);
    for (i = 0; _schemaLookup[i].name != NULL; i++) {
        if (!strcmp(_schemaLookup[i].name, filename)) {
            return 1;
        }
    }

    return 0;
}


static CatalogueEntry * get_schema_by_id(const char * id) {
    size_t i = 0;

    for (i = 0; _schemaCatalogue[i].id != NULL; i++) {
        if (!strcmp(_schemaCatalogue[i].id, id)) {
            return &_schemaCatalogue[i];
        }
    }

    return NULL;
}


static McxStatus parse_schema(CatalogueEntry * schema_desc) {
    int ret_val = 0;
    int handler_id = -1;
    McxStatus ret_status = RETURN_OK;

    xmlSchemaParserCtxtPtr parser_context = NULL;

    if (!schema_desc) {
        mcx_log(LOG_ERROR, "parse_schema: No schema description provided");
        return RETURN_ERROR;
    }

    handler_id = xmlRegisterInputCallbacks(SchemaMatch, SchemaOpen, SchemaRead, SchemaClose);
    if (handler_id == -1) {
        mcx_log(LOG_ERROR, "Registration of input handlers for schema %s failed", schema_desc->id);
        ret_status = RETURN_ERROR;
        goto cleanup;
    }

    parser_context = xmlSchemaNewMemParserCtxt(schema_desc->content, xmlStrlen(schema_desc->content));
    if (!parser_context) {
        mcx_log(LOG_ERROR, "Parser context creation failed for schema %s", schema_desc->id);
        ret_status = RETURN_ERROR;
        goto cleanup;
    }

    schema_desc->schema = xmlSchemaParse(parser_context);
    if (!schema_desc->schema) {
        mcx_log(LOG_ERROR, "Parsing of schema %s failed", schema_desc->id);
        ret_status = RETURN_ERROR;
        goto cleanup;
    }

    schema_desc->context = xmlSchemaNewValidCtxt(schema_desc->schema);
    if (!schema_desc->context) {
        mcx_log(LOG_ERROR, "Validation context creation failed for schema %s", schema_desc->id);
        ret_status = RETURN_ERROR;
        goto cleanup;
    }

cleanup:
    if (handler_id != -1) {
        ret_val = xmlPopInputCallbacks();
        if (ret_val != handler_id) {
            mcx_log(LOG_ERROR, "Wrong input callbacks deregistered");
            ret_status = RETURN_ERROR;
        } else if (ret_val == -1) {
            mcx_log(LOG_ERROR, "Deregistering input callbacks failed");
            ret_status = RETURN_ERROR;
        }
    }

    if (parser_context) {
        xmlSchemaFreeParserCtxt(parser_context);
    }

    if (ret_status == RETURN_ERROR && schema_desc->schema) {
        xmlSchemaFree(schema_desc->schema);
        schema_desc->schema = NULL;
    }

    return ret_status;
}


void xml_cleanup_parsed_schemas() {
    size_t i = 0;

    for (i = 0; _schemaCatalogue[i].id != NULL; i++) {
        if (_schemaCatalogue[i].schema) {
            xmlSchemaFree(_schemaCatalogue[i].schema);
            _schemaCatalogue[i].schema = NULL;
        }

        if (_schemaCatalogue[i].context) {
            xmlSchemaFreeValidCtxt(_schemaCatalogue[i].context);
            _schemaCatalogue[i].context = NULL;
        }
    }
}


McxStatus xml_validate_node(xmlNodePtr node, const char * schema_id) {
    CatalogueEntry * schema;
    int ret_val = 0;

    if (!node) {
        mcx_log(LOG_ERROR, "xml_validate_node: No node given");
        return RETURN_ERROR;
    }

    if (!schema_id) {
        mcx_log(LOG_ERROR, "xml_validate_node: No schema id given");
        return RETURN_ERROR;
    }

    schema = get_schema_by_id(schema_id);
    if (!schema) {
        mcx_log(LOG_ERROR, "Unknown schema id: %s", schema_id);
        return RETURN_ERROR;
    }

    if (!schema->context) {
        if (parse_schema(schema) == RETURN_ERROR) {
            mcx_log(LOG_ERROR, "Parsing schema %s failed", schema_id);
            return RETURN_ERROR;
        }
    }

    ret_val = xmlSchemaValidateOneElement(schema->context, node);
    if (ret_val != 0) {
        mcx_log(LOG_ERROR, "In input file, line %d: Element %s does not conform to schema %s", node->line, node->name, schema_id);
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

McxStatus xml_validate_doc(xmlDocPtr doc, const char * schema_id) {
    CatalogueEntry * schema;
    int ret_val = 0;

    if (!doc) {
        mcx_log(LOG_ERROR, "xml_validate_doc: No document given");
        return RETURN_ERROR;
    }

    if (!schema_id) {
        mcx_log(LOG_ERROR, "xml_validate_doc: No schema id given");
        return RETURN_ERROR;
    }

    schema = get_schema_by_id(schema_id);
    if (!schema) {
        mcx_log(LOG_ERROR, "Unknown schema id: %s", schema_id);
        return RETURN_ERROR;
    }

    if (!schema->context) {
        if (parse_schema(schema) == RETURN_ERROR) {
            mcx_log(LOG_ERROR, "Parsing schema %s failed", schema_id);
            return RETURN_ERROR;
        }
    }

    ret_val = xmlSchemaValidateDoc(schema->context, doc);
    if (ret_val != 0) {
        mcx_log(LOG_ERROR, "Document %s does not conform to schema %s", doc->name, schema_id);
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */