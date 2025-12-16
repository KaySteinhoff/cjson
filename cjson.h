#ifndef CJSON_H_
#define CJSON_H_

typedef enum {
	CJ_OBJECT,
	CJ_ARRAY,
	CJ_STRING,
	CJ_NUMBER,
#ifdef CJSON_IMPLEMENTATION
	CJ_QUOTE = '"',
	CJ_LABEL,
	CJ_COMMA = ',',
	CJ_DOT = '.',
	CJ_DDOT = ':',
	CJ_SQB_OPEN = '[',
	CJ_SQB_CLOSED = ']',
	CJ_CUB_OPEN = '{',
	CJ_CUB_CLOSED = '}',
#endif
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
// Parses the given json data and returns a filled cJsonElement struct.
//
// @args
// data: the json string to parse
//
// @return
// Returns a filled cJsonElement struct upon success.
// Returns NULL on failure and sets errno to it's appropriate value listed below:
// ENOMEM: Failure to allocate memory
// EUCLEAN: Malformed json string
//
cJsonElement* cjsonParse(char *data);

// @summary
// Free a filles cJsonElement struct.
//
// @args
// mapping: the filles cJsonElement struct that is to be freed
//
// @return
// ---
//
void cjsonFreeMapping(cJsonElement *mapping);

#ifdef CJSON_IMPLEMENTATION

#if !defined(CJSON_NO_DEFAULT_ALLOC) || !defined(CJSON_NO_DEFAULT_STRTOD)

#include <stdlib.h>

#endif

#ifndef CJSON_NO_DEFAULT_STRTOD

#define cjson_strtod(START, ENDP) strtod(START, ENDP)

#endif // CJSON_NO_DEFAULT_STRTOD

#ifndef CJSON_NO_DEFAULT_ALLOC

#define cjson_malloc(NSIZE) malloc(NSIZE)
#define cjson_realloc(OLD_PTR, NEW_NSIZE) realloc(OLD_PTR, NEW_NSIZE)
#define cjson_free(PTR) free(PTR)

#endif // CJSON_NO_DEFAULT_ALLOC

#include <errno.h>
#include <string.h>

typedef struct {
	cJsonEnum type;
	int length;
	char *value;
} cJsonToken;

static unsigned int cjsonIsWhitespace(char c);
static int cjsonReadNumber(cJsonToken *tokens, int count, cJsonElement *number);
static int cjsonReadString(cJsonToken *tokens, int count, char **ptr);
static int cjsonReadField(cJsonToken *tokens, cJsonEnum listStart, int count, cJsonElement *field);
static int cjsonReadList(cJsonToken *tokens, int count, cJsonElement *list);

static char* cjsonAppendToken(cJsonToken **list, int *count, int *capacity, cJsonEnum type, char *value, int length)
{
	if(*count >= *capacity)
	{
		cJsonToken *tmp = cjson_realloc(list[0], sizeof(cJsonToken) * (*capacity << 1));
		if(!tmp)
		{
			errno = ENOMEM;
			return NULL;
		}
		list[0] = tmp;
		*capacity = *capacity << 1;
	}

	list[0][*count].type = type;
	list[0][*count].value = value;
	list[0][*count].length = length;
	*count = *count + 1;

	return value + length;
}

static char* cjsonProcessToken(char *data, cJsonToken **list, int *count, int *capacity)
{
	if(!data || *data == 0)
	{
		errno = EUCLEAN;
		return NULL;
	}

	switch(*data)
	{
		case CJ_QUOTE:
			data = cjsonAppendToken(list, count, capacity, CJ_QUOTE, data, 1);
			return data;
		case CJ_COMMA:
			data = cjsonAppendToken(list, count, capacity, CJ_COMMA, data, 1);
			return data;
		case CJ_DOT:
			data = cjsonAppendToken(list, count, capacity, CJ_DOT, data, 1);
			return data;
		case CJ_DDOT:
			data = cjsonAppendToken(list, count, capacity, CJ_DDOT, data, 1);
			return data;
		case CJ_SQB_OPEN:
			data = cjsonAppendToken(list, count, capacity, CJ_SQB_OPEN, data, 1);
			return data;
		case CJ_SQB_CLOSED:
			data = cjsonAppendToken(list, count, capacity, CJ_SQB_CLOSED, data, 1);
			return data;
		case CJ_CUB_OPEN:
			data = cjsonAppendToken(list, count, capacity, CJ_CUB_OPEN, data, 1);
			return data;
		case CJ_CUB_CLOSED:
			data = cjsonAppendToken(list, count, capacity, CJ_CUB_CLOSED, data, 1);
			return data;
	}

	char *endp = NULL;
	if(*data >= '0' && *data <= '9')
	{
		long dummy = 0;
		dummy = cjson_strtod(data, &endp);
		data = cjsonAppendToken(list, count, capacity, CJ_NUMBER, data, endp - data);
		return data;
	}

	for(endp = data;
		*endp != CJ_QUOTE &&
		*endp != CJ_COMMA &&
		*endp != CJ_DOT &&
		*endp != CJ_DDOT &&
		*endp != CJ_SQB_OPEN &&
		*endp != CJ_SQB_CLOSED &&
		*endp != CJ_CUB_OPEN &&
		*endp != CJ_CUB_CLOSED; endp++)
	{ }
	data = cjsonAppendToken(list, count, capacity, CJ_LABEL, data, endp - data);
	return data;
}

static cJsonToken* cjsonLex(char *data, int *count)
{
	if(!data || (*data != '{' && *data != '['))
	{
		errno = EUCLEAN;
		return NULL;
	}

	int tokCount = 0, tokCapacity = 2;
	cJsonToken *tokens = cjson_malloc(sizeof(cJsonToken) * tokCapacity);
	if(!tokens)
	{
		errno = ENOMEM;
		return NULL;
	}

	for(; data != NULL && *data != 0;)
	{
		data = cjsonProcessToken(data, &tokens, &tokCount, &tokCapacity);
		while(cjsonIsWhitespace(*data))
			data++;
	}

	if(!data)
	{
		cjson_free(tokens);
		return NULL;
	}

	*count = tokCount;
	return tokens;
}

static unsigned int cjsonIsWhitespace(char c)
{
	return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}

// cJson factories
// Legend:
// '<>' : factory
// '()' : Set where only one element can and any one must exist
// '[]' : Set where one or more elements can and any one must exist
// '|' : Seperator of elements in a set
// '*' : One or more elements at this position (Element exclusive)
// '+' : Zero or more elements at this position (Element exclusive)
// '"?"' : Following must exist if previous is true
// '$' : Standing int for the starting of the list
// '!' : Allows zero or max elements of a set (Set exclusive)
//
// <list> -> (CJ_CUB_OPEN|CJ_SQB_OPEN) <field>* ("$ == CJ_CUB_OPEN ? CJ_CUB_CLOSED" | "$ == CJ_SQB_OPEN ? CJ_SQB_CLOSED")
// <field> -> "$ == CJ_CUB_OPEN ? <string> CJ_DDOT" (<list>|<string>|<number>) CJ_COMMA
// <string> -> CJ_QUOTE [CJ_LABEL+|CJ_DDOT+|CJ_DOT+|CJ_COMMA+]! CJ_QUOTE
// <number> -> (CJ_NUMBER)! [(CJ_DOT)! | (CJ_NUMBER)!]

static int cjsonReadNumber(cJsonToken *tokens, int count, cJsonElement *number)
{
	if(count < 1 || (tokens[0].type != CJ_DOT && tokens[0].type != CJ_NUMBER))
	{
		errno = EUCLEAN;
		return -1;
	}

	char *endp = NULL;
	number->data.number = cjson_strtod(tokens[0].value, &endp);
	// sanity check
	if(endp == tokens[0].value && tokens[0].type != CJ_DOT)
	{
		errno = EUCLEAN;
		return -1;
	}

	number->type = CJ_NUMBER;
	int length = endp - tokens[0].value, idx = 0;
	if(length == 0) // must be a single '.'
		return count - 1;

	while(length >= tokens[idx].length)
		length -= tokens[idx++].length;

	return count - idx;
}

static int cjsonReadString(cJsonToken *tokens, int count, char **ptr)
{
	if(count < 2 || tokens[0].type != CJ_QUOTE)
	{
		errno = EUCLEAN;
		return -1;
	}
	if(*ptr)
		cjson_free(*ptr);

	*ptr = NULL;
	int strLength = 1, idx = 1;
	while(	idx < count && tokens[idx].type != CJ_QUOTE)
		strLength += tokens[idx++].length;

	if(idx == count || tokens[idx].type != CJ_QUOTE)
		return -1;

	*ptr = cjson_malloc(strLength);
	if(!*ptr)
	{
		errno = ENOMEM;
		return -1;
	}

	memset(*ptr, 0, strLength);
	if(idx > 1)
		memcpy(*ptr, tokens[1].value, strLength - 1); // we can copy all values starting from the first token since they are still the original json data and thus continous until the token we last read

	return count - idx - 1; // - 1 as we want to skip the closing '"'
}

static int cjsonReadField(cJsonToken *tokens, cJsonEnum listStart, int count, cJsonElement *field)
{
	if(count < 2)
	{
		errno = EUCLEAN;
		return -1;
	}

	int idx = 0;
	if(listStart == CJ_CUB_OPEN)
	{
		idx = cjsonReadString(tokens, count, &field->name);
		if(idx < 0)
			return -1;
		else if(tokens[count - idx].type != CJ_DDOT)
		{
			errno = EUCLEAN;
			cjson_free(field->name);
			return -1;
		}
		idx = count - idx + 1;
	}

	/* Parse list, string or number */
	if(	tokens[idx].type == CJ_SQB_OPEN ||
		tokens[idx].type == CJ_CUB_OPEN)
		idx = cjsonReadList(tokens + idx, count - idx, field);
	else if(tokens[idx].type == CJ_NUMBER ||
		tokens[idx].type == CJ_DOT)
		idx = cjsonReadNumber(tokens + idx, count - idx, field);
	else if(tokens[idx].type == CJ_QUOTE)
	{
		idx = cjsonReadString(tokens + idx, count - idx, &field->data.string);
		if(idx >= 0)
			field->type = CJ_STRING;
	}
	else
	{
		errno = EUCLEAN;
		return -1;
	}

	if(idx < 0)
		return -1;

	idx = count - idx;
	if(tokens[idx].type == CJ_COMMA)
		return count - idx - 1; // - 1 as we want to skip ','
	else if((listStart == CJ_CUB_OPEN && tokens[idx].type != CJ_CUB_CLOSED) ||
		(listStart == CJ_SQB_OPEN && tokens[idx].type != CJ_SQB_CLOSED))
	{
		errno = EUCLEAN;
		cjsonFreeMapping(field);
		return -1;
	}

	return count - idx;
}

static int cjsonReadList(cJsonToken *tokens, int count, cJsonElement *list)
{
	if(count < 1 || (tokens[0].type != CJ_CUB_OPEN && tokens[0].type != CJ_SQB_OPEN))
	{
		errno = EUCLEAN;
		return -1;
	}

	int fieldsCount = 0, capacity = 2;
	cJsonElement **fields = cjson_malloc(sizeof(cJsonElement*) * capacity);
	if(!fields)
	{
		errno = ENOMEM;
		return -1;
	}

	list->type = tokens[0].type == CJ_CUB_OPEN ? CJ_OBJECT : CJ_ARRAY;
	cJsonEnum listStart = tokens[0].type, listEnd = listStart == CJ_CUB_OPEN ? CJ_CUB_CLOSED : CJ_SQB_CLOSED;
	int idx = 1, error = 0;
	/* Parse fields */
	while(idx < count && tokens[idx].type != listEnd)
	{
		if(fieldsCount >= capacity)
		{
			cJsonElement **tmp = cjson_realloc(fields, sizeof(cJsonElement*) * (capacity << 1));
			if(!tmp)
			{
				errno = ENOMEM;
				error = 1;
				break;
			}
			fields = tmp;
			capacity <<= 1;
		}

		fields[fieldsCount] = cjson_malloc(sizeof(cJsonElement));
		if(!fields[fieldsCount])
		{
			errno = ENOMEM;
			error = 1;
			break;
		}

		idx = cjsonReadField(tokens + idx, listStart, count - idx, fields[fieldsCount++]);
		if(idx < 0)
			break;

		idx = count - idx;
		if(tokens[idx].type == CJ_COMMA)
			idx++;
	}

	if(error || idx < 0)
	{
		// If the json data was malformed we know it happend on the last element and we'll need to free it separately
		int num = errno == EUCLEAN ? fieldsCount - 1 : fieldsCount;
		for(int i = 0; i < num; ++i)
			cjsonFreeMapping(fields[i]);

		if(errno == EUCLEAN)
			cjson_free(fields[fieldsCount - 1]);

		cjson_free(fields);
		return -1;
	}

	if(tokens[idx].type != listEnd)
	{
		errno = EUCLEAN;
		for(int i = 0; i < fieldsCount; ++i)
			cjsonFreeMapping(fields[i]);

		cjson_free(fields);
		return -1;
	}

	list->data.object.count = fieldsCount;
	list->data.object.values = fields;
	return count - idx - 1; // - 1 as we want to skip the closing bracket
}

cJsonElement* cjsonParse(char *data)
{
	if(!data || (*data != '{' && *data != '[')) // Invalid pointer or data start
	{
		errno = EUCLEAN;
		return NULL;
	}

	int tokCount = 0;
	cJsonToken *list = cjsonLex(data, &tokCount);
	if(!list)
		return NULL;

	cJsonElement *root = cjson_malloc(sizeof(cJsonElement));
	if(!root)
	{
		cjson_free(list);
		errno = ENOMEM;
		return NULL;
	}

	memset(root, 0, sizeof(cJsonElement));
	tokCount = cjsonReadList(list, tokCount, root);

	cjson_free(list);
	if(tokCount == 0)
		return root;

	// data may a be malformed json but failure to read a string/number or allocation
	// of heap memory also cause NULL to be returned so we must not modify errno here
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
