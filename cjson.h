#ifndef CJSON_H_
#define CJSON_H_

typedef enum {
	CJ_OBJECT,
	CJ_ARRAY,
	CJ_STRING,
	CJ_NUMBER,
} cJsonEnum;

typedef struct cjson_element {
	cJsonEnum type;
	char *name;
	union {
		char *string;
		double number;
		struct cjson_list {
			unsigned int count;
			struct cjson_element **values;
		} array, object;
	} data;
} cJsonElement;

// @summary
// Parses the given json data and returns a filled cJsonElement structure mapping the given data.
//
// @args
// data: the json data that is to be parsed
//
// @return
// Returns a pointer to a filled cJsonElement struct upon success.
// Returns NULL on failure and sets errno to it's appropriate value listed below:
// ENOMEM: Memory allocation failed
// EUCLEAN: Malformed json data
//
cJsonElement* cjsonParseData(char *data);

// @summary
// Frees a filled cJsonElement struct mapping some arbirary json data.
//
// @args
// mapping: the filled cJsonElement struct that is to be freed
//
// @return
// ---
//
void cjsonFreeMapping(cJsonElement *mapping);

#ifdef CJSON_IMPLEMENTATION

#if !defined(CJSON_NO_DEFAULT_ALLOC) || !defined(CJSON_NO_DEFAULT_STRTOD)

#include <stdlib.h>

#endif

#ifndef CJSON_NO_DEFAULT_ALLOC

#define cjson_malloc(NSIZE) malloc(NSIZE)
#define cjson_realloc(OLD_PTR, NEW_NSIZE) realloc(OLD_PTR, NEW_NSIZE)
#define cjson_free(PTR) free(PTR)

#endif // CJSON_NO_DEFAULT_ALLOC

#include <errno.h>
#include <string.h>

static unsigned int cjsonIsWhitespace(char c);
static char* cjsonMoveToNextChar(char *ptr, char haltSymbol);
static char* cjsonReadString(char *strStart, cJsonElement *string);
static char* cjsonReadNumber(char *numStart, cJsonElement *number);
static char* cjsonReadObject(char *data, cJsonElement *object);
static char* cjsonReadArray(char *data, cJsonElement *array);

static unsigned int cjsonIsWhitespace(char c)
{
	return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}

static char* cjsonMoveToNextChar(char *ptr, char haltSymbol)
{
	while(*ptr != 0 && *ptr != haltSymbol)
		ptr++;
	if(*ptr == 0)
		return NULL;
	return ptr;
}

static char* cjsonReadString(char *strStart, cJsonElement *string)
{
	char *endp = cjsonMoveToNextChar(strStart + 1, '"');
	if(!endp) // malformed json
	{
		errno = EUCLEAN;
		return NULL;
	}

	char *fieldTerm = endp + 1;
	while(cjsonIsWhitespace(*fieldTerm))
		fieldTerm++;

	if(*fieldTerm != ',' && *fieldTerm != '}' && *fieldTerm != ']') // malformed json
	{
		errno = EUCLEAN;
		return NULL;
	}

	string->type = CJ_STRING;
	string->data.string = cjson_malloc(endp - strStart);
	if(!string->data.string)
	{
		errno = ENOMEM;
		return NULL;
	}
	memset(string->data.string, 0, endp - strStart);
	memcpy(string->data.string, strStart + 1, endp - strStart - 1);

	return fieldTerm;
}

static char* cjsonReadNumber(char *numStart, cJsonElement *number)
{
	char *endp = NULL;
	double n = strtod(numStart, &endp);
	if(endp == numStart) // invalid number
	{
		errno = EUCLEAN;
		return NULL;
	}

	while(cjsonIsWhitespace(*endp))
		endp++;

	if(*endp != ',' && *endp != '}' && *endp != ']') // malformed json
	{
		errno = EUCLEAN;
		return NULL;
	}

	number->type = CJ_NUMBER;
	number->data.number = n;
	return endp;
}

// @summary
// An abstrcted fuction to read json fields.
//
// @args
// startp: the start of the field to read.
// endp: an optional parameter. When set the field is treated as an object member; if not it is treated as an array value
// values: a pointer to a pointer of cJsonElement pointers. I know it's cursed but it works and is syntactically valid
// count: a pointer to an integer holding the current number of elements in 'values'
// capacity: a pointer to an integer holding the current capacity of 'values'
//
// @return
// Returns the endpoint of the read field upon successfully reading it.
// Returns NULL upon failing to read the field and sets errno to it's appropriate value, if possible.
//
static char* cjsonReadField(char *startp, char *endp, cJsonElement ***values, int *count, int *capacity)
{
	if(*count >= *capacity)
	{
		cJsonElement **tmp = cjson_realloc(*values, sizeof(cJsonElement*) * (*capacity << 1));
		if(!tmp)
		{
			errno = ENOMEM;
			return NULL;
		}
		*values = tmp;
		*capacity = *capacity << 1;
	}

	values[0][*count] = cjson_malloc(sizeof(cJsonElement));
	if(!values[0][*count])
	{
		errno = ENOMEM;
		return NULL;
	}

	*count = *count + 1;
	memset(values[0][*count - 1], 0, sizeof(cJsonElement));
	if(endp)
	{
		values[0][*count - 1]->name = cjson_malloc(endp - startp);
		if(!values[0][*count - 1]->name)
		{
			errno = ENOMEM;
			return NULL;
		}

		memset(values[0][*count - 1]->name, 0, endp - startp);
		memcpy(values[0][*count - 1]->name, startp + 1, endp - startp - 1);

		startp = cjsonMoveToNextChar(endp, ':') + 1; // + 1 as we want to skip the ':'
	}

	// Read field
	while(cjsonIsWhitespace(*startp))
		startp++;

	if(*startp == '{')
		endp = cjsonReadObject(startp, values[0][*count - 1]);
	else if(*startp == '[')
		endp = cjsonReadArray(startp, values[0][*count - 1]);
	else if(*startp == '"')
		endp = cjsonReadString(startp, values[0][*count - 1]);
	else if((*startp >= '0' && *startp <= '9') || *startp == '.')
		endp = cjsonReadNumber(startp, values[0][*count - 1]);
	else // malformed json
	{
//		*count = *count + 1; // we need to increase count by one to properly error handle the allocated children
		errno = EUCLEAN;
		return NULL;
	}

	return endp;
}

static char* cjsonReadObject(char *data, cJsonElement *object)
{
	object->type = CJ_OBJECT;

	// We'll have to recalculate 'nextClosing' repeatedly since we may not fetch
	// the closing bracket of the current element but one of it's children
	char *startp = cjsonMoveToNextChar(data, '"'), *nextClosing = cjsonMoveToNextChar(data, '}'), *endp = NULL;
	if(nextClosing < startp)
		return nextClosing; // no more fields to read (e.g. the object is empty)

	int count = 0, capacity = 2;
	cJsonElement **fields = cjson_malloc(sizeof(cJsonElement*) * capacity);
	if(!fields)
	{
		errno = ENOMEM;
		return NULL;
	}

	int error = 0;
	while(!error && startp && nextClosing && nextClosing > startp)
	{
		// Read field name
		endp = cjsonMoveToNextChar(startp + 1, '"');
		if(!endp) // malformed json
		{
			errno = EUCLEAN;
			error = 1;
			break;
		}

		endp = cjsonReadField(startp, endp, &fields, &count, &capacity);
		if(!endp) // errno may be some unspecified value meaning we must not change it
		{
			error = 1;
			break;
		}

		startp = cjsonMoveToNextChar(endp, '"');
		nextClosing = cjsonMoveToNextChar(endp, '}');
	}

	if(error) // failure
	{
		for(int i = 0; i < count - 1; ++i)
			cjsonFreeMapping(fields[i]);

		cjson_free(fields[count - 1]); // we know the last element failed and we need to free it, but not it's children
		return NULL;
	}

	object->data.object.count = count;
	object->data.object.values = fields;

	return endp + 1; // + 1 as we want to skip the '}'
}

// If you have any questions regarding this function please refer to cjsonReadObject as they are extremly similar and thus I didn't copy comments
static char* cjsonReadArray(char *data, cJsonElement *array)
{
	data++;

	int count = 0, capacity = 2;
	cJsonElement **values = cjson_malloc(sizeof(cJsonElement*) * capacity);
	if(!values)
	{
		errno = ENOMEM;
		return NULL;
	}

	int error = 0;
	while(data && *data != ']')
	{
		data = cjsonReadField(data, NULL, &values, &count, &capacity);

		if(!data)
		{
			error = 1;
			break;
		}

		if(*data == ',')
			data++;

		while(cjsonIsWhitespace(*data))
			data++;

		if(*data == 0)
		{
			errno = EUCLEAN;
			error = 1;
			break;
		}
	}

	if(!data || error)
	{
		for(int i = 0; i < count - 1; ++i)
			cjsonFreeMapping(values[i]);

		cjson_free(values[count - 1]);
		return NULL;
	}

	array->data.array.count = count;
	array->data.array.values = values;

	return data + 1; // + 1 as we want to skip ']'
}

cJsonElement* cjsonParseData(char *data)
{
	if(!data || (*data != '{' && *data != '[')) // Invalid pointer or data start
	{
		errno = EUCLEAN;
		return NULL;
	}

	cJsonElement *root = cjson_malloc(sizeof(cJsonElement));
	if(!root)
	{
		errno = ENOMEM;
		return NULL;
	}

	char *ptr = NULL;
	memset(root, 0, sizeof(cJsonElement));
	if(*data == '{')
		ptr = cjsonReadObject(data, root);
	else
		ptr = cjsonReadArray(data, root);

	if(ptr)
		return root;

	// data may a be malformed json but failure to read a string/number or allocation
	// also cause NULL to be returned so we must not modify errno here
	cjsonFreeMapping(root);
	return NULL;
}

void cjsonFreeMapping(cJsonElement *mapping)
{
	if(mapping->name)
	{
		cjson_free(mapping->name);
		mapping->name = NULL;
	}

	if(mapping->type == CJ_NUMBER)
	{
		cjson_free(mapping);
		return;
	}
	else if(mapping->type == CJ_STRING)
	{
		if(mapping->data.string)
			cjson_free(mapping->data.string);

		mapping->data.string = NULL;
		return;
	}

	// It doesn't matter wether we use .array or .object as they use the same struct
	// We could even mix the two and it would still work due to them being stored in a union
	if(mapping->data.object.values)
	{
		for(int i = 0; i < mapping->data.object.count; ++i)
			cjsonFreeMapping(mapping->data.object.values[i]);

		cjson_free(mapping->data.object.values);
		mapping->data.object.values = NULL;
	}

	cjson_free(mapping);
}

#endif // CJSON_IMPLEMENTATION

#endif // CJSON_H_
