

#if !defined(parse_COCO_SCANNER_H__)
#define parse_COCO_SCANNER_H__

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

// io.h and fcntl are used to ensure binary read from streams on windows
#if _MSC_VER >= 1300
#include <io.h>
#include <fcntl.h>
#endif

#if _MSC_VER >= 1400
#define coco_swprintf swprintf_s
#elif _MSC_VER >= 1300
#define coco_swprintf _snwprintf
#elif defined __MINGW32__
#define coco_swprintf _snwprintf
#else
// assume every other compiler knows swprintf
#define coco_swprintf swprintf
#endif

#define COCO_WCHAR_MAX 65535
#define COCO_MIN_BUFFER_LENGTH 1024
#define COCO_MAX_BUFFER_LENGTH (64*COCO_MIN_BUFFER_LENGTH)
#define COCO_HEAP_BLOCK_SIZE (64*1024)
#define COCO_CPP_NAMESPACE_SEPARATOR L':'

namespace parse {


// string handling, wide character
wchar_t* coco_string_create(const wchar_t *value);
wchar_t* coco_string_create(const wchar_t *value, size_t startIndex);
wchar_t* coco_string_create(const wchar_t *value, size_t startIndex, size_t length);
wchar_t* coco_string_create_upper(const wchar_t* data);
wchar_t* coco_string_create_lower(const wchar_t* data);
wchar_t* coco_string_create_lower(const wchar_t* data, size_t startIndex, size_t dataLen);
wchar_t* coco_string_create_append(const wchar_t* data1, const wchar_t* data2);
wchar_t* coco_string_create_append(const wchar_t* data, const wchar_t value);
void  coco_string_delete(wchar_t* &data);
size_t   coco_string_length(const wchar_t* data);
bool  coco_string_endswith(const wchar_t* data, const wchar_t *value);
size_t   coco_string_indexof(const wchar_t* data, const wchar_t value);
size_t   coco_string_lastindexof(const wchar_t* data, const wchar_t value);
void  coco_string_merge(wchar_t* &data, const wchar_t* value);
bool  coco_string_equal(const wchar_t* data1, const wchar_t* data2);
int   coco_string_compareto(const wchar_t* data1, const wchar_t* data2);
int   coco_string_hash(const wchar_t* data);

// string handling, ascii character
wchar_t* coco_string_create(const char *value);
char* coco_string_create_char(const wchar_t *value);
void  coco_string_delete(char* &data);


class Token  
{
public:
	int kind;     // token kind
	size_t pos;      // token position in bytes in the source text (starting at 0)
	size_t charPos;  // token position in characters in the source text (starting at 0)
	size_t col;      // token column (starting at 1)
	size_t line;     // token line (starting at 1)
	wchar_t* val; // token value
	Token *next;  // ML 2005-03-11 Peek tokens are kept in linked list

	Token();
	~Token();
};

class Buffer {
// This Buffer supports the following cases:
// 1) seekable stream (file)
//    a) whole stream in buffer
//    b) part of stream in buffer
// 2) non seekable stream (network, console)
private:
	unsigned char *buf; // input buffer
	size_t bufCapacity;    // capacity of buf
	size_t bufStart;       // position of first byte in buffer relative to input stream
	size_t bufLen;         // length of buffer
	size_t fileLen;        // length of input stream (may change if the stream is no file)
	size_t bufPos;         // current position in buffer
	FILE* stream;       // input stream (seekable)
	bool isUserStream;  // was the stream opened by the user?
	
	size_t ReadNextStreamChunk();
	bool CanSeek();     // true if stream can be seeked otherwise false
	
public:
	static const int EoF = COCO_WCHAR_MAX + 1;

	Buffer(FILE* s, bool isUserStream);
	Buffer(const unsigned char* buf, size_t len);
	Buffer(Buffer *b);
	virtual ~Buffer();
	
	virtual void Close();
	virtual int Read();
	virtual int Peek();
	virtual wchar_t* GetString(size_t beg, size_t end);
	virtual size_t GetPos();
	virtual void SetPos(size_t value);
};

class UTF8Buffer : public Buffer {
public:
	UTF8Buffer(Buffer *b) : Buffer(b) {};
	virtual int Read();
};

//-----------------------------------------------------------------------------------
// StartStates  -- maps characters to start states of tokens
//-----------------------------------------------------------------------------------
class StartStates {
private:
	class Elem {
	public:
		int key, val;
		Elem *next;
		Elem(int key, int val) { this->key = key; this->val = val; next = NULL; }
	};

	Elem **tab;

public:
	StartStates() { tab = new Elem*[128]; memset(tab, 0, 128 * sizeof(Elem*)); }
	virtual ~StartStates() {
		for (int i = 0; i < 128; ++i) {
			Elem *e = tab[i];
			while (e != NULL) {
				Elem *next = e->next;
				delete e;
				e = next;
			}
		}
		delete [] tab;
	}

	void set(int key, int val) {
		Elem *e = new Elem(key, val);
		int k = ((unsigned int) key) % 128;
		e->next = tab[k]; tab[k] = e;
	}

	int state(int key) {
		Elem *e = tab[((unsigned int) key) % 128];
		while (e != NULL && e->key != key) e = e->next;
		return e == NULL ? 0 : e->val;
	}
};

//-------------------------------------------------------------------------------------------
// KeywordMap  -- maps strings to integers (identifiers to keyword kinds)
//-------------------------------------------------------------------------------------------
class KeywordMap {
private:
	class Elem {
	public:
		wchar_t *key;
		int val;
		Elem *next;
		Elem(const wchar_t *key, int val) { this->key = coco_string_create(key); this->val = val; next = NULL; }
		virtual ~Elem() { coco_string_delete(key); }
	};

	Elem **tab;

public:
	KeywordMap() { tab = new Elem*[128]; memset(tab, 0, 128 * sizeof(Elem*)); }
	virtual ~KeywordMap() {
		for (size_t i = 0; i < 128; ++i) {
			Elem *e = tab[i];
			while (e != NULL) {
				Elem *next = e->next;
				delete e;
				e = next;
			}
		}
		delete [] tab;
	}

	void set(const wchar_t *key, int val) {
		Elem *e = new Elem(key, val);
		int k = coco_string_hash(key) % 128;
		e->next = tab[k]; tab[k] = e;
	}

	int get(const wchar_t *key, int defaultVal) {
		Elem *e = tab[coco_string_hash(key) % 128];
		while (e != NULL && !coco_string_equal(e->key, key)) e = e->next;
		return e == NULL ? defaultVal : e->val;
	}
};

class Scanner {
private:
	void *firstHeap;
	void *heap;
	void *heapTop;
	void **heapEnd;

	unsigned char EOL;
	int eofSym;
	int noSym;
	int maxT;
	int charSetSize;
	StartStates start;
	KeywordMap keywords;

	Token *t;         // current token
	wchar_t *tval;    // text of current token
	size_t tvalLength;   // length of text of current token
	size_t tlen;         // length of current token

	Token *tokens;    // list of tokens already peeked (first token is a dummy)
	Token *pt;        // current peek token

	int ch;           // current input character

	int pos;          // byte position of current character
	int charPos;      // position by unicode characters starting with 0
	int line;         // line number of current character
	int col;          // column number of current character
	int oldEols;      // EOLs that appeared in a comment;

	void CreateHeapBlock();
	Token* CreateToken();
	void AppendVal(Token *t);
	void SetScannerBehindT();

	void Init();
	void NextCh();
	void AddCh();
	bool Comment0();
	bool Comment1();

	Token* NextToken();

public:
	Buffer *buffer;   // scanner buffer
	
	Scanner(const unsigned char* buf, size_t len);
	Scanner(const wchar_t* fileName);
	Scanner(const char* fileName); // OCL: Added c'tor
	Scanner(FILE* s);
	~Scanner();
	Token* Scan();
	Token* Peek();
	void ResetPeek();

}; // end Scanner

} // namespace


#endif

