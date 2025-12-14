# cJson

cJson is a stb-style minmal json parser for C.<br>
<br>
This json parser doesn't validate the data for syntacital correctness, meaning this is technically correct json data:
```
{
	"name"example that would be syntactically invalid"93u!/()@: "some string value"
}
```
This is due to cjson using "anchor symbols" such as ':' to skip to the next token that is to be parsed.<br>
In the example above cjson would skip the chars between "name" and : and would result in the following mapping:
```
root
|
+> name -> "some string value"
```
This behaviour occurs both before and beyond ':' as well as the following pairs:
|||Confirmed?|
|---|---|---|
|'{'|'"'|Yes|
|'{'|'['|Yes|
|':'|'"'|Yes|
|':'|'{'|Yes|
|':'|'['|Yes|
|'"'|':'|Yes|
|','|'"'|No|

# Quick start

cJson uses the default stb implementation style:
```c
#define CJSON_IMPLEMENTATION
#include <cjson.h>

// ...

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
