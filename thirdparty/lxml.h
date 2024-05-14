/*
MIT License

Copyright (c) 2020 Simon Durbridge

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#ifndef LITTLE_XML_H
#define LITTLE_XML_H

// Includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Deffinitions
// #define DEBUG
#ifdef DEBUG
#ifndef DEBUG_PRINT
#define DEBUG_PRINT printf
#endif
#else
#ifndef DEBUG_PRINT
#define DEBUG_PRINT(...)
#endif
#endif
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

int lxml_ends_with(const char *haystack, const char *needle) {
  int h_len = strlen(haystack);
  int n_len = strlen(needle);
  if (h_len < n_len) {
    return FALSE;
  }

  for (int i = 0; i < n_len; i++) {
    if (haystack[h_len - n_len + i] != needle[i]) {
      return FALSE;
    }
  }
  return TRUE;
}

struct _LXMLNodeList {
  int heap_size;
  int size;
  struct _LXMLNode **data;
};
typedef struct _LXMLNodeList LXMLNodeList;

struct _LXMLAttribute {
  char *key;
  char *value;
};
typedef struct _LXMLAttribute LXMLAttribute;

struct _LXMLAttributeList {
  int heap_size;
  int size;
  LXMLAttribute *data;
};
typedef struct _LXMLAttributeList LXMLAttributeList;

struct _LXMLNode {
  char *tag;
  char *inner_text;
  struct _LXMLNode *parent;
  LXMLAttributeList attributes;
  LXMLNodeList children;
};
typedef struct _LXMLNode LXMLNode;

struct _LXMLDocument {
  char *version;
  char *encoding;
  LXMLNode *root;
};
typedef struct _LXMLDocument LXMLDocument;

// Forward declaration

int LXMLDocument_load(LXMLDocument *doc, const char *path);
int LXMLDocument_write(LXMLDocument *doc, const char *path, int indent);
void LXMLDocument_free(LXMLDocument *doc);
LXMLNode *LXMLNode_new(LXMLNode *parent);
void LXMLNode_free(LXMLNode *node);
// LXMLAttribute* LXMLAttribute_new(LXMLNode* parent);
void LXMLAttribute_free(LXMLAttribute *attribute);
void LXMLAttributeList_init(LXMLAttributeList *list);
void LXMLAttributeList_free(LXMLAttributeList *list);
void LXMLAttributeList_add(LXMLAttributeList *list, LXMLAttribute *attribute);
void LXMLNodeList_init(LXMLNodeList *list);
void LXMLNodeList_free(LXMLNodeList *list);
void LXMLNodeList_add(LXMLNodeList *list, LXMLNode *node);
LXMLNode *LXMLNode_child(LXMLNode *parent, int index);
char *LXMLNode_attribute_value(LXMLNode *node, char *key);
LXMLNode *LXMLNodeList_at(LXMLNodeList *list, int index);
LXMLNodeList *LXMLNode_children(LXMLNode *parent, const char *tag);
LXMLAttribute *LXMLNode_attribute(LXMLNode *node, char *key);

// Implementations

enum _TagType { TAG_START, TAG_INLINE };
typedef enum _TagType TagType;

/** static void parse_attributes(char* buffer, int* i, char* lex, int* lexi,
 * LXMLNode* current_node)
 *
 */
static TagType parse_attributes(
  char *buffer, int *i, char *lex, int *lexi, LXMLNode *current_node) {
  LXMLAttribute currentAttribute = {0, 0};
  // Read the beginning of the tag of the node into the buffer
  while (buffer[(*i)] != '>') {
    lex[(*lexi)++] = buffer[(*i)++];
    // If we have it a patch of whitespace and we have not written a tag yet,
    // lex buffer now has the tag
    if (buffer[(*i)] == ' ' && !current_node->tag) {
      lex[(*lexi)] = '\0';
      // Create a new string with the same content as what we just read and
      // assign to the tag of the node
      current_node->tag = strdup(lex);
      DEBUG_PRINT("Tag of new node is %s \n", current_node->tag);
      // Reset index to lex buffer
      (*lexi) = 0;
      (*i)++;
      continue;
    }
    // unusally ignore spaces
    if (lex[(*lexi) - 1] == ' ') {
      (*lexi)--;
    }
    // If we hit an equals, we have the attribute key in the buffer
    if (buffer[(*i)] == '=') {
      lex[(*lexi)] = '\0';
      currentAttribute.key = strdup(lex);
      (*lexi) = 0;
      continue;
    }
    // attribute value
    if (buffer[(*i)] == '"') {
      if (!currentAttribute.key) {
        fprintf(
          stderr, "Value  %s has no key at node %s \n", lex, current_node->tag);
        return TAG_START; // FALSE;
      }
      (*lexi) = 0;
      (*i)++;
      while (buffer[(*i)] != '"') {
        lex[(*lexi)++] = buffer[(*i)++];
      }
      lex[(*lexi)] = '\0';
      currentAttribute.value = strdup(lex);
      LXMLAttributeList_add(&current_node->attributes, &currentAttribute);
      // Reset current attribute placeholder to empty
      currentAttribute.key = NULL;
      currentAttribute.value = NULL;
      (*lexi) = 0;
      (*i)++;
      continue;
    }
    // Inline node handling
    if (buffer[(*i) - 1] == '/' && buffer[(*i)] == '>') {
      lex[(*lexi)] = '\0';
      if (!current_node->tag) {
        current_node->tag = strdup(lex);
      }
      (*i)++;
      return TAG_INLINE;
    }
  }
  return TAG_START;
}

int LXMLDocument_load_memory(LXMLDocument *doc, char *buffer) {
  doc->root = LXMLNode_new(NULL);
  doc->encoding = NULL;
  doc->version = NULL;

  // Lexical Analysis
  char lex[256];
  int lexi = 0;
  int i = 0;

  LXMLNode *current_node = doc->root;

  // While loop that parses the document into new nodes
  while (buffer[i] != '\0') {
    if (buffer[i] == '<') {
      DEBUG_PRINT("Enterring new tag region\n");
      // null terminate anything in the lex buffer
      lex[lexi] = '\0';
      // If content has been written into lex buffer
      if (lexi > 0) {
        DEBUG_PRINT("Lex is not empty\n");
        if (!current_node) {
          fprintf(stderr, "text outside of document\n");
          return FALSE;
        }
        // Allocate a copy of the lex buffer contents to the inner text of the
        // current node
        if (current_node->inner_text != NULL) {
          free(current_node->inner_text);
        }
        current_node->inner_text = strdup(lex);
        DEBUG_PRINT("Contents of lex: %s \n", lex);
        DEBUG_PRINT("Contents of lext coppied to inner text\n");
        lexi = 0;
      } else {
        DEBUG_PRINT("Lex is empty \n");
      }
      // End of node
      if (buffer[i + 1] == '/') {
        DEBUG_PRINT("Entering end node region\n");
        i += 2; // move on to the text of the tag
        while (buffer[i] != '>') {
          lex[lexi++] = buffer[i++];
        }
        lex[lexi] = '\0';
        lexi = 0;
        if (!current_node) {
          fprintf(stderr, "Invalid LXML document: end tag at root\n");
          return FALSE;
        }
        // If these buffers are not the same then we have a problem
        if (strcmp(current_node->tag, lex)) {
          fprintf(
            stderr, "Mismatched tags (%s != %s) \n", current_node->tag, lex);
          return FALSE;
        }
        // If we hit the end tag, return to parent and continue reading
        current_node = current_node->parent;
        i++;
        continue;
      } else {
        DEBUG_PRINT("Not end node\n");
      }

      // handle comments
      if (buffer[i + 1] == '!') {
        while (buffer[i] != ' ' && buffer[i] != '>') {
          lex[lexi++] = buffer[i++];
        }
        lex[lexi] = '\0';
        if (!strcmp(lex, "<!--")) {
          lex[lexi] = '\0';
          while (!lxml_ends_with(lex, "-->")) {
            lex[lexi++] = buffer[i++];
            lex[lexi] = '\0';
          }
          continue;
        }
      }

      // handle declaration tags
      if (buffer[i + 1] == '?') {
        while (buffer[i] != ' ' && buffer[i] != '>') {
          lex[lexi++] = buffer[i++];
        }
        lex[lexi] = '\0';
        // Handle xml version spec declaration
        if (!strcmp(lex, "<?xml")) {
          lexi = 0;
          LXMLNode *desc = LXMLNode_new(NULL);
          parse_attributes(buffer, &i, lex, &lexi, desc);
          if (LXMLNode_attribute_value(desc, "version")) {
            doc->version = strdup(LXMLNode_attribute_value(desc, "version"));
          }
          doc->encoding = strdup(LXMLNode_attribute_value(desc, "encoding"));
          LXMLNode_free(desc);
          continue;
        }
      }

      // We are at a new node, so prepare current_node for the new node
      DEBUG_PRINT("Parent node of new node is %s \n", current_node->tag);
      // Parent is the last node
      current_node = LXMLNode_new(current_node);
      // Progress document pointer
      i++;
      // Parse attributes
      if (
        parse_attributes(buffer, &i, lex, &lexi, current_node) == TAG_INLINE) {
        current_node = current_node->parent;
        i++;
        continue;
      }
      // Reset index to lex buffer
      lex[lexi] = '\0';
      if (!current_node->tag) {
        // Create a new string with the same content as what we just read and
        // assign to the tag of the node
        current_node->tag = strdup(lex);
        DEBUG_PRINT("Tag of new node is %s \n", current_node->tag);
      }
      lexi = 0;
      i++; // Move on to the body
      continue;
    } else {
      // If we arent in a tag field, fill lex buffer with inner text content
      lex[lexi++] = buffer[i++];
    }
  }

  return TRUE;
}

/** bool LXMLDocument_load(LXMLDocument* doc, const char* path)
 *
 */
int LXMLDocument_load(LXMLDocument *doc, const char *path) {
  DEBUG_PRINT("opening file %s \n", path);
  FILE *file = fopen(path, "r");
  if (!file) {
    fprintf(stderr, "Failed to open file '%s' \n", path);
    return FALSE;
  }

  // Find size of file
  fseek(file, 0, SEEK_END);
  int size = ftell(file);
  fseek(file, 0, SEEK_SET);

  // Create a buffer to fill with the data of the document
  char *buffer = (char *)malloc(sizeof(char) * size + 1);
  // Read the document into the buffer
  fread(buffer, 1, size, file);
  fclose(file);
  buffer[size] = '\0';

  int ret = LXMLDocument_load_memory(doc, buffer);

  // Free the initial buffer
  free(buffer);
  // if we succeeded, return true
  return ret;
}

static void node_out(FILE *file, LXMLNode *node, int indent, int times) {
  // For all child nodes
  for (int i = 0; i < node->children.size; i++) {
    // Get the node
    LXMLNode *child = LXMLNode_child(node, i);
    // Apply spacing
    if (times > 0) {
      fprintf(file, "%*s", indent * times, " ");
    }
    // Write out tag
    fprintf(file, "<%s", child->tag);
    // Write out non-null attributes of node
    for (int j = 0; j < child->attributes.size; j++) {
      LXMLAttribute attribute = child->attributes.data[j];
      if ((!attribute.value) || (!strcmp(attribute.value, ""))) {
        continue;
      }
      fprintf(file, " %s=\"%s\"", attribute.key, attribute.value);
    }
    // If simple, make a one-line tag
    if ((child->children.size == 0) && (!child->inner_text)) {
      fprintf(file, " />\n");
    } else {
      // If node has children, start writing those on a new line.
      // If node has no children but does that innertext, write the innertext on
      // this line.
      if (child->children.size == 0) {
        fprintf(file, ">");
      } else {
        fprintf(file, ">\n");
      }
      // If node has no children but does have innertext
      // Write the innertext and end the node on this line
      if ((child->children.size == 0) && (child->inner_text)) {
        fprintf(file, "%s</%s>\n", child->inner_text, child->tag);
      } else {
        // If there are children, enter the children and begin recursive
        // writeout call
        if (child->children.size > 0) {
          node_out(file, child, indent, times + 1);
        }
        // If we have innertext
        if (child->inner_text) {
          // Apply indent
          if (times > 0) {
            fprintf(file, "%*s", indent * times, " ");
          }
          // Writeout innertext and newline the closing tag
          fprintf(file, "%s\n", child->inner_text);
        }
        // Writeout indent
        if (times > 0) {
          fprintf(file, "%*s", indent * times, " ");
        }
        // Closing tag after some combination of children and potentially inner
        // text
        fprintf(file, "</%s>\n", child->tag);
      }
    }
  }
}

/** int LXMLDocument_write(LXMLDocument* doc, const char* path, int indent)
 *
 *
 */
int LXMLDocument_write(LXMLDocument *doc, const char *path, int indent) {
  FILE *file = fopen(path, "w");
  if (!file) {
    fprintf(stderr, "Failed to open file '%s' \n", path);
    return FALSE;
  }
  // Write out LXML header
  fprintf(
    file, "<?xml version=\"%s\" encoding=\"%s\" ?>\n",
    (doc->version) ? doc->version : "1.0",
    (doc->encoding) ? doc->encoding : "UTF-8");

  node_out(file, doc->root, indent, 0);

  // close file
  fclose(file);
  return TRUE;
}

void LXMLDocument_free(LXMLDocument *doc) {
  if (doc->version) {
    free(doc->version);
  }
  if (doc->encoding) {
    free(doc->encoding);
  }
  LXMLNode_free(doc->root);
}

/** LXMLNode* LXMLNode_new(LXMLNode* parent)
 * Allocates a new node with a pointer to the partent node and null contents
 * Args: Parent: Pointer to parent node
 */
LXMLNode *LXMLNode_new(LXMLNode *parent) {
  LXMLNode *node = (LXMLNode *)malloc(sizeof(LXMLNode));
  node->parent = parent;
  node->tag = NULL;
  node->inner_text = NULL;
  LXMLAttributeList_init(&node->attributes);
  LXMLNodeList_init(&node->children);
  if (parent) {
    LXMLNodeList_add(&parent->children, node);
  }
  return node;
}

/** void LXMLNode_free(LXMLNode* node)
 * Checks and frees the contnets of the tag and inner text of a node,
 * before freeing the node itself.
 */
void LXMLNode_free(LXMLNode *node) {
  DEBUG_PRINT("Entered free of node %s \n", node->tag);
  DEBUG_PRINT("Freeing children of node %s \n", node->tag);
  LXMLNodeList_free(&node->children);
  LXMLAttributeList_free(&node->attributes);
  if (node->tag) {
    free(node->tag);
  }
  if (node->inner_text) {
    free(node->inner_text);
  }
  free(node);
}

/** void LXMLAttributeList_init(LXMLAttributeList* list)
 */
void LXMLAttributeList_init(LXMLAttributeList *list) {
  list->heap_size = 1;
  list->size = 0;
  list->data = (LXMLAttribute *)malloc(sizeof(LXMLAttribute) * list->heap_size);
}

/** void LXMLAttributeList_add(LXMLAttributeList* list, LXMLAttribute*
 * attribute)
 */
void LXMLAttributeList_add(LXMLAttributeList *list, LXMLAttribute *attribute) {
  // ensure that our list size does not go beyond the heap have made available
  while (list->size >= list->heap_size) {
    list->heap_size *= 2;
    list->data = (LXMLAttribute *)realloc(
      list->data, sizeof(LXMLAttribute) * list->heap_size);
  }
  list->data[list->size++] = *attribute;
}

/** void LXMLAttributeList_free(LXMLAttributeList* list)
 */
void LXMLAttributeList_free(LXMLAttributeList *list) {
  for (int i = 0; i < list->size; i++) {
    LXMLAttribute_free(&list->data[i]);
  }
  if (list->data) {
    free(list->data);
  }
}

/** void LXMLNodeList_init(LXMLNodeList* list)
 */
void LXMLNodeList_init(LXMLNodeList *list) {
  list->heap_size = 1;
  list->size = 0;
  list->data = (LXMLNode **)malloc(sizeof(LXMLNode *) * list->heap_size);
}

/** void LXMLNodeList_free(LXMLNodeList* list);
 *
 *
 */
void LXMLNodeList_free(LXMLNodeList *list) {
  if (list->data) {
    for (int index = 0; index < list->size; index++) {
      LXMLNode_free(list->data[index]);
    }
    free(list->data);
  }

  list->size = 0;
  list->heap_size = 0;
}

/** void LXMLNodeList_add(LXMLNodeList* list, LXMLNode* node);
 *
 *
 */
void LXMLNodeList_add(LXMLNodeList *list, LXMLNode *node) {
  // ensure that our list size does not go beyond the heap have made available
  while (list->size >= list->heap_size) {
    list->heap_size *= 2;
    list->data =
      (LXMLNode **)realloc(list->data, sizeof(LXMLNode *) * list->heap_size);
  }
  list->data[list->size++] = node;
}

/** LXMLNode* LXMLNode_child(LXMLNode* parent, int index)
 *
 *
 */
LXMLNode *LXMLNode_child(LXMLNode *parent, int index) {
  return parent->children.data[index];
}

/** char* LXMLNode_attribute_value(LXMLNode* node, char* key)
 *
 */
char *LXMLNode_attribute_value(LXMLNode *node, char *key) {
  for (int i = 0; i < node->attributes.size; i++) {
    LXMLAttribute tAttrib = node->attributes.data[i];
    if (!strcmp(tAttrib.key, key)) {
      return tAttrib.value;
    }
  }
  return NULL;
}

void LXMLAttribute_free(LXMLAttribute *attribute) {
  if (attribute->key) {
    free(attribute->key);
  }
  if (attribute->value) {
    free(attribute->value);
  }
}

/** LXMLNode* LXMLNodeList_at(LXMLNodeList* list, int index);
 * Get a node at a point in the node list
 */
LXMLNode *LXMLNodeList_at(LXMLNodeList *list, int index) {
  return list->data[index];
}

/** LXMLNodeList* LXMLNode_children(LXMLNode* node)
 * returns the nodelist of child nodes from an LXMLNode
 * Should be 'child by name'
 */
LXMLNodeList *LXMLNode_children(LXMLNode *parent, const char *tag) {
  LXMLNodeList *list = (LXMLNodeList *)malloc(sizeof(LXMLNodeList));
  LXMLNodeList_init(list);
  for (int i = 0; i < parent->children.size; i++) {
    LXMLNode *child = LXMLNode_child(parent, i);
    if (!strcmp(child->tag, tag)) {
      LXMLNodeList_add(list, child);
    }
  }
  return list;
}

/** LXMLAttribute* LXMLNode_attribute(LXMLNode* node, char* key)
 *
 *
 */
LXMLAttribute *LXMLNode_attribute(LXMLNode *node, char *key) {
  for (int i = 0; i < node->attributes.size; i++) {
    LXMLAttribute *tAttrib = &node->attributes.data[i];
    if (!strcmp(tAttrib->key, key)) {
      return tAttrib;
    }
  }
  return NULL;
}

#endif // LITTLE_XML_H