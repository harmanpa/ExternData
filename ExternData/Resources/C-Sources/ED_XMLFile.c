/* ED_XMLFile.c - XML functions
 *
 * Copyright (C) 2015-2017, tbeu
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#if defined(__gnu_linux__)
#define _GNU_SOURCE 1
#endif

#include <string.h>
#if defined(_MSC_VER)
#define strdup _strdup
#endif
#include "ED_locale.h"
#include "bsxml.h"
#include "ModelicaUtilities.h"
#include "../Include/ED_XMLFile.h"

/* The standard way to detect posix is to check _POSIX_VERSION,
 * which is defined in <unistd.h>
 */
#if defined(__unix__) || defined(__linux__) || defined(__APPLE_CC__)
#include <unistd.h>
#endif
#if !defined(_POSIX_) && defined(_POSIX_VERSION)
#define _POSIX_ 1
#endif

/* Use re-entrant string tokenize function if available */
#if defined(_POSIX_)
#elif defined(_MSC_VER) && _MSC_VER >= 1400
#define strtok_r(str, delim, saveptr) strtok_s((str), (delim), (saveptr))
#else
#define strtok_r(str, delim, saveptr) strtok((str), (delim))
#endif

typedef struct {
	char* fileName;
	XmlNodeRef root;
	ED_LOCALE_TYPE loc;
} XMLFile;

void* ED_createXML(const char* fileName, int verbose)
{
	XmlParser xmlParser;
	XMLFile* xml = (XMLFile*)malloc(sizeof(XMLFile));
	if (xml == NULL) {
		ModelicaError("Memory allocation error\n");
		return NULL;
	}
	xml->fileName = strdup(fileName);
	if (xml->fileName == NULL) {
		free(xml);
		ModelicaError("Memory allocation error\n");
		return NULL;
	}

	if (verbose == 1) {
		/* Print info message, that file is loading */
		ModelicaFormatMessage("... loading \"%s\"\n", fileName);
	}

	xml->root = XmlParser_parse_file(&xmlParser, fileName);
	if (xml->root == NULL) {
		free(xml->fileName);
		free(xml);
		if (XmlParser_getErrorLineSet(&xmlParser) != 0) {
			ModelicaFormatError("Error \"%s\" in line %lu: Cannot parse file \"%s\"\n",
				XmlParser_getErrorString(&xmlParser), XmlParser_getErrorLine(&xmlParser), fileName);
		}
		else {
			ModelicaFormatError("Cannot read \"%s\": %s\n", fileName, XmlParser_getErrorString(&xmlParser));
		}
		return NULL;
	}
	xml->loc = ED_INIT_LOCALE;
	return xml;
}

void ED_destroyXML(void* _xml)
{
	XMLFile* xml = (XMLFile*)_xml;
	if (xml != NULL) {
		if (xml->fileName != NULL) {
			free(xml->fileName);
		}
		XmlNode_deleteTree(xml->root);
		ED_FREE_LOCALE(xml->loc);
		free(xml);
	}
}

static char* findValue(XmlNodeRef* root, const char* varName, const char* fileName)
{
	char* token = NULL;
	char* buf = strdup(varName);
	if (buf != NULL) {
		int elementError = 0;
		char* nextToken = NULL;
		token = strtok_r(buf, ".", &nextToken);
		if (token == NULL) {
			elementError = 1;
		}
		while (token != NULL && elementError == 0) {
			XmlNodeRef iter = XmlNode_findChild(*root, token);
			if (NULL != iter) {
				*root = iter;
				token = strtok_r(NULL, ".", &nextToken);
			}
			else {
				elementError = 1;
			}
		}
		free(buf);
		if (0 == elementError) {
			XmlNode_getValue(*root, &token);
		}
		else {
			ModelicaFormatMessage("Error in line %i: Cannot find element \"%s\" in file \"%s\"\n",
				XmlNode_getLine(*root), varName, fileName);
			*root = NULL;
			token = NULL;
		}
	}
	else {
		ModelicaError("Memory allocation error\n");
	}
	return token;
}

double ED_getDoubleFromXML(void* _xml, const char* varName, int* exist)
{
	double ret = 0.;
	XMLFile* xml = (XMLFile*)_xml;
	*exist = 1;
	if (xml != NULL) {
		XmlNodeRef root = xml->root;
		char* token = findValue(&root, varName, xml->fileName);
		if (token != NULL) {
			if (ED_strtod(token, xml->loc, &ret)) {
				ModelicaFormatError("Error in line %i: Cannot read double value \"%s\" from file \"%s\"\n",
					XmlNode_getLine(root), token, xml->fileName);
			}
		}
		else if (NULL != root) {
			ModelicaFormatMessage("Error in line %i: Cannot read double value from file \"%s\"\n",
				XmlNode_getLine(root), xml->fileName);
			*exist = 0;
		}
		else {
			*exist = 0;
		}
	}
	return ret;
}

const char* ED_getStringFromXML(void* _xml, const char* varName, int* exist)
{
	XMLFile* xml = (XMLFile*)_xml;
	*exist = 1;
	if (xml != NULL) {
		XmlNodeRef root = xml->root;
		char* token = findValue(&root, varName, xml->fileName);
		if (token != NULL) {
			char* ret = ModelicaAllocateString(strlen(token));
			strcpy(ret, token);
			return (const char*)ret;
		}
		else if (NULL != root) {
			ModelicaFormatMessage("Error in line %i: Cannot read value from file \"%s\"\n",
				XmlNode_getLine(root), xml->fileName);
			*exist = 0;
		}
		else {
			*exist = 0;
		}
	}
	return "";
}

int ED_getIntFromXML(void* _xml, const char* varName, int* exist)
{
	long ret = 0;
	XMLFile* xml = (XMLFile*)_xml;
	*exist = 1;
	if (xml != NULL) {
		XmlNodeRef root = xml->root;
		char* token = findValue(&root, varName, xml->fileName);
		if (token != NULL) {
			if (ED_strtol(token, xml->loc, &ret)) {
				ModelicaFormatError("Error in line %i: Cannot read int value \"%s\" from file \"%s\"\n",
					XmlNode_getLine(root), token, xml->fileName);
			}
		}
		else if (NULL != root) {
			ModelicaFormatMessage("Error in line %i: Cannot read int value from file \"%s\"\n",
				XmlNode_getLine(root), xml->fileName);
			*exist = 0;
		}
		else {
			*exist = 0;
		}
	}
	return (int)ret;
}

void ED_getDoubleArray1DFromXML(void* _xml, const char* varName, double* a, size_t n)
{
	XMLFile* xml = (XMLFile*)_xml;
	if (xml != NULL) {
		XmlNodeRef root = xml->root;
		int iLevel = 0;
		char* token = findValue(&root, varName, xml->fileName);
		while (token == NULL && XmlNode_getChildCount(root) > 0) {
			/* Try children if root is empty */
			root = XmlNode_getChild(root, 0);
			XmlNode_getValue(root, &token);
			iLevel++;
		}
		if (token != NULL) {
			char* buf = strdup(token);
			if (buf != NULL) {
				size_t i = 0;
				size_t iSibling = 0;
				XmlNodeRef parent = XmlNode_getParent(root);
				size_t nSiblings = XmlNode_getChildCount(parent);
				int line = XmlNode_getLine(root);
				int foundSibling = 0;
				char* nextToken = NULL;
				token = strtok_r(buf, "[]{},; \t", &nextToken);
				while (i < n) {
					if (token != NULL) {
						if (ED_strtod(token, xml->loc, &a[i++])) {
							free(buf);
							ModelicaFormatError("Error in line %i: Cannot read double value \"%s\" from file \"%s\"\n",
								line, token, xml->fileName);
							return;
						}
						token = strtok_r(NULL, "[]{},; \t", &nextToken);
					}
					else if (++iSibling < nSiblings) {
						/* Retrieve value from next sibling */
						XmlNodeRef child = XmlNode_getChild(parent, iSibling);
						if (child != root && XmlNode_isTag(child, XmlNode_getTag(root))) {
							foundSibling = 1;
							XmlNode_getValue(child, &token);
							line = XmlNode_getLine(child);
							free(buf);
							if (token != NULL) {
								buf = strdup(token);
								if (buf != NULL) {
									char* nextToken = NULL;
									token = strtok_r(buf, "[]{},; \t", &nextToken);
								}
								else {
									ModelicaError("Memory allocation error\n");
									return;
								}
							}
							else {
								ModelicaFormatError("Error in line %: Cannot read empty (%lu.) element \"%s\" from file \"%s\"\n",
									line, (unsigned long)(iSibling + 1), varName, xml->fileName);
								return;
							}
						}
					}
					else {
						/* Error: token is NULL and no (more) siblings */
						free(buf);
						if (foundSibling != 0) {
							const char* levels[] = {"", "child ", "grandchild ", "great-grandchild ", "great-great-grandchild "};
							XmlNodeRef child = XmlNode_getChild(parent, nSiblings - 1);
							line = XmlNode_getLine(child);
							if (iLevel > 4) {
								iLevel = 0;
							}
							ModelicaFormatError("Error after line %i: Cannot find %lu. %selement of \"%s\" in file \"%s\"\n",
								line, (unsigned long)(iSibling + 1), levels[iLevel], varName, xml->fileName);
						}
						else {
							ModelicaFormatError("Error in line %i: Cannot read %lu double values of \"%s\" from file \"%s\"\n",
								line, (unsigned long)n, varName, xml->fileName);
						}
						return;
					}
				}
				free(buf);
			}
			else {
				ModelicaError("Memory allocation error\n");
			}
		}
		else {
			ModelicaFormatError("Error in line %i: Cannot read empty element \"%s\" in file \"%s\"\n",
				XmlNode_getLine(root), varName, xml->fileName);
		}
	}
}

void ED_getDoubleArray2DFromXML(void* _xml, const char* varName, double* a, size_t m, size_t n)
{
	ED_getDoubleArray1DFromXML(_xml, varName, a, m*n);
}
