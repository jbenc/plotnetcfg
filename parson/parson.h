/*
 Parson ( http://kgabis.github.com/parson/ )
 Copyright (c) 2012 - 2014 Krzysztof Gabis
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
*/

#ifndef parson_parson_h
#define parson_parson_h

#ifdef __cplusplus
extern "C"
{
#endif    
    
#include <stddef.h>   /* size_t */    
    
/* Types and enums */
typedef struct json_object_t JSON_Object;
typedef struct json_array_t  JSON_Array;
typedef struct json_value_t  JSON_Value;

typedef enum json_value_type {
    JSONError   = 0,
    JSONNull    = 1,
    JSONString  = 2,
    JSONNumber  = 3,
    JSONObject  = 4,
    JSONArray   = 5,
    JSONBoolean = 6
} JSON_Value_Type;
   
/* Parses first JSON value in a file, returns NULL in case of error */
JSON_Value  * json_parse_file(const char *filename);

/* Parses first JSON value in a file and ignores comments (/ * * / and //),
   returns NULL in case of error */
JSON_Value  * json_parse_file_with_comments(const char *filename);
    
/*  Parses first JSON value in a string, returns NULL in case of error */
JSON_Value  * json_parse_string(const char *string);

/*  Parses first JSON value in a string and ignores comments (/ * * / and //),
    returns NULL in case of error */
JSON_Value  * json_parse_string_with_comments(const char *string);
    
/* Serialization */
size_t json_serialization_size_in_bytes(const JSON_Value *value);
int    json_serialize_to_buffer(const JSON_Value *value, char *buf, size_t buf_size_in_bytes);
char * json_serialize(const JSON_Value *value);
void   json_free_serialization_string(char *string);
    
/* Verification */
int json_verify(const JSON_Value *schema, const JSON_Value *value);
    
/*
 * JSON Object
 */
JSON_Value  * json_object_get_value  (const JSON_Object *object, const char *name);
const char  * json_object_get_string (const JSON_Object *object, const char *name);
JSON_Object * json_object_get_object (const JSON_Object *object, const char *name);
JSON_Array  * json_object_get_array  (const JSON_Object *object, const char *name);
double        json_object_get_number (const JSON_Object *object, const char *name);
int           json_object_get_boolean(const JSON_Object *object, const char *name);

/* dotget functions enable addressing values with dot notation in nested objects,
 just like in structs or c++/java/c# objects (e.g. objectA.objectB.value).
 Because valid names in JSON can contain dots, some values may be inaccessible
 this way. */
JSON_Value  * json_object_dotget_value  (const JSON_Object *object, const char *name);
const char  * json_object_dotget_string (const JSON_Object *object, const char *name);
JSON_Object * json_object_dotget_object (const JSON_Object *object, const char *name);
JSON_Array  * json_object_dotget_array  (const JSON_Object *object, const char *name);
double        json_object_dotget_number (const JSON_Object *object, const char *name);
int           json_object_dotget_boolean(const JSON_Object *object, const char *name);

/* Functions to get available names */
size_t        json_object_get_count(const JSON_Object *object);
const char  * json_object_get_name (const JSON_Object *object, size_t index);
    
/* Functions below return 0 on success and -1 on failure. */
/* Creates new name-value pair or frees and replaces old value with new one. */
int           json_object_set(JSON_Object *object, const char *name, JSON_Value *value);
    
/* Works like dotget functions, but creates whole hierarchy if necessary. */
int           json_object_dotset(JSON_Object *object, const char *name, JSON_Value *value);

/* Frees and removes name-value pair */
int           json_object_remove(JSON_Object *object, const char *name);

/* Works like dotget function, but removes name-value pair only on exact match. */
int           json_object_dotremove(JSON_Object *object, const char *key);

/* Removes all name-value pairs in object */
int           json_object_clear(JSON_Object *object);
    
/* 
 *JSON Array 
 */
JSON_Value  * json_array_get_value  (const JSON_Array *array, size_t index);
const char  * json_array_get_string (const JSON_Array *array, size_t index);
JSON_Object * json_array_get_object (const JSON_Array *array, size_t index);
JSON_Array  * json_array_get_array  (const JSON_Array *array, size_t index);
double        json_array_get_number (const JSON_Array *array, size_t index);
int           json_array_get_boolean(const JSON_Array *array, size_t index);
size_t        json_array_get_count  (const JSON_Array *array);
    
/* Functions below return 0 on success and -1 on failure. */

/* Frees and removes value at given index, does nothing and returns error (-1) if index doesn't exist.
 * Order of values in array may change during execution.  */
int           json_array_remove(JSON_Array *array, size_t i);

/* Frees and removes from array value at given index and replaces it with given one.
 * Does nothing and returns error (-1) if index doesn't exist. */
int           json_array_replace(JSON_Array *array, size_t i, JSON_Value *value);
    
/* Frees and removes all values from array */
int           json_array_clear(JSON_Array *array);

/* Appends new value at the end of array. */
int           json_array_append(JSON_Array *array, JSON_Value *value);

    
/*
 *JSON Value
 */
JSON_Value *    json_value_init_object(void);
JSON_Value *    json_value_init_array(void);
JSON_Value *    json_value_init_string(const char *string); /* copies passed string */
JSON_Value *    json_value_init_number(double number);
JSON_Value *    json_value_init_boolean(int boolean);
JSON_Value *    json_value_init_null(void);
JSON_Value *    json_value_deep_copy  (const JSON_Value *value);
void            json_value_free       (JSON_Value *value);

JSON_Value_Type json_value_get_type   (const JSON_Value *value);
JSON_Object *   json_value_get_object (const JSON_Value *value);
JSON_Array  *   json_value_get_array  (const JSON_Value *value);
const char  *   json_value_get_string (const JSON_Value *value);
double          json_value_get_number (const JSON_Value *value);
int             json_value_get_boolean(const JSON_Value *value);

/* Same as above, but shorter */
JSON_Value_Type json_type   (const JSON_Value *value);
JSON_Object *   json_object (const JSON_Value *value);
JSON_Array  *   json_array  (const JSON_Value *value);
const char  *   json_string (const JSON_Value *value);
double          json_number (const JSON_Value *value);
int             json_boolean(const JSON_Value *value);
    
#ifdef __cplusplus
}
#endif

#endif
