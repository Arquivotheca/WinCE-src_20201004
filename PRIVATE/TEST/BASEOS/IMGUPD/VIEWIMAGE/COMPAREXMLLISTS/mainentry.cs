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
using System.Collections;
using System.Xml;
using XslTransforms;

namespace CompareXmlLists
{
	/// <summary>
	/// Summary description for MainEntry.
	/// </summary>
	class MainEntry
	{
		/// <summary>
		/// The main entry point for the application.
		/// </summary>
		[STAThread]
		static void Main(string[] args)
		{
            try {
                Options options = parseOptions(args);
                validateOptions(options);

                Console.Out.WriteLine(options);
                Console.Out.WriteLine();

                DateTime start = DateTime.Now;
                compare(options);
                DateTime finish = DateTime.Now;
                TimeSpan duration = finish - start;

                Console.Out.WriteLine("\tduration: " + duration);
            } catch (InvalidArg e) {
                Console.Error.WriteLine(e.Message);
                Console.Error.WriteLine(Usage);
            } catch (XmlException e) {
                Console.Error.WriteLine(e.Message);
            } catch (System.Xml.XPath.XPathException e) {
                Console.Error.WriteLine(e.Message);
            }
		}

        private static void compare(Options options) {
            XmlDocument doc1 = new XmlDocument();
            XmlDocument doc2 = new XmlDocument();
            XmlReader reader;

            reader = new XmlTextReader(options.f1);
            doc1.Load(reader);

            reader = new XmlTextReader(options.f2);
            doc2.Load(reader);
            reader = null;

            SortedLookup lookup = new SortedLookup();
            lookup.LookupPath = options.p1;
            lookup.SortPath = options.sortPath;

            XmlNodeList nodes1 = lookup.PerformSortedLookup(doc1);
            XmlNodeList nodes2 = lookup.PerformSortedLookup(doc2);

            ICompareStrategy compareStrategy = new NameCompareStrategy(); //$ HARD-CODED
            ArrayList nodeVisitors = new ArrayList();
            if ( options.performList )
                nodeVisitors.Add(new NodeLister(options.f1, options.f2));
            //$ if ( options.performDiff )
            //$     nodeVisitors.Add(new NodeDiffer(options.f1, options.f2));

            foreach (object v in nodeVisitors) {
                NodesVisitor visitor = (NodesVisitor)v;
                applyVisitors(visitor, nodes1, nodes2, compareStrategy);
            }
        }

        private static void applyVisitors(NodesVisitor visitor,
                                          XmlNodeList nodes1, 
                                          XmlNodeList nodes2,
                                          ICompareStrategy compareStrategy) 
        {
            ArrayList leftNodes = new ArrayList();
            ArrayList leftKeys = new ArrayList();
            ArrayList rightNodes = new ArrayList();
            ArrayList rightKeys = new ArrayList();

            foreach (XmlNode node in nodes1) {
                string key = compareStrategy.GetSortValue(node);
                leftNodes.Add(node);
                leftKeys.Add(key);
            }

            foreach (XmlNode node in nodes2) {
                string key = compareStrategy.GetSortValue(node);
                rightNodes.Add(node);
                rightKeys.Add(key);
            }

            int idxLeft = 0; 
            int idxRight = 0;

            visitor.Initialize();

            while ( idxLeft < leftKeys.Count || idxRight < rightKeys.Count ) {
                string leftKey, rightKey;
                XmlNode leftNode = null, rightNode = null;

                if ( idxLeft < leftKeys.Count ) {
                    if ( idxRight < rightKeys.Count ) {
                        leftKey = (string)leftKeys[idxLeft];
                        leftNode = (XmlNode)leftNodes[idxLeft];
                        rightKey = (string)rightKeys[idxRight];
                        rightNode = (XmlNode)rightNodes[idxRight];

                        if ( leftKey.CompareTo(rightKey) < 0 ) {
                            rightKey = "";
                            rightNode = null;
                            ++idxLeft;
                        } else if ( leftKey.CompareTo(rightKey) > 0 ) {
                            leftKey = "";
                            leftNode = null;
                            ++idxRight;
                        } else {
                            ++idxLeft;
                            ++idxRight;
                        }
                    } else {
                        leftKey = (string)leftKeys[idxLeft];
                        leftNode = (XmlNode)leftNodes[idxLeft];
                        rightKey = "";
                        rightNode = null;
                        ++idxLeft;
                    }
                } else {
                    leftKey = "";
                    leftNode = null;
                    rightKey = (string)rightKeys[idxRight];
                    rightNode = (XmlNode)rightNodes[idxRight];
                    ++idxRight;
                }

                visitor.visitNodes(leftNode, leftKey, rightNode, rightKey);
            }

            visitor.Uninitialize();
        }

        class Options 
        {
            public string f1;
            public string p1;
            public string f2;
            public string p2;
            public string sortPath;
            public bool   performList;
            public bool   performDiff;

            internal Options() {
                sortPath = "name";
                performList = false;
                performDiff = false;
            }

            public override string ToString() {
                return 
                    "\tf1=" + f1
                    + "\n\tf2=" + f2
                    + "\n\tp1=" + p1
                    + "\n\tp2=" + p2
                    + "\n\tsortPath=" + sortPath
                    + "\n\tperformList=" + performList
                    + "\n\tperformDiff=" + performDiff
                    ;
            }

        }

        class InvalidArg : Exception
        {
            public InvalidArg(string s)
                : base(s)
            {
            }
        }

        private static Options parseOptions(string[] args) 
        {
            Options a = new Options();

            for (int idx = 0; idx < args.Length; idx++ ) 
            {
                string arg = args[idx];

                if ( arg.Equals("-f1") ) {
                    if ( idx == args.Length - 1)
                        throw new InvalidArg("-f1 option was incomplete");
                    a.f1 = args[++idx];
                } else if ( arg.Equals("-f2") ) {
                    if ( idx == args.Length - 1)
                        throw new InvalidArg("-f2 option was incomplete");
                    a.f2 = args[++idx];
                } else if ( arg.Equals("-p1") ) {
                    if ( idx == args.Length - 1)
                        throw new InvalidArg("-p1 option was incomplete");
                    a.p1 = args[++idx];
                } else if ( arg.Equals("-p2") ) {
                    if ( idx == args.Length - 1)
                        throw new InvalidArg("-p2 option was incomplete");
                    a.p2 = args[++idx];
                } else if ( arg.Equals("-sort") ) {
                    if ( idx == args.Length - 1)
                        throw new InvalidArg("-p2 option was incomplete");
                    a.sortPath = args[++idx];
                } else if ( arg.Equals("-list") ) {
                    a.performList = true;
                } else if ( arg.Equals("-diff") ) {
                    a.performDiff = true;
                } else {
                    throw new InvalidArg("Unrecognized option: '" + arg + "'");
                }
            }

            if ( a.p2 == null || a.p2.Length == 0 )
                a.p2 = a.p1;

            return a;
        }

        private static void validateOptions(Options options) {
            if ( options.f1 == null || options.f1.Length == 0 )
                throw new InvalidArg("-f1 must be specified");
            if ( options.p1 == null || options.p1.Length == 0 )
                throw new InvalidArg("-p1 must be specified");
            if ( options.f2 == null || options.f2.Length == 0 )
                throw new InvalidArg("-f2 must be specified");
            if ( !options.performDiff && !options.performList )
                throw new InvalidArg("Either -list or -diff must be specified");
        }

        public static readonly string Usage = 
@"CompareXmlLists -f1 <file1> -p1 <path1> -f2 <file2> [-p2 <path2>]
    -f1     the first file
    -p1     an XPath to f1's elements (e.g., ""//modules/module"")
    -f2     the second file
    -p2     an XPath to f2's elements; uses -p1 if -p2 is not provided";
	}
}
