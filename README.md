# cJson

cJson is a stb-style minmal json parser for C.<br>
<br>

# Quick start

cJson uses the default stb implementation style:
```c
#include <stdio.h>
#include <errno.h>
#define CJSON_IMPLEMENTATION
#include <cjson.h>

int main(int argc, char **argv)
{
	if(argc < 2)
	{
		puts("Please provide a json string!");
		return -1;
	}

	cJsonElement *root = cjsonParse(argv[1]);
	if(!root)
	{
		puts("Failed to parse the given json data! Error:");
		if(errno == ENOMEM)
			puts("Failed to allocate memory");
		else if(errno == EUCLEAN)
			puts("Malformed json data!");
		else
			puts("Unknown error!");
		return errno;
	}

	printJson(root); // not an actual cJson function just an example

	cjsonFreeMapping(root);
	return 0;
}

```

cJson additionally features some function replacement defines, such as `CJSON_NO_DEFAULT_ALLOC` and `CJSON_NO_STRTOD`:
```c
#define CJSON_IMPLEMENTATION
#define CJSON_NO_DEFAULT_ALLOC
#define CJSON_NO_STDTOD
#include <cjson.h>
```

Important to note is that declaring these defines requires you to implement your version of the thrown out functions.
- CJSON_NO_DEFAULT_ALLOC -> cjson_malloc, cjson_realloc, cjson_free
- CJSON_NO_STRTOD -> cjson_strtod

The replacing functions need to have the same signature as the cjson_* functions.
