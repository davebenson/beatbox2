#ifndef __BB_XML_H_
#define __BB_XML_H_

typedef struct _BbXml BbXml;

#include <glib.h>

typedef enum
{
  BB_XML_NODE,
  BB_XML_TEXT
} BbXmlType;

struct _BbXml
{
  BbXmlType type;
  char *str;		/* name or text */
  char **attrs;		/* key-value pairs, or NULL */
  BbXml *parent;
  BbXml *first_child, *last_child;
  BbXml *prev_sibling, *next_sibling;
};

typedef void (*BbXmlFunc) (BbXml   *xml,
                           gpointer data);

BbXml   *bb_xml_parse_string (const char *str,
                              GError    **error);
BbXml   *bb_xml_parse_file   (const char *str,
                              GError    **error);
void     bb_xml_iterate      (BbXml      *xml,
                              const char *path,
                              BbXmlFunc   func,
          		      gpointer    data);
BbXml   *bb_xml_find_child   (BbXml      *xml,
                              const char *name);
const char *bb_xml_find_attr (BbXml      *xml,
                              const char *name);
char    *bb_xml_get_all_text (BbXml      *xml);
void     bb_xml_free         (BbXml      *xml);

BbXml   *bb_xml_new_text     (const char *text);
BbXml   *bb_xml_new_text_len (const char *text,
                              guint       len);
BbXml   *bb_xml_new_node     (const char *name,
                              char      **key_value_pairs);
BbXml   *bb_xml_new_node2    (const char *name,
                              const char**attr_keys,
                              const char**attr_values);
void     bb_xml_add_child    (BbXml      *parent,
                              BbXml      *child);
void     bb_xml_add_text_child(BbXml     *parent,
                               const char *tag,
                               const char *contents);
char    *bb_xml_to_string    (BbXml      *xml);
gboolean bb_xml_save         (BbXml      *xml,
                              const char *filename,
                              GError    **error);

#endif
