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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
using System;
using System.IO;
using System.Text;
using System.Xml;
using System.Xml.Xsl;

namespace XslTransforms
{
	/// <summary>
	/// Summary description for SortedLookup.
	/// </summary>
	public class SortedLookup
	{
        public string LookupPath;
        public string SortPath;

        public XmlNodeList PerformSortedLookup(XmlDocument doc) {
            XslTransform xslt = new XslTransform();
            string xsltString = GetTransformString();
            TextReader textReader = new StringReader(xsltString);
            XmlReader reader = new XmlTextReader(textReader);
            xslt.Load(reader, null, null);

            XsltArgumentList arglist = new XsltArgumentList();
            arglist.AddParam("search-path", "", LookupPath);
            arglist.AddParam("sort-path", "", SortPath);
            XmlReader resultReader = xslt.Transform(doc, arglist, (XmlResolver)null);
            XmlDocument resultDoc = new XmlDocument();
            resultDoc.Load(resultReader);
            return resultDoc.SelectNodes("/myroot/*"); // return just the children
        }

        private string GetTransformString() {
            // couldn't get params to work, so replace
            // it here ($search-path works with msxsml,
            // $sort-path does not)
            StringBuilder sb = new StringBuilder(Transform);
            sb.Replace("$search-path", LookupPath);
            sb.Replace("$sort-path", SortPath);

            return sb.ToString();
        }

        private static string Transform = 
@"
<xsl:transform version=""1.0""
         xmlns:xsl=""http://www.w3.org/1999/XSL/Transform"">


<xsl:output method=""xml"" indent=""yes"" encoding=""utf-8""/>

<xsl:template match=""/"">
    <xsl:element name=""myroot"">
        <xsl:for-each select=""$search-path"">
            <xsl:sort select=""$sort-path"" />
                <xsl:element name=""{name()}"">
                    <xsl:apply-templates select=""@*|node()"" />
                </xsl:element>
        </xsl:for-each>
    </xsl:element>
</xsl:template>

<!-- this is an identity copy: -->
<xsl:template match=""@*|node()"">
    <xsl:copy>
        <xsl:apply-templates select=""@*|node()""/>
    </xsl:copy>
</xsl:template>


</xsl:transform>
";
	}
}
