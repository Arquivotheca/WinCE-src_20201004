//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
/*
 *		tokens.cpp
 *
 *		The sorted table of strings and token values
 *
 *		Note: if you insert new keywords, be sure to insert the corresponding
 *		i_keyword in the i_xxx enum in tokens.h.  This enum defines the
 *		indices used by RTFWrit to output RTF control words.
 *
 *		Original RichEdit 1.0 RTF converter: Anthony Francisco
 *		Conversion to C++ and RichEdit 2.0:  Murray Sargent
 *
 */
#include "_common.h"
#include "tokens.h"

KEYWORD rgKeyword[] =
{
	{"animtext",	tokenAnimText},
	{"ansi",		tokenCharSetAnsi},
	{"ansicpg",		tokenAnsiCodePage},
	{"b",			tokenBold},
	{"bgbdiag",		tokenBckgrndBckDiag},
	{"bgcross",		tokenBckgrndCross},
	{"bgdcross",	tokenBckgrndDiagCross},
	{"bgdkbdiag",	tokenBckgrndDrkBckDiag},
	{"bgdkcross",	tokenBckgrndDrkCross},
	{"bgdkdcross",	tokenBckgrndDrkDiagCross},
	{"bgdkfdiag",	tokenBckgrndDrkFwdDiag},
	{"bgdkhoriz",	tokenBckgrndDrkHoriz},
	{"bgdkvert",	tokenBckgrndDrkVert},
	{"bgfdiag",		tokenBckgrndFwdDiag},
	{"bghoriz",		tokenBckgrndHoriz},
	{"bgvert",		tokenBckgrndVert},
	{"bin",			tokenBinaryData},
	{"blue",		tokenColorBlue},
	{"box",			tokenBox},
	{"brdrb",		tokenBorderBottom},
	{"brdrbar",		tokenBorderOutside},
	{"brdrbtw",		tokenBorderBetween},
	{"brdrcf",		tokenBorderColor},
	{"brdrdash",	tokenBorderDash},
	{"brdrdashsm",	tokenBorderDashSmall},
	{"brdrdb",		tokenBorderDouble},
	{"brdrdot",		tokenBorderDot},
	{"brdrhair",	tokenBorderHairline},
	{"brdrl",		tokenBorderLeft},
	{"brdrr",		tokenBorderRight},
	{"brdrs",		tokenBorderSingleThick},
	{"brdrsh",		tokenBorderShadow},
	{"brdrt",		tokenBorderTop},
	{"brdrth",		tokenBorderDoubleThick},
	{"brdrtriple",	tokenBorderTriple},
	{"brdrw",		tokenBorderWidth},
	{"brsp",		tokenBorderSpace},
	{"bullet",		BULLET},
	{"caps",		tokenCaps},
	{"cbpat",		tokenColorBckgrndPat},
	{"cell",		tokenCell},
	{"cellx",		tokenCellX},
	{"cf",			tokenColorForeground},
	{"cfpat",		tokenColorForgrndPat},
	{"clbrdrb",		tokenCellBorderBottom},
	{"clbrdrl",		tokenCellBorderLeft},
	{"clbrdrr",		tokenCellBorderRight},
	{"clbrdrt",		tokenCellBorderTop},
	{"collapsed",	tokenCollapsed},
	{"colortbl",	tokenColorTable},
	{"cpg",			tokenCodePage},
	{"cs",			tokenCharStyle},
	{"deff",		tokenDefaultFont},
	{"deflang",		tokenDefaultLanguage},
	{"deflangfe",	tokenDefaultLanguageFE},
	{"deftab",		tokenDefaultTabWidth},
	{"deleted",		tokenDeleted},
	{"dibitmap",	tokenPictureWindowsDIB},
	{"disabled",	tokenDisabled},
	{"dn",			tokenDown},
	{"embo",		tokenEmboss},
#if 1
	{"emdash",		'-'},
	{"emspace",		' '},
	{"endash",		'-'},
	{"enspace",		' '},
#else
	// FUTURE(BradO):  It turns out that we can't reliably
	//	display these Unicode characters for any particular
	//	font applied to them.  If we choose to do something
	//	more intelligent to ensure that these special chars
	//	are displayed regardless of the font applied, then
	//	we should re-enable this code (see Bug #3179).

	{"emdash",		EMDASH},
	{"emspace",		EMSPACE},
	{"endash",		ENDASH},
	{"enspace",		ENSPACE},
#endif
	{"expndtw",		tokenExpand},
	{"f",			tokenFontSelect},
	{"fbidi",		tokenFontFamilyBidi},
	{"fchars",		tokenFollowingPunct},
	{"fcharset",	tokenCharSet},
	{"fdecor",		tokenFontFamilyDecorative},
	{"fi",			tokenIndentFirst},
	{"field",		tokenField},
	{"fldinst",		tokenFieldInstruction},
	{"fldrslt",		tokenFieldResult},
	{"fmodern",		tokenFontFamilyModern},
	{"fname",		tokenRealFontName},
	{"fnil",		tokenFontFamilyDefault},
	{"fonttbl",		tokenFontTable},
	{"footer",		tokenNullDestination},
	{"footerf",		tokenNullDestination},
	{"footerl",		tokenNullDestination},
	{"footerr",		tokenNullDestination},
	{"footnote",	tokenNullDestination},
	{"fprq",		tokenPitch},
	{"froman",		tokenFontFamilyRoman},
	{"fs",			tokenFontSize},
	{"fscript",		tokenFontFamilyScript},
	{"fswiss",		tokenFontFamilySwiss},
	{"ftech",		tokenFontFamilyTechnical},
	{"ftncn",		tokenNullDestination},
	{"ftnsep",		tokenNullDestination},
	{"ftnsepc",		tokenNullDestination},
	{"green",		tokenColorGreen},
	{"header",		tokenNullDestination},
	{"headerf",		tokenNullDestination},
	{"headerl",		tokenNullDestination},
	{"headerr",		tokenNullDestination},
	{"highlight",	tokenColorBackground},
	{"hyphpar",		tokenHyphPar},
	{"i",			tokenItalic},
	{"impr",		tokenImprint},
	{"info",		tokenDocumentArea},
	{"intbl",		tokenInTable},
	{"keep",		tokenKeep},
	{"keepn",		tokenKeepNext},
	{"kerning",		tokenKerning},
	{"lang",		tokenLanguage},
	{"lchars",		tokenLeadingPunct},
	{"ldblquote",	LDBLQUOTE},
	{"li",			tokenIndentLeft},
	{"line",		tokenLineBreak},
	{"lnkd",		tokenLink},
	{"lquote",		LQUOTE},
	{"ltrch",		tokenLToRChars},
	{"ltrdoc",		tokenLToRDocument},
	{"ltrmark",		LTRMARK},
	{"ltrpar",		tokenLToRPara},
	{"macpict",		tokenPictureQuickDraw},
	{"noline",		tokenNoLineNumber},
	{"nosupersub",	tokenNoSuperSub},
	{"nowidctlpar", tokenNoWidCtlPar},  
	{"objattph",	tokenObjectPlaceholder},
	{"objautlink",	tokenObjectAutoLink},
	{"objclass",	tokenObjectClass},
	{"objcropb",	tokenCropBottom},
	{"objcropl",	tokenCropLeft},
	{"objcropr",	tokenCropRight},
	{"objcropt",	tokenCropTop},
	{"objdata",		tokenObjectData},
	{"object",		tokenObject},
	{"objemb",		tokenObjectEmbedded},
	{"objh",		tokenHeight},
	{"objicemb",	tokenObjectMacICEmbedder},
	{"objlink",		tokenObjectLink},
	{"objname",		tokenObjectName},
	{"objpub",		tokenObjectMacPublisher},
	{"objscalex",	tokenScaleX},
	{"objscaley",	tokenScaleY},
	{"objsetsize",	tokenObjectSetSize},
	{"objsub",		tokenObjectMacSubscriber},
	{"objw",		tokenWidth},
	{"outl",		tokenOutline},
	{"page",		FF},
	{"pagebb",		tokenPageBreakBefore},
	{"par",			tokenEndParagraph},
	{"pard",		tokenParagraphDefault},
	{"piccropb",	tokenCropBottom},
	{"piccropl",	tokenCropLeft},
	{"piccropr",	tokenCropRight},
	{"piccropt",	tokenCropTop},
	{"pich",		tokenHeight},
	{"pichgoal",	tokenDesiredHeight},
	{"picscalex",	tokenScaleX},
	{"picscaley",	tokenScaleY},
	{"pict",		tokenPicture},
	{"picw",		tokenWidth},
	{"picwgoal",	tokenDesiredWidth},
	{"plain",		tokenCharacterDefault},
	{"pmmetafile",	tokenPictureOS2Metafile},
	{"pn",			tokenParaNum},
	{"pnaiueo",     tokenParaNumKatakanaAIUEOdbl}, // GuyBark JupiterJ: added this - MAPS to dbl
	{"pnaiueod",    tokenParaNumKatakanaAIUEOdbl}, // GuyBark JupiterJ: added this
	{"pndec",		tokenParaNumDecimal},
	{"pnindent",	tokenParaNumIndent},
	{"pniroha",     tokenParaNumKatakanaIROHAdbl}, // GuyBark JupiterJ: added this - MAPS to dbl
	{"pnirohad",    tokenParaNumKatakanaIROHAdbl}, // GuyBark JupiterJ: added this
	{"pnlcltr",		tokenParaNumLCLetter},
	{"pnlcrm",		tokenParaNumLCRoman},
	{"pnlvlblt",	tokenParaNumBullet},
	{"pnlvlbody",	tokenParaNumBody},
	{"pnlvlcont",	tokenParaNumCont},
	{"pnstart",		tokenParaNumStart},
	{"pntext",		tokenParaNumText},
	{"pntxta",		tokenParaNumAfter},
	{"pntxtb",		tokenParaNumBefore},
	{"pnucltr",		tokenParaNumUCLetter},
	{"pnucrm",		tokenParaNumUCRoman},
	{"protect",		tokenProtect},
	{"pwd",			tokenPocketWord},
	{"qc",			tokenAlignCenter},
	{"qj",			tokenAlignJustify},
	{"ql",			tokenAlignLeft},
	{"qr",			tokenAlignRight},
	{"rdblquote",	RDBLQUOTE},
	{"red",			tokenColorRed},
	{"result",		tokenObjectResult},
	{"revauth",		tokenRevAuthor},
	{"revised",		tokenRevised},
	{"ri",			tokenIndentRight},
	{"row",			tokenRow},
	{"rquote",		RQUOTE},
	{"rtf",			tokenRtf},
	{"rtlch",		tokenRToLChars},
	{"rtldoc",		tokenRToLDocument},
	{"rtlmark",		RTLMARK},
	{"rtlpar",		tokenRToLPara},
	{"s",			tokenStyle},
 	{"sa",			tokenSpaceAfter},
	{"sb",			tokenSpaceBefore},
	{"sbys",		tokenSideBySide},
	{"scaps",		tokenSmallCaps},
	{"sect",		tokenEndSection},
	{"sectd",		tokenSectionDefault},
	{"shad",		tokenShadow},
	{"shading",		tokenShading},
	{"sl",			tokenLineSpacing},
	{"slmult",		tokenLineSpacingRule},
	{"strike",		tokenStrikeOut},
	{"stylesheet",	tokenStyleSheet},
	{"sub",			tokenSubscript},
	{"super",		tokenSuperscript},
	{"tab",			9},
	{"tb",			tokenTabBar},
	{"tc",			tokenNullDestination},
	{"tldot",		tokenTabLeaderDots},
	{"tleq",		tokenTabLeaderEqual},
	{"tlhyph",		tokenTabLeaderHyphen},
	{"tlth",		tokenTabLeaderThick},
	{"tlul",		tokenTabLeaderUnderline},
	{"tqc",			tokenCenterTab},
	{"tqdec",		tokenDecimalTab},
	{"tqr",			tokenFlushRightTab},
	{"trbrdrb",		tokenBorderBottom},
	{"trbrdrl",		tokenBorderLeft},
	{"trbrdrr",		tokenBorderRight},
	{"trbrdrt",		tokenBorderTop},
	{"trgaph",		tokenCellHalfGap},
	{"trleft",		tokenRowLeft},
	{"trowd",		tokenRowDefault},
	{"trqc",		tokenRowAlignCenter},
	{"trqr",		tokenRowAlignRight},
	{"tx",			tokenTabPosition},
	{"u",			tokenUnicode},
	{"uc",			tokenUnicodeCharByteCount},
	{"ul",			tokenUnderline},
	{"uld",			tokenUnderlineDotted},
	{"uldash",		tokenUnderlineDash},
	{"uldashd",		tokenUnderlineDashDotted},
	{"uldashdd",	tokenUnderlineDashDotDotted},
	{"uldb",		tokenUnderlineDouble},
	{"ulhair",		tokenUnderlineHairline},
	{"ulnone",		tokenStopUnderline},
	{"ulth",		tokenUnderlineThick},
	{"ulw",			tokenUnderlineWord},
	{"ulwave",		tokenUnderlineWave},
	{"up",			tokenUp},
	{"utf",			tokenUTF},
	{"v",			tokenHiddenText},
	{"viewkind",	tokenViewKind},
	{"viewscale",	tokenViewScale},
	{"wbitmap",		tokenPictureWindowsBitmap},
	{"wbmbitspixel",tokenBitmapBitsPerPixel},
	{"wbmplanes",	tokenBitmapNumPlanes},
	{"wbmwidthbytes",tokenBitmapWidthBytes},
	{"wmetafile",	tokenPictureWindowsMetafile},
	{"xe",			tokenNullDestination},
	{"zwj",			ZWJ},
	{"zwnj",		ZWNJ}
};

INT cKeywords = sizeof(rgKeyword) / sizeof(rgKeyword[0]);

extern const BYTE szSymbolKeywords[];
extern const TOKEN tokenSymbol[];

const BYTE szSymbolKeywords[] = "*:{}\\_|\r\n-~";

const TOKEN tokenSymbol[] =				// Keep in same order as szSymbolKeywords
{
	tokenOptionalDestination,		// *
	tokenIndexSubentry,				// :
	'{',							// {
	'}',							// }
	'\\',							// BSLASH
	'-',							// _ (nonbreaking hyphen; should be 0x2011)
	tokenFormulaCharacter,			// |
	tokenEndParagraph,				// CR
	tokenEndParagraph,				// LF
#if 1
	'-',							// - (optional hyphen)
	' '								// ~ (nonbreaking space)
#else
	// FUTURE(BradO):  It turns out that we can't reliably
	//	display these Unicode characters for any particular
	//	font applied to them.  If we choose to do something
	//	more intelligent to ensure that these special chars
	//	are displayed regardless of the font applied, then
	//	we should re-enable this code (see Bug #3179).

	0xad,							// - (optional hyphen)
	0xa0							// ~ (nonbreaking space)
#endif
};


