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

namespace CompareXmlLists
{
	/// <summary>
	/// 
	/// </summary>
	public class NodeLister : CompareXmlLists.NodesVisitor
	{
        readonly string leftFile;
        readonly string rightFile;

        public NodeLister(string leftFile, string rightFile) {
            this.leftFile = leftFile;
            this.rightFile = rightFile;
        }

        public override void Initialize() {
            base.Initialize ();

            WriteOutputLine("left: " + leftFile, "right: " + rightFile);
            WriteOutputLine("----------------------------------------",
                            "----------------------------------------");
        }

        public override void visitNodes(System.Xml.XmlNode leftNode, string leftSortName, System.Xml.XmlNode rightNode, string rightSortName) {
            WriteOutputLine(leftSortName, rightSortName);
        }

        private static void WriteOutputLine(string left, string right) {
            Console.Out.WriteLine("{0,40} - {1,-40}", left, right);
        }

    }
}
