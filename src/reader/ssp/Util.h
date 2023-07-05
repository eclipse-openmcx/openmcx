/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_READER_SSP_UTIL_H
#define MCX_READER_SSP_UTIL_H

#include <stdint.h>
#include <libxml/tree.h>
#include <libxml/xmlerror.h>

#include "CentralParts.h"
#include "reader/core/InputElement.h"

#include "channel_value/ChannelValue.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*****************************************************************************/
/*                             Configuration                                 */
/*****************************************************************************/
void xml_configure_path_resolution(const char * file);
void xml_cleanup_path_resolution_configuration();

void xml_set_error_handlers();
void xml_init_parser();

/*****************************************************************************/
/*                                 Files                                     */
/*****************************************************************************/
xmlDocPtr xml_read_doc(const char * file);
void xml_free_doc(xmlDocPtr doc);

/*****************************************************************************/
/*                                 Nodes                                     */
/*****************************************************************************/
xmlNodePtr xml_root(xmlDocPtr doc);

size_t xml_num_children(xmlNodePtr node);
xmlNodePtr xml_child(xmlNodePtr node, const char * node_name);
xmlNodePtr xml_child_by_index(xmlNodePtr node, size_t index);

xmlNodePtr xml_child_annotation_of_type(xmlNodePtr node, const char * type);
xmlNodePtr xml_annotation_of_type(xmlNodePtr node, const char * type);

char * xml_node_get_name(xmlNodePtr node);
size_t xml_node_line_number(xmlNodePtr node);

/*****************************************************************************/
/*                               Attributes                                  */
/*****************************************************************************/
typedef enum {
    SSD_MANDATORY,
    SSD_OPTIONAL
} SSDParamMode;

int xml_has_attr(xmlNodePtr node, const char * attribute_name);

McxStatus xml_attr_double(xmlNodePtr node, const char * attribute_name, double * dest, SSDParamMode mode);
McxStatus xml_attr_int32(xmlNodePtr node, const char * attribute_name, int32_t * dest, SSDParamMode mode);
McxStatus xml_attr_int64(xmlNodePtr node, const char * attribute_name, int64_t * dest, SSDParamMode mode);
McxStatus xml_attr_size_t(xmlNodePtr node, const char * attribute_name, size_t * dest, SSDParamMode mode);
McxStatus xml_attr_bool32(xmlNodePtr node, const char * attribute_name, int32_t * dest, SSDParamMode mode);
McxStatus xml_attr_bool64(xmlNodePtr node, const char * attribute_name, int64_t * dest, SSDParamMode mode);
McxStatus xml_attr_string(xmlNodePtr node, const char * attribute_name, char ** dest, SSDParamMode mode);
McxStatus xml_attr_path(xmlNodePtr node, const char * attribute_name, char ** dest, SSDParamMode mode);
McxStatus xml_attr_enum(xmlNodePtr node, const char * attribute_name, MapStringInt * mapping, int * dest, SSDParamMode mode);
McxStatus xml_attr_enum_weak_ptr(xmlNodePtr node, const char * attribute_name, void * map[], size_t entry_size, void ** value, SSDParamMode mode);
McxStatus xml_attr_channel_value_data(xmlNodePtr node, const char * attribute_name, ChannelType * type, ChannelValueData * dest, SSDParamMode mode);

McxStatus xml_opt_attr_double(xmlNodePtr node, const char * attribute_name, OPTIONAL_VALUE(double) * dest);
McxStatus xml_opt_attr_int32(xmlNodePtr node, const char * attribute_name, OPTIONAL_VALUE(int) * dest);
McxStatus xml_opt_attr_int64(xmlNodePtr node, const char * attribute_name, OPTIONAL_VALUE(int64_t) * dest);
McxStatus xml_opt_attr_size_t(xmlNodePtr node, const char * attribute_name, OPTIONAL_VALUE(size_t) * dest);
McxStatus xml_opt_attr_bool32(xmlNodePtr node, const char * attribute_name, OPTIONAL_VALUE(int) * dest);
McxStatus xml_opt_attr_bool64(xmlNodePtr node, const char * attribute_name, OPTIONAL_VALUE(int64_t) * dest);
McxStatus xml_opt_attr_enum(xmlNodePtr node, const char * attribute_name, MapStringInt * mapping, OPTIONAL_VALUE(int) * dest);
McxStatus xml_opt_attr_channel_value_data(xmlNodePtr node, const char * attribute_name, ChannelType * type, OPTIONAL_VALUE(ChannelValueData) * dest);

McxStatus xml_attr_double_vec(xmlNodePtr node, const char * attribute_name, size_t * len, double ** dest, SSDParamMode mode);
McxStatus xml_attr_bool32_vec(xmlNodePtr node, const char * attribute_name, size_t * len, int32_t ** dest, SSDParamMode mode);
McxStatus xml_attr_bool64_vec(xmlNodePtr node, const char * attribute_name, size_t * len, int64_t ** dest, SSDParamMode mode);
McxStatus xml_attr_int64_vec(xmlNodePtr node, const char * attribute_name, size_t * len, int64_t ** dest, SSDParamMode mode);

McxStatus xml_attr_vec_len(xmlNodePtr node, const char * attribute_name, ChannelType * type, size_t expectedLen, void ** dest, SSDParamMode mode);

/*****************************************************************************/
/*                                 Errors                                    */
/*****************************************************************************/
McxStatus xml_error_parse(xmlNodePtr node);
McxStatus xml_error_unsupported_node(xmlNodePtr node);
McxStatus xml_error_missing_child(xmlNodePtr node, const char * child_name);
McxStatus xml_error_generic(xmlNodePtr node, const char * format, ...);

McxStatus xml_warning_generic(xmlNodePtr node, const char * format, ...);


#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif // !MCX_READER_SSP_UTIL_H