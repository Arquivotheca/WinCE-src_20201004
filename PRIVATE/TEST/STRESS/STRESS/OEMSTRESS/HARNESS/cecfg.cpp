//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*
________________________________________________________________________________
THIS  CODE AND  INFORMATION IS  PROVIDED "AS IS"  WITHOUT WARRANTY  OF ANY KIND,
EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.

Module Name: cecfg.cpp

Abstract: Contains OS component matching routines for OEM stress harness

Notes:

________________________________________________________________________________
*/

#include <windows.h>
#include <stdio.h>
#include <stressutils.h>
#include <stressutilsp.h>
#include "helpers.h"
#include "logger.h"
#include "cecfg.h"
#include "stresslist.h"


enum TOKEN
{
	TOKEN_FALSE,
	TOKEN_TRUE,
	TOKEN_OPERATOR_AND,
	TOKEN_OPERATOR_OR,
	TOKEN_OPENPAREN,
	TOKEN_CLOSEPAREN,
	TOKEN_BEGINEXPR,
	TOKEN_ENDEXPR,
	TOKEN_FAILURE
};


struct TOKEN_NODE
{
	TOKEN _token;
	TOKEN_NODE *_pNext;
};


/*

@class:	stack_t, implements the token stack to be used to evaluate the logical
expression form the initialization file; used only by logicalexpression_t

*/

class stack_t
{
	TOKEN_NODE *_pTop;	// null value ensures an empty stack

public:
	stack_t()
	{
		_pTop = NULL;
		return;
	}

	bool Push(IN TOKEN token)
	{
		TOKEN_NODE *pNode = new TOKEN_NODE;

		if (pNode)
		{
			pNode->_token = token;
			pNode->_pNext = _pTop;
			_pTop = pNode;
			return true;
		}

		return false;
	}

	bool Pop(OUT TOKEN& token)
	{
		if (!_pTop)
		{
			token = TOKEN_FAILURE;	// stack empty
			return false;
		}

		TOKEN_NODE *pNode = _pTop;
		token = pNode->_token;
		_pTop = pNode->_pNext;
		delete pNode;

		return true;
	}

	inline TOKEN Top()
	{
		if (!_pTop)
		{
			return TOKEN_FAILURE;	// stack empty
		}

		return _pTop->_token;
	}

	inline bool IsEmpty()
	{
		return !_pTop;
	}

	void Empty()
	{
		while (_pTop)
		{
			TOKEN_NODE *pNode = _pTop;
			_pTop = pNode->_pNext;
			delete pNode;
		}

		return;
	}

	~stack_t()
	{
		Empty();
		return;
	}
};


/*

@class:	queue_t, implements the token queue to be used to evaluate the logical
expression form the initialization file; used only by logicalexpression_t

*/

class queue_t
{
	TOKEN_NODE *_pFront;	// null value ensures an empty queue
	TOKEN_NODE *_pRear;	// might contain bogus address even when the queue is empty

public:
	queue_t()
	{
		_pFront = _pRear = NULL;
		return;
	}

	bool Add(IN TOKEN token)
	{
		TOKEN_NODE *pNode = new TOKEN_NODE;

		if (pNode)
		{
			pNode->_token = token;
			pNode->_pNext = NULL;

			if (!_pFront)
			{
				_pFront = _pRear = pNode;	// queue was empty
			}
			else
			{
				_pRear->_pNext = pNode;
				_pRear = pNode;
			}

			return true;
		}

		return false;
	}

	bool Delete(OUT TOKEN& token)
	{
		if (!_pFront)
		{
			token = TOKEN_FAILURE;	// queue empty
			return false;
		}
		else
		{
			TOKEN_NODE *pNode = _pFront;
			token = pNode->_token;
			_pFront = pNode->_pNext;
			delete pNode;
		}

		return true;
	}

	inline bool IsEmpty()
	{
		return !(_pFront && _pRear);
	}

	void Empty()
	{
		while (_pFront)
		{
			TOKEN_NODE *pNode = _pFront;
			_pFront = pNode->_pNext;
			delete pNode;
		}

		_pFront = _pRear = NULL;

		return;
	}

	~queue_t()
	{
		Empty();
		return;
	}
};


/*******************************************************************************

@class:	logicalexpression_t, implements evaluation of a logical expression of OS
components; based on the availability of the OS components in the current config
logical expressions similar to the one from the following example, is evaluated.

e.g.

	E -> (CE_MODULES_NK & CE_MODULES_FILESYS & (OLE32_DCOMOLE | OLE32_MCOMBASE))

Replacing the OS component name with the availability state in the current
config (fictitious) gives following expression.

	E -> (TRUE & TRUE & (FALSE | TRUE)

The expression evaluates to a TRUE.

@note:	Each logical expression in the oemstress.ini adheres to following formal
specifications:

	x	->	& OR |
	E	->	OS_COMP
	E	->	(E)
	E	->	E x E

Expressions are delimited by newline; Whitespaces in expressions are ignored.

@fault:	None

@pre:	Call logicalexpression_t::Set to initialize

@post:	Call logicalexpression_t::Reset to clean up

*******************************************************************************/

struct logicalexpression_t
{
private:
	static LPCSTR _lpszExpression;	// do not modify the pointer
	static LPCSTR _pStart, _pEnd;	// indicates first and last of the current token in the expression
	static stack_t _TokenStack;
	static queue_t _PostfixQueue;
	static _ceconfig_t *_pConfig;

public:
	static bool Set(IN LPCSTR lpszExpression, _ceconfig_t *pCeConfig)
	{
		if (!lpszExpression || !*lpszExpression || !pCeConfig)
		{
			return false;
		}

		_lpszExpression = lpszExpression;
		_pStart = _pEnd = lpszExpression;
		_pConfig = pCeConfig;

		return true;
	}

	static void Reset()
	{
		_lpszExpression = _pStart = _pEnd = NULL;
		_TokenStack.Empty();
		_PostfixQueue.Empty();
		_pConfig = NULL;
		return;
	}

	static bool Evaluate(OUT TOKEN& result)
	{
		bool fOk = true;

		if (!ConvertToPostfix())
		{
			fOk = false;
			goto done;
		}

		_TokenStack.Empty();

		while (true)
		{
			TOKEN token;

			if (!_PostfixQueue.Delete(token))
			{
				fOk = false;	// error
				break;
			}

			if (TOKEN_ENDEXPR == token)
			{
				// answer is at the top of the stack

				if (!_TokenStack.Pop(result))
				{
					fOk = false;	// error
				}

				break;
			}
			else if (IsTokenOperand(token))
			{
				if (!_TokenStack.Push(token))
				{
					fOk = false;	// error
					break;
				}
			}
			else
			{
				TOKEN operand1, operand2;

				if (!_TokenStack.Pop(operand1) || !_TokenStack.Pop(operand2))
				{
					fOk = false; // error
					break;
				}
				else
				{
					/* token must be an operator: remove correct number of
					operands from the stack, store the result onto the stack */

					TOKEN tmp = EvaluateBasicExpression(operand1, operand2, token);

					if (TOKEN_FAILURE == tmp)
					{
						fOk = false;
						break;
					}

					if (!_TokenStack.Push(tmp))
					{
						fOk = false;
						break;
					}
				}
			}
		}

	done:
		return fOk;
	}


private:
	static TOKEN EvaluateBasicExpression(IN TOKEN operand1, IN TOKEN operand2, IN TOKEN op)
	{
		TOKEN result = TOKEN_FAILURE;

		if (!IsTokenOperand(operand1) || !IsTokenOperand(operand2) || !IsTokenOperator(op))
		{
			result = TOKEN_FAILURE;	// syntax error
			goto done;
		}

		switch (op)
		{
		case TOKEN_OPERATOR_AND:
			result = ((TOKEN_TRUE == operand1) && (TOKEN_TRUE == operand2)) ? TOKEN_TRUE : TOKEN_FALSE;
			break;
		case TOKEN_OPERATOR_OR:
			result = ((TOKEN_TRUE == operand1) || (TOKEN_TRUE == operand2)) ? TOKEN_TRUE : TOKEN_FALSE;
			break;
		default:
			result = TOKEN_FAILURE;	// syntax error
			break;
		}

	done:
		return result;
	}

	static bool ConvertToPostfix()
	{
		while (true)
		{
			TOKEN token = GetNextToken();

			if (TOKEN_ENDEXPR == token)
			{
				TOKEN tmp;

				// empty the stack

				while (_TokenStack.Pop(tmp))
				{
					_PostfixQueue.Add(tmp);
				}

				_PostfixQueue.Add(TOKEN_ENDEXPR);

				return true;
			}
			else if (IsTokenOperand(token))
			{
				_PostfixQueue.Add(token);
			}
			else if (TOKEN_CLOSEPAREN == token)
			{
				// unstack until '('

				while (TOKEN_OPENPAREN != _TokenStack.Top())
				{
					TOKEN tmp;
					_TokenStack.Pop(tmp);
					_PostfixQueue.Add(tmp);
				}

				TOKEN temp;
				_TokenStack.Pop(temp);	// delete '(' from the stack
			}
			else
			{
				/* not dealing with in-stack-priority and in-coming-priority of
				the operators as all of them are logical operators with
				identical precedences */
				_TokenStack.Push(token);
			}
		}

		return false;
	}

	static TOKEN GetNextToken()
	{
		const MAX_BUF = 0xFF;
		char szToken[MAX_BUF];
		TOKEN tokenReturn = TOKEN_ENDEXPR;

		if (_pEnd && *_pEnd)
		{
			// skip leading whitespaces

			while (_pStart && *_pStart && IsWhiteSpace(*_pStart))
			{
				_pStart++;
			}

			// end of the expression

			if (!_pStart || !*_pStart)
			{
				tokenReturn = TOKEN_ENDEXPR;
				memset(szToken, 0, sizeof szToken);
				goto done;
			}

			_pEnd = _pStart;

			// find the end of the token

			while (_pEnd && *_pEnd && !IsOperator(*_pEnd) && !IsParenthesis(*_pEnd) && !IsWhiteSpace(*_pEnd))
			{
				_pEnd++;
			}

			// extract the token

			memset(szToken, 0, sizeof szToken);

			if (_pStart)
			{
				if ((_pStart == _pEnd) && (IsOperator(*_pEnd) || IsParenthesis(*_pEnd)))
				{
					_pEnd++;
				}

				memcpy(szToken, _pStart, (_pEnd - _pStart));

				// todo: strip leading and trailing whitespaces

				// determine the token type (including whether the current component exists in the ceconfig.h)

				tokenReturn = GetTokenType(szToken);
				goto done;
			}

		done:
			_pStart = _pEnd;
		}

		#ifdef	__DUMPDEBUGINFO__
		DumpTokenDetails(szToken, tokenReturn);
		#endif	/* __DUMPDEBUGINFO__ */
		return tokenReturn;
	}

	static bool IsComponentAvailable(LPCSTR lpszComponent)
	{
		return _pConfig->IsComponentAvailable((LPSTR)lpszComponent);
	}

	inline static bool IsOperator(char ch)
	{
		char sz[] = {'&', '|'};

		return (ch == sz[0] || ch == sz[1]);
	}

	inline static bool IsWhiteSpace(char ch)
	{
		char sz[] = {' ', '\t'};

		return (ch == sz[0] || ch == sz[1]);
	}

	inline static bool IsParenthesis(char ch)
	{
		char sz[] = {'(', ')'};

		return (ch == sz[0] || ch == sz[1]);
	}

	inline static bool IsTokenOperand(TOKEN token)
	{
		return ((TOKEN_TRUE == token) || (TOKEN_FALSE == token));
	}

	inline static bool IsTokenOperator(TOKEN token)
	{
		return ((TOKEN_OPERATOR_AND == token) || (TOKEN_OPERATOR_OR == token));
	}

	static TOKEN GetTokenType(LPCSTR lpszToken)
	{
		TOKEN tokenReturn = TOKEN_FAILURE;

		if (!lpszToken || !*lpszToken)
		{
			tokenReturn = TOKEN_FAILURE;
			goto done;
		}

		// determine the token type

		if (1 == strlen(lpszToken))
		{
			switch (*lpszToken)
			{
			case '(':
				tokenReturn = TOKEN_OPENPAREN;
				break;
			case ')':
				tokenReturn = TOKEN_CLOSEPAREN;
				break;
			case '&':
				tokenReturn = TOKEN_OPERATOR_AND;
				break;
			case '|':
				tokenReturn = TOKEN_OPERATOR_OR;
				break;
			default:
				tokenReturn = TOKEN_FAILURE;
				break;
			}
		}
		else
		{
			/* token is a component name: return TOKEN_TRUE if it's in the
			ceconfig.h, TOKEN_FALSE otherwise */

			tokenReturn = IsComponentAvailable(lpszToken) ? TOKEN_TRUE : TOKEN_FALSE;
			goto done;
		}

	done:
		return tokenReturn;
	}

	#ifdef	__DUMPDEBUGINFO__
	static void DumpTokenDetails(LPCSTR lpszToken, TOKEN token)
	{
        LPCSTR rgszTokenType[] = {"TOKEN_FALSE", "TOKEN_TRUE", "TOKEN_OPERATOR_AND",
			"TOKEN_OPERATOR_OR", "TOKEN_OPENPAREN", "TOKEN_CLOSEPAREN", "TOKEN_BEGINEXPR",
			"TOKEN_ENDEXPR", "TOKEN_FAILURE"};

		DebugDump(("Name: %s, Type: %s\r\n"), lpszToken, rgszTokenType[token]);
	}
	#endif	/* __DUMPDEBUGINFO__ */
};



LPCSTR logicalexpression_t::_lpszExpression, logicalexpression_t::_pStart, logicalexpression_t::_pEnd;
stack_t logicalexpression_t::_TokenStack;
queue_t logicalexpression_t::_PostfixQueue;
_ceconfig_t *logicalexpression_t::_pConfig;


_ceconfig_t::_ceconfig_t(IN IFile *pFile, IN bool fByRef)
{
	_pFile = pFile;
	_hMemOSComps = NULL;
	_fCeCfgFileLoaded = false;

	if (_pFile->Open() && LoadCeconfigFile())
	{
		_fCeCfgFileLoaded = true;
	}
	else
	{
		_pFile->Close();
		_fCeCfgFileLoaded = false;
	}

	return;
}


_ceconfig_t::~_ceconfig_t()
{
	if (!_fByRef)
	{
		delete _pFile;
	}

	if (_hMemOSComps)
	{
		LocalFree(_hMemOSComps);
		_hMemOSComps = NULL;
	}

	return;
}


/*

@func:	_ceconfig_t.LoadCeconfigFile, Loads ceconfig file from the specified
path for future component matching

@rdesc:	true if successful, false otherwise

@param:	void

@fault:	None

@pre:	None

@post:	None

@note:	Private function

*/

bool _ceconfig_t::LoadCeconfigFile()
{
	bool fOk = true;

 	if (!_pFile)
	{
		fOk = false;
		goto done;
	}

	memset(_szBuffer, 0, sizeof _szBuffer);

	while (_pFile->Read(_szBuffer, MAX_BUFFER_SIZE))
	{
		if (_hMemOSComps)
		{
			char *psz = (char *)LocalLock(_hMemOSComps);
			INT nLength = strlen(psz) + strlen(_szBuffer) + 1;
			LocalUnlock(_hMemOSComps);

			HLOCAL hMemTmp = LocalReAlloc(_hMemOSComps, nLength, LMEM_MOVEABLE | LMEM_ZEROINIT);

			if (!hMemTmp)
			{
				LocalFree(_hMemOSComps);
				_hMemOSComps = NULL;
				fOk = false;
				goto done;
			}

			_hMemOSComps = hMemTmp;
		}
		else
		{
			INT nLength = strlen(_szBuffer) + 1;

			_hMemOSComps = LocalAlloc(LMEM_ZEROINIT, nLength);

			if (!_hMemOSComps)
			{
				fOk = false;
				goto done;
			}
		}

		char *psz = (char *)LocalLock(_hMemOSComps);
		strcat(psz, _strlwr(_szBuffer));	// store in lowercase
		LocalUnlock(_hMemOSComps);

		memset(_szBuffer, 0, sizeof _szBuffer);
	}

done:
	return fOk;
}


#ifdef DEBUG
void _ceconfig_t::DumpConfigInfo()
{
	const TINY_BUFFER = 0x40;
	wchar_t *pw = NULL;
	char *p = (char *)LocalLock(_hMemOSComps);
	char *pch = p;

	while (true)
	{
		if (pch && *pch)
		{
			wchar_t szw[TINY_BUFFER + 1];
			char szm[TINY_BUFFER + 1];

			memset(szm, 0, sizeof szm);
			memset(szw, 0, sizeof szw);

			strncpy(szm, pch, (strlen(pch) < TINY_BUFFER) ? strlen(pch) : TINY_BUFFER);
			pch += strlen(szm);
			mbstowcs(szw, szm, strlen(szm));
			DebugDump(L"%s", szw);
		}
		else
		{
			break;
		}
	}

	LocalUnlock(_hMemOSComps);
}
#endif /* DEBUG */


/*

@func:	_ceconfig_t.IsRunnable, Decided whether a specific module is runnable
on the current OS config

@rdesc:	true if runnable, false otherwise

@param:	IN LPSTR pszModOSComp: Complete logical expression consisting of the
components the module uses, use the syntax defined in logicalexpression_t header

@fault:	None

@pre:	None

@post:	None

@note:	None

*/

bool _ceconfig_t::IsRunnable(IN LPSTR pszModOSComp)
{
	if (!(_hMemOSComps && pszModOSComp && *pszModOSComp))
	{
		return false;
	}

	bool fRunnable = false;
	TOKEN result = TOKEN_FALSE;

	if (!logicalexpression_t::Set(pszModOSComp, this) || !logicalexpression_t::Evaluate(result))
	{
		DebugDump(L"#### _ceconfig_t::IsRunnable - Error Evaluating Expression ####\r\n");
		fRunnable = false;
		goto cleanup;
	}


	switch (result)
	{
	case TOKEN_TRUE:
		fRunnable = true;
		break;

	case TOKEN_FALSE:
		fRunnable = false;
		break;

	default:
		DebugDump(L"#### _ceconfig_t::IsRunnable - logicalexpression_t::Evaluate returned invalid result ####\r\n");
		fRunnable = false;
		break;
	}

cleanup:
	logicalexpression_t::Reset();

	return fRunnable;
}



/*

@func:	_ceconfig_t.IsComponentAvailable, Decides whether a specific component
is available in the current OS config

@rdesc:	true if available, false otherwise

@param:	IN LPSTR lpszComponent: Name of the component, terminated by NULL

@fault:	None

@pre:	None

@post:	None

@note:	Private function

*/

bool _ceconfig_t::IsComponentAvailable(IN LPSTR lpszComponent)
{
	bool fAvailable = false;

	char *lpszAvailableOSComponents = (char *)LocalLock(_hMemOSComps);

	char *pch = lpszAvailableOSComponents;

	do
	{
		pch = strstr(pch, _strlwr(lpszComponent));

		if (!pch)
		{
			fAvailable = false;
			goto done;
		}

		// double validation: make sure matching entry is a complete word

		if (pch != lpszAvailableOSComponents)	// if not the first one
		{
			// expect a whitespace just before the match

			if ((*(pch - 1) != ' ') && (*(pch - 1) != '\t'))
			{
				fAvailable = false;
				pch += strlen(lpszComponent);
				continue;
			}
		}

		if (*(pch + strlen(lpszComponent)))	// if not the last one
		{
			// expect a whitespace just after the matching word

			if ((*(pch + strlen(lpszComponent)) != ' ') &&
				(*(pch + strlen(lpszComponent)) != '\t'))
			{
				fAvailable = false;
				pch += strlen(lpszComponent);
				continue;
			}
		}

		fAvailable = true;
	}
	while (*pch && !fAvailable);

done:
	LocalUnlock(_hMemOSComps);
	return fAvailable;
}
