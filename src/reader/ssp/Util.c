/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "reader/ssp/Util.h"

#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#include "util/os.h"
#include "util/paths.h"
#include "util/string.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

static void trim_trailing_whitespaces(char * str) {
    if (str) {
        char * end = str + strlen(str) - 1;
        while (end > str && isspace(*end)) {
            end--;
        }

        end[1] = '\0';
    }
}

static void structured_error_handler(void * user_data, xmlErrorPtr error) {
    LogSeverity logSeverity = LOG_INFO;
    char * msg_copy = NULL;

    switch (error->level) {
        case XML_ERR_ERROR:
            logSeverity = LOG_ERROR;
            break;
        case XML_ERR_FATAL:
            logSeverity = LOG_FATAL;
            break;
        case XML_ERR_WARNING:
            logSeverity = LOG_WARNING;
            break;
        default:
            logSeverity = LOG_INFO;
            break;
    }

    msg_copy = mcx_string_copy(error->message);
    trim_trailing_whitespaces(msg_copy);

    if (error->line > 0) {
        mcx_log(logSeverity, "In input file, line %d: %s", error->line, msg_copy);
    } else {
        mcx_log(logSeverity, "In input file: %s", msg_copy);
    }

    mcx_free(msg_copy);
}

void xml_set_error_handlers() {
    xmlSetStructuredErrorFunc(NULL, structured_error_handler);
}

void xml_init_parser() {
    xmlInitParser();
}

static char * ssd_containing_dir_path = NULL;

void xml_configure_path_resolution(const char * file) {
    ssd_containing_dir_path = mcx_path_dir_name(file);
}

void xml_cleanup_path_resolution_configuration() {
    if (ssd_containing_dir_path) {
        mcx_free(ssd_containing_dir_path);
        ssd_containing_dir_path = NULL;
    }
}

xmlDocPtr xml_read_doc(const char * file) {
    if (!file) {
        mcx_log(LOG_ERROR, "xml_read_doc: No file provided");
        return NULL;
    }

    return xmlReadFile(file, NULL, 0);
}

void xml_free_doc(xmlDocPtr doc) {
    xmlFreeDoc(doc);
}

xmlNodePtr xml_root(xmlDocPtr doc) {
    if (!doc) {
        mcx_log(LOG_ERROR, "xml_root: No xml document provided");
        return NULL;
    }

    return xmlDocGetRootElement(doc);
}

xmlNodePtr xml_child(xmlNodePtr node, const char * node_name) {
    xmlNodePtr child = NULL;
    int len = 0;

    if (!node) {
        mcx_log(LOG_ERROR, "xml_child: Node can not be NULL");
        return NULL;
    }

    if (!node_name) {
        mcx_log(LOG_ERROR, "xml_child: No node name defined");
        return NULL;
    }

    child = node->children;
    len = xmlStrlen(node_name);

    while (child) {
        if (child->type == XML_ELEMENT_NODE && xmlStrncmp(child->name, node_name, len) == 0) {
            return child;
        }
        child = child->next;
    }

    return NULL;
}

xmlNodePtr xml_child_annotation_of_type(xmlNodePtr node, const char * type) {
    xmlNodePtr child = NULL;
    size_t len = 0;

    if (!node) {
        mcx_log(LOG_ERROR, "xml_child_annotation_of_type: Node can not be NULL");
        return NULL;
    }

    if (!type) {
        mcx_log(LOG_ERROR, "xml_child_annotation_of_type: No annotation type defined");
        return NULL;
    }

    child = node->children;
    len = strlen(type);


    while (child) {
        if (child->type == XML_ELEMENT_NODE && xmlStrcmp(child->name, "Annotation") == 0) {
            xmlChar * annotation_type = xmlGetProp(child, "type");

            if (xmlStrcmp(annotation_type, type) == 0) {
                xmlFree(annotation_type);
                return child;
            }

            xmlFree(annotation_type);
        }

        child = child->next;
    }

    return NULL;
}

xmlNodePtr xml_annotation_of_type(xmlNodePtr node, const char * type) {
    xmlNodePtr annotationsNode = xml_child(node, "Annotations");

    if (annotationsNode) {
        return xml_child_annotation_of_type(annotationsNode, type);
    }

    return NULL;
}

size_t xml_num_children(xmlNodePtr node) {
    return xmlChildElementCount(node);
}

xmlNodePtr xml_child_by_index(xmlNodePtr node, size_t index) {
    xmlNodePtr child = NULL;
    size_t i = 0;

    if (!node) {
        mcx_log(LOG_ERROR, "xml_child_by_index: Node can not be NULL");
        return NULL;
    }

    child = node->children;
    while (child) {
        if (child->type == XML_ELEMENT_NODE) {
            if (i == index) {
                return child;
            }
            i++;
        }
        child = child->next;
    }

    return NULL;
}

char * xml_node_get_name(xmlNodePtr node) {
    return (char *) node->name;
}

size_t xml_node_line_number(xmlNodePtr node) {
    return node->line;
}

int xml_has_attr(xmlNodePtr node, const char * attribute_name) {
    return xmlHasProp(node, attribute_name) != NULL;
}

/**
 * Parses a bool value from buffer into dest.
 *
 * @return The number of read characters from buffer or -1 if no bool
 * could be parsed.
 */
static int xml_parse_bool(const xmlChar * buffer, int * dest) {
    int value = 0;
    int ret_val = 0;
    int num = 0;

    // skip over leading whitespace
    while ((isspace(buffer[num]) && buffer[num] != '\0')) {
        num += 1;
    }

    ret_val = sscanf(buffer + num, "%d", &value);
    if (ret_val == 1) {
        *dest = value != 0 ? TRUE : FALSE;
    } else if (xmlStrncmp("true", buffer + num, 4) == 0) {
        *dest = TRUE;
    } else if (xmlStrncmp("false", buffer + num, 5) == 0) {
        *dest = FALSE;
    } else {
        return -1;
    }

    // skip over (non-whitespace) value
    while (!(isspace(buffer[num]) || buffer[num] == '\0')) {
        num += 1;
    }

    return num;
}

/**
 * Parses an int value from buffer into dest.
 *
 * @return The number of read characters from buffer or -1 if no int
 * could be parsed.
 */
static int xml_parse_int(const xmlChar * buffer, int * dest) {
    int value = 0;
    int ret_val = 0;
    int num = 0;

    // skip over leading whitespace
    while ((isspace(buffer[num]) && buffer[num] != '\0')) {
        num += 1;
    }

    ret_val = sscanf(buffer + num, "%d", &value);
    if (ret_val == 1) {
        *dest = value;
    } else {
        return -1;
    }

    // skip over (non-whitespace) value
    while (!(isspace(buffer[num]) || buffer[num] == '\0')) {
        num += 1;
    }

    return num;
}

/**
 * Parses an double value from buffer into dest.
 *
 * @return The number of read characters from buffer or -1 if no
 * double could be parsed.
 */
static int xml_parse_double(const xmlChar * buffer, double * dest) {
    double value = 0;
    int ret_val = 0;
    int num = 0;

    // skip over leading whitespace
    while ((isspace(buffer[num]) && buffer[num] != '\0')) {
        num += 1;
    }

    ret_val = sscanf(buffer + num, "%lf", &value);
    if (ret_val == 1) {
        *dest = value;
    } else {
        return -1;
    }

    // skip over (non-whitespace) value
    while (!(isspace(buffer[num]) || buffer[num] == '\0')) {
        num += 1;
    }

    return num;
}

/**
 * Parses an size_t value from buffer into dest.
 *
 * @return The number of read characters from buffer or -1 if no
 * size_t could be parsed.
 */
static int xml_parse_size_t(const xmlChar * buffer, size_t * dest) {
    size_t value = 0;
    int ret_val = 0;
    int num = 0;

    // skip over leading whitespace
    while ((isspace(buffer[num]) && buffer[num] != '\0')) {
        num += 1;
    }

    ret_val = sscanf(buffer + num, "%zu", &value);
    if (ret_val == 1) {
        *dest = value;
    } else {
        return -1;
    }

    // skip over (non-whitespace) value
    while (!(isspace(buffer[num]) || buffer[num] == '\0')) {
        num += 1;
    }

    return num;
}

McxStatus xml_attr_double(xmlNodePtr node, const char * attribute_name, double * dest, SSDParamMode mode) {
    McxStatus ret_status = RETURN_OK;
    int ret_val = 0;
    xmlChar * buffer = NULL;

    if (!node) {
        mcx_log(LOG_ERROR, "xml_attr_double: Node can not be NULL");
        ret_status = RETURN_ERROR;
        goto cleanup;
    }

    if (!attribute_name) {
        mcx_log(LOG_ERROR, "xml_attr_double: No attribute name defined");
        ret_status = RETURN_ERROR;
        goto cleanup;
    }

    if (!dest) {
        mcx_log(LOG_ERROR, "xml_attr_double: Destination can not be NULL");
        ret_status = RETURN_ERROR;
        goto cleanup;
    }

    buffer = xmlGetProp(node, attribute_name);
    if (buffer == NULL && mode == SSD_OPTIONAL) {
        ret_status = RETURN_WARNING;
        goto cleanup;
    } else if (buffer == NULL) {
        mcx_log(LOG_ERROR, "In input file, line %d: Attribute %s is not defined in node %s",
                node->line, attribute_name, node->name);
        ret_status = RETURN_ERROR;
        goto cleanup;
    }

    ret_val = xml_parse_double(buffer, dest);
    if (ret_val == -1) {
        mcx_log(LOG_ERROR, "In input file, line %d: Could not convert value of attribute %s to a double in element %s",
                node->line, attribute_name, node->name);
        ret_status = RETURN_ERROR;
        goto cleanup;
    }

cleanup:
    if (buffer) { xmlFree(buffer); }

    return ret_status;
}

McxStatus xml_attr_int(xmlNodePtr node, const char * attribute_name, int * dest, SSDParamMode mode) {
    McxStatus ret_status = RETURN_OK;
    int ret_val = 0;
    xmlChar * buffer = NULL;

    if (!node) {
        mcx_log(LOG_ERROR, "xml_attr_int: Node can not be NULL");
        ret_status = RETURN_ERROR;
        goto cleanup;
    }

    if (!attribute_name) {
        mcx_log(LOG_ERROR, "xml_attr_int: No attribute name defined");
        ret_status = RETURN_ERROR;
        goto cleanup;
    }

    if (!dest) {
        mcx_log(LOG_ERROR, "xml_attr_int: Destination can not be NULL");
        ret_status = RETURN_ERROR;
        goto cleanup;
    }

    buffer = xmlGetProp(node, attribute_name);
    if (buffer == NULL && mode == SSD_OPTIONAL) {
        ret_status = RETURN_WARNING;
        goto cleanup;
    } else if (buffer == NULL) {
        mcx_log(LOG_ERROR, "In input file, line %d: Attribute %s is not defined in node %s",
                node->line, attribute_name, node->name);
        ret_status = RETURN_ERROR;
        goto cleanup;
    }

    ret_val = xml_parse_int(buffer, dest);
    if (ret_val == -1) {
        mcx_log(LOG_ERROR, "In input file, line %d: Could not convert value of attribute %s to an integer in element %s",
                node->line, attribute_name, node->name);
        ret_status = RETURN_ERROR;
        goto cleanup;
    }

cleanup:
    if (buffer) { xmlFree(buffer); }

    return ret_status;
}

McxStatus xml_attr_size_t(xmlNodePtr node, const char * attribute_name, size_t * dest, SSDParamMode mode) {
    McxStatus ret_status = RETURN_OK;
    int ret_val = 0;
    xmlChar * buffer = NULL;

    if (!node) {
        mcx_log(LOG_ERROR, "xml_attr_size_t: Node can not be NULL");
        ret_status = RETURN_ERROR;
        goto cleanup;
    }

    if (!attribute_name) {
        mcx_log(LOG_ERROR, "xml_attr_size_t: No attribute name defined");
        ret_status = RETURN_ERROR;
        goto cleanup;
    }

    if (!dest) {
        mcx_log(LOG_ERROR, "xml_attr_size_t: Destination can not be NULL");
        ret_status = RETURN_ERROR;
        goto cleanup;
    }

    buffer = xmlGetProp(node, attribute_name);
    if (buffer == NULL && mode == SSD_OPTIONAL) {
        ret_status = RETURN_WARNING;
        goto cleanup;
    } else if (buffer == NULL) {
        mcx_log(LOG_ERROR, "In input file, line %d: Attribute %s is not defined in node %s",
                node->line, attribute_name, node->name);
        ret_status = RETURN_ERROR;
        goto cleanup;
    }

    ret_val = xml_parse_size_t(buffer, dest);
    if (ret_val == -1) {
        mcx_log(LOG_ERROR, "In input file, line %d: Could not convert value of attribute %s to an unsigned integer in element %s",
                node->line, attribute_name, node->name);
        ret_status = RETURN_ERROR;
        goto cleanup;
    }

cleanup:
    if (buffer) { xmlFree(buffer); }

    return ret_status;
}

McxStatus xml_attr_bool(xmlNodePtr node, const char * attribute_name, int * dest, SSDParamMode mode) {
    McxStatus ret_status = RETURN_OK;
    int ret_val = 0;
    xmlChar * buffer = NULL;
    int value = 0;

    if (!node) {
        mcx_log(LOG_ERROR, "xml_attr_bool: Node can not be NULL");
        ret_status = RETURN_ERROR;
        goto cleanup;
    }

    if (!attribute_name) {
        mcx_log(LOG_ERROR, "xml_attr_bool: No attribute name defined");
        ret_status = RETURN_ERROR;
        goto cleanup;
    }

    if (!dest) {
        mcx_log(LOG_ERROR, "xml_attr_bool: Destination can not be NULL");
        ret_status = RETURN_ERROR;
        goto cleanup;
    }

    buffer = xmlGetProp(node, attribute_name);
    if (buffer == NULL && mode == SSD_OPTIONAL) {
        ret_status = RETURN_WARNING;
        goto cleanup;
    } else if (buffer == NULL) {
        mcx_log(LOG_ERROR, "In input file, line %d: Attribute %s is not defined in node %s",
                node->line, attribute_name, node->name);
        ret_status = RETURN_ERROR;
        goto cleanup;
    }

    ret_val = xml_parse_bool(buffer, dest);
    if (-1 == ret_val) {
        mcx_log(LOG_ERROR, "In input file, line %d: Could not convert value of attribute %s to a boolean in element %s",
                node->line, attribute_name, node->name);
        ret_status = RETURN_ERROR;
        goto cleanup;
    }

cleanup:
    if (buffer) { xmlFree(buffer); }

    return ret_status;
}

McxStatus xml_attr_string(xmlNodePtr node, const char * attribute_name, char ** dest, SSDParamMode mode) {
    McxStatus ret_status = RETURN_OK;
    xmlChar * buffer = NULL;
    char * str_cpy = NULL;

    if (!node) {
        mcx_log(LOG_ERROR, "xml_attr_string: Node can not be NULL");
        ret_status = RETURN_ERROR;
        goto cleanup;
    }

    if (!attribute_name) {
        mcx_log(LOG_ERROR, "xml_attr_string: No attribute name defined");
        ret_status = RETURN_ERROR;
        goto cleanup;
    }

    if (!dest) {
        mcx_log(LOG_ERROR, "xml_attr_string: Destination can not be NULL");
        ret_status = RETURN_ERROR;
        goto cleanup;
    }

    buffer = xmlGetProp(node, attribute_name);
    if (buffer == NULL && mode == SSD_OPTIONAL) {
        ret_status = RETURN_WARNING;
        goto cleanup;
    } else if (buffer == NULL) {
        mcx_log(LOG_ERROR, "In input file, line %d: Attribute %s is not defined in node %s",
                node->line, attribute_name, node->name);
        ret_status = RETURN_ERROR;
        goto cleanup;
    }

    str_cpy = mcx_string_copy(buffer);
    if (!str_cpy) {
        mcx_log(LOG_ERROR, "In input file, line %d: Could not read the string value of attribute %s in element %s",
                node->line, attribute_name, node->name);
        ret_status = RETURN_ERROR;
        goto cleanup;
    }

    *dest = str_cpy;

cleanup:
    if (buffer) { xmlFree(buffer); }

    return ret_status;
}

McxStatus xml_attr_path(xmlNodePtr node, const char * attribute_name, char ** dest, SSDParamMode mode) {
    McxStatus ret_status = RETURN_OK;
    char * temp = NULL;
    char * resolved_uri = NULL;
    char * resolved_path = NULL;

    ret_status = xml_attr_string(node, attribute_name, &temp, mode);
    if (ret_status != RETURN_OK) {
        goto cleanup;
    }

    resolved_path = mcx_resolve_env_var(temp);
    if (!resolved_path) {
        mcx_log(LOG_ERROR, "In input file, line %d: Could not resolve environment variables in path %s", node->line, temp);
        ret_status = RETURN_ERROR;
        goto cleanup;
    } else {
        mcx_free(temp);
        temp = resolved_path;
    }

    resolved_uri = mcx_path_from_uri(temp);
    if (resolved_uri) {
        // the path was indeed a uri
        mcx_free(temp);
        temp = resolved_uri;
    } else {
        // the path was not a uri, so use as is
    }

    if (!mcx_path_is_absolute(temp)) {
        // a relative path, make it relative to the directory of the model file
        if (ssd_containing_dir_path != NULL) {
            char * path_list[] = { ssd_containing_dir_path, temp };
            char * old_value = temp;
            mcx_path_merge((const char * *) path_list, 2, &temp);
            mcx_free(old_value);
        } else {
            mcx_log(LOG_ERROR, "In input file, line %d: The path %s could not be adapted", node->line, temp);
            ret_status = RETURN_ERROR;
            goto cleanup;
        }
    }

    mcx_path_to_platform(temp);

    *dest = temp;

cleanup:
    if (ret_status == RETURN_ERROR) {
        if (temp) { mcx_free(temp); }
    }

    return ret_status;
}

McxStatus xml_attr_enum(xmlNodePtr node, const char * attribute_name, MapStringInt * mapping, int * dest, SSDParamMode mode) {
    McxStatus ret_status = RETURN_OK;
    size_t i = 0;
    xmlChar * buffer = NULL;

    if (!node) {
        mcx_log(LOG_ERROR, "xml_attr_enum: Node can not be NULL");
        ret_status = RETURN_ERROR;
        goto cleanup;
    }

    if (!attribute_name) {
        mcx_log(LOG_ERROR, "xml_attr_enum: No attribute name defined");
        ret_status = RETURN_ERROR;
        goto cleanup;
    }

    if (!mapping) {
        mcx_log(LOG_ERROR, "xml_attr_enum: Enum mapping can not be NULL");
        ret_status = RETURN_ERROR;
        goto cleanup;
    }

    if (!dest) {
        mcx_log(LOG_ERROR, "xml_attr_enum: Destination can not be NULL");
        ret_status = RETURN_ERROR;
        goto cleanup;
    }

    buffer = xmlGetProp(node, attribute_name);
    if (buffer == NULL && mode == SSD_OPTIONAL) {
        ret_status = RETURN_WARNING;
        goto cleanup;
    } else if (buffer == NULL) {
        mcx_log(LOG_ERROR, "In input file, line %d: Attribute %s is not defined in node %s",
                node->line, attribute_name, node->name);
        ret_status = RETURN_ERROR;
        goto cleanup;
    }

    for (i = 0; mapping[i].key != NULL; i++) {
        if (xmlStrcmp(mapping[i].key, buffer) == 0) {
            *dest = mapping[i].value;
            break;
        }
    }

cleanup:
    if (buffer) { xmlFree(buffer); }

    return ret_status;
}

McxStatus xml_attr_enum_weak_ptr(xmlNodePtr node,
                                 const char * attribute_name,
                                 void * map[],
                                 size_t entry_size,
                                 void ** value,
                                 SSDParamMode mode) {
    char * buffer = NULL;
    McxStatus ret_status = RETURN_OK;

    buffer = xmlGetProp(node, attribute_name);
    if (buffer == NULL && mode == SSD_OPTIONAL) {
        ret_status = RETURN_WARNING;
        goto cleanup;
    } else if (buffer == NULL) {
        mcx_log(LOG_ERROR, "In input file, line %d: Attribute %s is not defined in node %s",
                node->line, attribute_name, node->name);
        ret_status = RETURN_ERROR;
        goto cleanup;
    }

    {
        size_t i = 0;
        for (i = 0; map[i]; i++) {
            char * entry = map[i];

            while (*(char **)entry) {
                if (0 == strcmp(buffer, *(char **)entry)) {
                    *value = entry;
                    goto cleanup;
                }
                entry = entry + entry_size;
            }
        }
    }

    // the specified value does not occur in the map
    mcx_log(LOG_ERROR, "In input file, line %d: Unsupported value (%s) of attribute %s in node %s",
            node->line, buffer, attribute_name, node->name);
    ret_status = RETURN_ERROR;

cleanup:
    if (buffer) { xmlFree(buffer); }

    return ret_status;
}

McxStatus xml_attr_channel_value_data(xmlNodePtr node, const char * attribute_name, ChannelType type, ChannelValueData * dest, SSDParamMode mode) {
    switch (type.con) {
    case CHANNEL_DOUBLE:
        return xml_attr_double(node, attribute_name, &dest->d, mode);
    case CHANNEL_INTEGER:
        return xml_attr_int(node, attribute_name, &dest->i, mode);
    case CHANNEL_BOOL:
        return xml_attr_bool(node, attribute_name, &dest->i, mode);
    case CHANNEL_STRING:
        return xml_attr_string(node, attribute_name, &dest->s, mode);
    default:
        if (xml_child(node, attribute_name)) {
            return xml_error_generic(xml_child(node, attribute_name), "Unspoorted data type %s", ChannelTypeToString(type));
        } else {
            if (mode == SSD_MANDATORY) {
                mcx_log(LOG_ERROR, "In input file, line %d: Attribute %s is not defined in node %s", node->line, attribute_name, node->name);
                return RETURN_ERROR;
            } else {
                return RETURN_WARNING;
            }
        }
    }
}

static size_t xml_buffer_count_elements(xmlChar * buffer) {
    size_t num = 0;

    if (!buffer) {
        return SIZE_T_ERROR;
    }

    while (*buffer) {
        // skip over whitespace
        while (isspace(*buffer)) {
            buffer += 1;
        }

        if (*buffer == '\0') {
            return num;
        } else {
            while (!(isspace(*buffer) || *buffer == '\0')) {
                buffer += 1;
            }
            num += 1;
        }
    }

    return num;
}

static McxStatus xml_double_vector_get(xmlChar * buffer, size_t * num, double ** dest) {
    size_t n = 0;
    double * values = NULL;

    McxStatus retVal = RETURN_OK;

    n = xml_buffer_count_elements(buffer);
    if (n == SIZE_T_ERROR) {
        retVal = RETURN_ERROR;
        goto cleanup;
    }

    values = mcx_calloc(sizeof(double), n);
    if (!values) {
        retVal = RETURN_ERROR;
        goto cleanup;
    }

    {
        size_t i = 0;
        xmlChar * ptr = buffer;

        for (i = 0; i < n; i++) {
            int ret_status = xml_parse_double(ptr, &values[i]);
            if (ret_status == -1) {
                retVal = RETURN_ERROR;
                goto cleanup;
            }
            ptr += ret_status;
        }
    }

    *dest = values;
    *num = n;

cleanup:
    if (retVal == RETURN_ERROR) {
        if (values) { mcx_free(values); }
    }

    return retVal;
}

static McxStatus xml_bool_vector_get(xmlChar * buffer, size_t * num, int ** dest) {
    size_t n = 0;
    int * values = NULL;

    McxStatus retVal = RETURN_OK;

    n = xml_buffer_count_elements(buffer);
    if (n == SIZE_T_ERROR) {
        retVal = RETURN_ERROR;
        goto cleanup;
    }

    values = mcx_calloc(sizeof(double), n);
    if (!values) {
        retVal = RETURN_ERROR;
        goto cleanup;
    }

    {
        size_t i = 0;
        xmlChar * ptr = buffer;

        for (i = 0; i < n; i++) {
            int ret_status = xml_parse_bool(ptr, &values[i]);
            if (ret_status == -1) {
                retVal = RETURN_ERROR;
                goto cleanup;
            }
            ptr += ret_status;
        }
    }

    *dest = values;
    *num = n;

cleanup:
    if (retVal == RETURN_ERROR) {
        if (values) { mcx_free(values); }
    }

    return retVal;
}

static McxStatus xml_int_vector_get(xmlChar * buffer, size_t * num, int ** dest) {
    size_t n = 0;
    int * values = NULL;

    McxStatus retVal = RETURN_OK;

    n = xml_buffer_count_elements(buffer);
    if (n == SIZE_T_ERROR) {
        retVal = RETURN_ERROR;
        goto cleanup;
    }

    values = mcx_calloc(sizeof(double), n);
    if (!values) {
        retVal = RETURN_ERROR;
        goto cleanup;
    }

    {
        size_t i = 0;
        xmlChar * ptr = buffer;

        for (i = 0; i < n; i++) {
            int ret_status = xml_parse_int(ptr, &values[i]);
            if (ret_status == -1) {
                retVal = RETURN_ERROR;
                goto cleanup;
            }
            ptr += ret_status;
        }
    }

    *dest = values;
    *num = n;

cleanup:
    if (retVal == RETURN_ERROR) {
        if (values) { mcx_free(values); }
    }

    return retVal;
}

McxStatus xml_attr_double_vec(xmlNodePtr node, const char * attribute_name, size_t * len, double ** dest, SSDParamMode mode) {
    McxStatus ret_status = RETURN_OK;
    int ret_val = 0;
    xmlChar * buffer = NULL;

    size_t num = 0;
    double * values = NULL;

    if (!node) {
        mcx_log(LOG_ERROR, "xml_attr_double_vec: Node can not be NULL");
        ret_status = RETURN_ERROR;
        goto cleanup;
    }

    if (!attribute_name) {
        mcx_log(LOG_ERROR, "xml_attr_double_vec: No attribute name defined");
        ret_status = RETURN_ERROR;
        goto cleanup;
    }

    if (!dest) {
        mcx_log(LOG_ERROR, "xml_attr_double_vec: Destination can not be NULL");
        ret_status = RETURN_ERROR;
        goto cleanup;
    }

    buffer = xmlGetProp(node, attribute_name);
    if (buffer == NULL && mode == SSD_OPTIONAL) {
        ret_status = RETURN_WARNING;
        goto cleanup;
    } else if (buffer == NULL) {
        mcx_log(LOG_ERROR, "In input file, line %d: Attribute %s is not defined in node %s",
                node->line, attribute_name, node->name);
        ret_status = RETURN_ERROR;
        goto cleanup;
    }

    ret_status = xml_double_vector_get(buffer, &num, &values);
    if (RETURN_ERROR == ret_status) {
        mcx_log(LOG_ERROR, "In input file, line %d: Could not convert value of attribute %s to a list of doubles in element %s",
                node->line, attribute_name, node->name);

        goto cleanup;
    }

    *len = num;
    *dest = values;

cleanup:
    if (buffer) { xmlFree(buffer); }

    if (ret_status == RETURN_ERROR) {
        if (values) { mcx_free(values); }
    }

    return ret_status;
}

McxStatus xml_attr_bool_vec(xmlNodePtr node, const char * attribute_name, size_t * len, int ** dest, SSDParamMode mode) {
    McxStatus ret_status = RETURN_OK;
    int ret_val = 0;
    xmlChar * buffer = NULL;

    size_t num = 0;
    int * values = NULL;

    if (!node) {
        mcx_log(LOG_ERROR, "xml_attr_bool_vec: Node can not be NULL");
        ret_status = RETURN_ERROR;
        goto cleanup;
    }

    if (!attribute_name) {
        mcx_log(LOG_ERROR, "xml_attr_bool_vec: No attribute name defined");
        ret_status = RETURN_ERROR;
        goto cleanup;
    }

    if (!dest) {
        mcx_log(LOG_ERROR, "xml_attr_bool_vec: Destination can not be NULL");
        ret_status = RETURN_ERROR;
        goto cleanup;
    }

    buffer = xmlGetProp(node, attribute_name);
    if (buffer == NULL && mode == SSD_OPTIONAL) {
        ret_status = RETURN_WARNING;
        goto cleanup;
    } else if (buffer == NULL) {
        mcx_log(LOG_ERROR, "In input file, line %d: Attribute %s is not defined in node %s",
                node->line, attribute_name, node->name);
        ret_status = RETURN_ERROR;
        goto cleanup;
    }

    ret_status = xml_bool_vector_get(buffer, &num, &values);
    if (RETURN_ERROR == ret_status) {
        mcx_log(LOG_ERROR, "In input file, line %d: Could not convert value of attribute %s to a list of booleans in element %s",
                node->line, attribute_name, node->name);

        goto cleanup;
    }

    *len = num;
    *dest = values;

cleanup:
    if (buffer) { xmlFree(buffer); }

    if (ret_status == RETURN_ERROR) {
        if (values) { mcx_free(values); }
    }

    return ret_status;
}

McxStatus xml_attr_int_vec(xmlNodePtr node, const char * attribute_name, size_t * len, int ** dest, SSDParamMode mode) {
    McxStatus ret_status = RETURN_OK;
    int ret_val = 0;
    xmlChar * buffer = NULL;

    size_t num = 0;
    int * values = NULL;

    if (!node) {
        mcx_log(LOG_ERROR, "xml_attr_int_vec: Node can not be NULL");
        ret_status = RETURN_ERROR;
        goto cleanup;
    }

    if (!attribute_name) {
        mcx_log(LOG_ERROR, "xml_attr_int_vec: No attribute name defined");
        ret_status = RETURN_ERROR;
        goto cleanup;
    }

    if (!dest) {
        mcx_log(LOG_ERROR, "xml_attr_int_vec: Destination can not be NULL");
        ret_status = RETURN_ERROR;
        goto cleanup;
    }

    buffer = xmlGetProp(node, attribute_name);
    if (buffer == NULL && mode == SSD_OPTIONAL) {
        ret_status = RETURN_WARNING;
        goto cleanup;
    } else if (buffer == NULL) {
        mcx_log(LOG_ERROR, "In input file, line %d: Attribute %s is not defined in node %s",
                node->line, attribute_name, node->name);
        ret_status = RETURN_ERROR;
        goto cleanup;
    }

    ret_status = xml_int_vector_get(buffer, &num, &values);
    if (RETURN_ERROR == ret_status) {
        mcx_log(LOG_ERROR, "In input file, line %d: Could not convert value of attribute %s to a list of integers in element %s",
                node->line, attribute_name, node->name);

        goto cleanup;
    }

    *len = num;
    *dest = values;

cleanup:
    if (buffer) { xmlFree(buffer); }

    if (ret_status == RETURN_ERROR) {
        if (values) { mcx_free(values); }
    }

    return ret_status;
}


McxStatus xml_opt_attr_double(xmlNodePtr node, const char * attribute_name, OPTIONAL_VALUE(double) * dest) {
    McxStatus ret_val = xml_attr_double(node, attribute_name, &dest->value, SSD_OPTIONAL);

    if (ret_val == RETURN_ERROR) {
        return RETURN_ERROR;
    }

    dest->defined = ret_val == RETURN_WARNING ? FALSE : TRUE;

    return RETURN_OK;
}

McxStatus xml_opt_attr_int(xmlNodePtr node, const char * attribute_name, OPTIONAL_VALUE(int) * dest) {
    McxStatus ret_val = xml_attr_int(node, attribute_name, &dest->value, SSD_OPTIONAL);

    if (ret_val == RETURN_ERROR) {
        return RETURN_ERROR;
    }

    dest->defined = ret_val == RETURN_WARNING ? FALSE : TRUE;

    return RETURN_OK;
}

McxStatus xml_opt_attr_size_t(xmlNodePtr node, const char * attribute_name, OPTIONAL_VALUE(size_t) * dest) {
    McxStatus ret_val = xml_attr_size_t(node, attribute_name, &dest->value, SSD_OPTIONAL);

    if (ret_val == RETURN_ERROR) {
        return RETURN_ERROR;
    }

    dest->defined = ret_val == RETURN_WARNING ? FALSE : TRUE;

    return RETURN_OK;
}

McxStatus xml_opt_attr_bool(xmlNodePtr node, const char * attribute_name, OPTIONAL_VALUE(int) * dest) {
    McxStatus ret_val = xml_attr_bool(node, attribute_name, &dest->value, SSD_OPTIONAL);

    if (ret_val == RETURN_ERROR) {
        return RETURN_ERROR;
    }

    dest->defined = ret_val == RETURN_WARNING ? FALSE : TRUE;

    return RETURN_OK;
}

McxStatus xml_opt_attr_enum(xmlNodePtr node, const char * attribute_name, MapStringInt * mapping, OPTIONAL_VALUE(int) * dest) {
    McxStatus ret_val = xml_attr_enum(node, attribute_name, mapping, &dest->value, SSD_OPTIONAL);

    if (ret_val == RETURN_ERROR) {
        return RETURN_ERROR;
    }

    dest->defined = ret_val == RETURN_WARNING ? FALSE : TRUE;

    return RETURN_OK;
}

McxStatus xml_opt_attr_channel_value_data(xmlNodePtr node, const char * attribute_name, ChannelType type, OPTIONAL_VALUE(ChannelValueData) * dest) {
    McxStatus ret_val = xml_attr_channel_value_data(node, attribute_name, type, &dest->value, SSD_OPTIONAL);

    if (ret_val == RETURN_ERROR) {
        return RETURN_ERROR;
    }

    dest->defined = ret_val == RETURN_WARNING ? FALSE : TRUE;

    return RETURN_OK;
}

McxStatus xml_attr_vec_len(xmlNodePtr node, const char * attribute_name, ChannelType type, size_t expectedLen, void ** dest, SSDParamMode mode) {
    McxStatus ret_status = RETURN_OK;
    int ret_val = 0;
    xmlChar * buffer = NULL;

    void * values;
    size_t len = 0;

    if (!node) {
        mcx_log(LOG_ERROR, "xml_attr_vec_len: Node can not be NULL");
        ret_status = RETURN_ERROR;
        goto cleanup;
    }

    if (!attribute_name) {
        mcx_log(LOG_ERROR, "xml_attr_vec_len: No attribute name defined");
        ret_status = RETURN_ERROR;
        goto cleanup;
    }

    if (!dest) {
        mcx_log(LOG_ERROR, "xml_attr_vec_len: Destination can not be NULL");
        ret_status = RETURN_ERROR;
        goto cleanup;
    }

    buffer = xmlGetProp(node, attribute_name);
    if (buffer == NULL && mode == SSD_OPTIONAL) {
        ret_status = RETURN_WARNING;
        goto cleanup;
    } else if (buffer == NULL) {
        mcx_log(LOG_ERROR, "In input file, line %d: Attribute %s is not defined in node %s",
                node->line, attribute_name, node->name);
        ret_status = RETURN_ERROR;
        goto cleanup;
    }

    switch (type.con) {
    case CHANNEL_DOUBLE:
        ret_status = xml_attr_double_vec(node, attribute_name, &len, (double **) &values, mode);
        break;
    case CHANNEL_BOOL:
        ret_status = xml_attr_bool_vec(node, attribute_name, &len, (int **) &values, mode);
        break;
    case CHANNEL_INTEGER:
        ret_status = xml_attr_int_vec(node, attribute_name, &len, (int **) &values, mode);
        break;
    default:
        ret_status = RETURN_ERROR;
        mcx_log(LOG_ERROR, "xml_attr_vec_len: Illegal type %s", ChannelTypeToString(type));
        break;
    }

    if (ret_status == RETURN_ERROR) {
        goto cleanup;
    }

    if (expectedLen != len) {
        mcx_log(LOG_ERROR, "xml_attr_vec_len: Expected length (%d) does not match actual length (%d)", expectedLen, len);
        ret_status = RETURN_ERROR;
        goto cleanup;
    }

    *dest = values;

cleanup:
    if (buffer) { xmlFree(buffer); }

    return ret_status;
}

McxStatus xml_error_parse(xmlNodePtr node) {
    if (!node) {
        mcx_log(LOG_ERROR, "xml_error_parse: No node provided");
        return RETURN_ERROR;
    }

    mcx_log(LOG_ERROR, "Failed to parse element %s at line %d", node->name, node->line);
    return RETURN_ERROR;
}

McxStatus xml_error_unsupported_node(xmlNodePtr node) {
    if (!node) {
        mcx_log(LOG_ERROR, "xml_error_unsupported_node: No node provided");
        return RETURN_ERROR;
    }

    mcx_log(LOG_ERROR, "Unsupported element %s at line %d", node->name, node->line);
    return RETURN_ERROR;
}

McxStatus xml_error_missing_child(xmlNodePtr node, const char * child_name) {
    if (!node) {
        mcx_log(LOG_ERROR, "xml_error_missing_child: No node provided");
        return RETURN_ERROR;
    }

    if (!child_name) {
        mcx_log(LOG_ERROR, "xml_error_missing_child: No child name provided");
        return RETURN_ERROR;
    }

    mcx_log(LOG_ERROR, "Element %s at line %d is missing child %s", node->name, node->line, child_name);
    return RETURN_ERROR;
}


static void xml_msg_generic(LogSeverity log_severity, xmlNodePtr node, const char * format, va_list args) {
    #define MSG_MAX_SIZE 2048
    char msg[MSG_MAX_SIZE] = { 0 };

    vsnprintf(msg, MSG_MAX_SIZE, format, args);

    mcx_log(log_severity, "In input file, element %s at line %d: %s", node->name, node->line, msg);
}

McxStatus xml_error_generic(xmlNodePtr node, const char * format, ...) {
    va_list args;

    if (!node) {
        mcx_log(LOG_ERROR, "xml_generic_error: No node provided");
        return RETURN_ERROR;
    }

    va_start(args, format);
    xml_msg_generic(LOG_ERROR, node, format, args);
    va_end(args);

    return RETURN_ERROR;
}

McxStatus xml_warning_generic(xmlNodePtr node, const char * format, ...) {
    va_list args;

    if (!node) {
        mcx_log(LOG_ERROR, "xml_warning_generic: No node provided");
        return RETURN_ERROR;
    }

    va_start(args, format);
    xml_msg_generic(LOG_WARNING, node, format, args);
    va_end(args);

    return RETURN_WARNING;
}

int test_xml_vectors() {
    McxStatus retVal = RETURN_OK;

    {
        xmlChar buffer[] = " 1.1 2.2  3.3";
        double ref[] = { 1.1, 2.2, 3.3 };

        size_t num = 0;
        double * vals = NULL;

        size_t i = 0;

        retVal = xml_double_vector_get(buffer, &num, &vals);
        if (retVal == RETURN_ERROR) {
            return 1;
        }

        if (num != 3) { return 1; }
        for (i = 0; i < num; i++) {
            if (vals[i] != ref[i]) { return 1; }
        }
    }

    {
        xmlChar buffer[] = "true false 1 0 ";
        int ref[] = { 1, 0, 1, 0};

        size_t num = 0;
        int * vals = NULL;

        size_t i = 0;

        retVal = xml_bool_vector_get(buffer, &num, &vals);
        if (retVal == RETURN_ERROR) {
            return 1;
        }

        if (num != 4) { return 1; }
        for (i = 0; i < num; i++) {
            if (vals[i] != ref[i]) { return 1; }
        }
    }

    {
        xmlChar buffer[] = " 1337 4711 \t 42 ";
        int ref[] = { 1337, 4711, 42};

        size_t num = 0;
        int * vals = NULL;

        size_t i = 0;

        retVal = xml_int_vector_get(buffer, &num, &vals);
        if (retVal == RETURN_ERROR) {
            return 1;
        }

        if (num != 3) { return 1; }
        for (i = 0; i < num; i++) {
            if (vals[i] != ref[i]) { return 1; }
        }
    }



    return 0;
}


#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */