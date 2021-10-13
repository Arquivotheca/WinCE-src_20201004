//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

using System;
using System.Data;
using System.Drawing;
using System.Collections;
using System.Windows.Forms;
using System.Text.RegularExpressions;
using Win32 = Microsoft.WindowsCE.Win32API;
using DATK = Microsoft.WindowsCE.Datk;
using System.Threading;


namespace SimplestSample
{
	/// <summary>
	/// Summary description for SimplestSampleTest.
	/// </summary>
	class SimplestSampleTest
	{

		/// <summary>
		/// ShowSite().
		/// </summary>
		static void ShowSite(string testURL, string pageTitle)
		{
			// launch app
			DATK.Application ieSample = new DATK.Application("iesample.exe");
			ieSample.Launch();
			
			// find the Address box, in two or three steps
			// The Address box is a text box in a combo box in a ReBarWindow in the IE window. Whew.
			DATK.WindowFinderEx myWindowFinder = new DATK.WindowFinderEx(DATK.WindowFinderEx.CurrentActiveForm);
			myWindowFinder.AddCriterion(new DATK.ClassNameMatcher("ReBarWindow32"));
			DATK.Form rebar = myWindowFinder.FindForm();
			
			myWindowFinder = new DATK.WindowFinderEx(rebar);
			myWindowFinder.AddCriterion(new DATK.ClassNameMatcher("combobox"));
			Microsoft.WindowsCE.Datk.Form combobox = myWindowFinder.FindForm();
			
			myWindowFinder = new DATK.WindowFinderEx(combobox);
			myWindowFinder.AddCriterion(new DATK.ClassNameMatcher("Edit"));
			DATK.TextBox editBox = new DATK.TextBox(myWindowFinder.FindControl());
			
			editBox.Click();
			DATK.KeyBoard.SendKey(testURL);
			DATK.KeyBoard.SendKey(DATK.KeyBoard.Key.Enter);
			
			if (pageTitle != DATK.WindowFinderEx.CurrentActiveForm.Text)
			{
				throw new Exception("The sample failed to navigate to MSDN.");
			}
		}
		

		/// <summary>
		/// OpenSystemControlPanel.
		/// </summary>
		static void OpenSystemControlPanel()
		{

			DATK.KeyBoard.SendKey(DATK.KeyBoard.Key.Tab);
			DATK.KeyBoard.SendKey(DATK.KeyBoard.Key.Enter);
			Thread.Sleep(5000);
			
			DATK.ContextMenu cm = new DATK.ContextMenu();
			//DATK.MenuItem mi = cm.FindMenuItemByText("%Settings");
			DATK.MenuItem mi = new DATK.MenuItem(cm, 3);
			mi.Click();

			DATK.KeyBoard.SendKey(DATK.KeyBoard.Key.Down);
			DATK.KeyBoard.SendKey(DATK.KeyBoard.Key.Enter);
			Thread.Sleep(5000);
			
			DATK.WindowFinderEx myWindowFinder = new DATK.WindowFinderEx(DATK.WindowFinderEx.CurrentActiveForm);
			myWindowFinder.AddCriterion(new DATK.ClassNameMatcher("CONTROLEXE_MAIN"));
			DATK.Form controlpanel = myWindowFinder.FindForm();
			
			myWindowFinder = new DATK.WindowFinderEx(controlpanel);
			myWindowFinder.AddCriterion(new DATK.TextMatcher("Control Panel"));
			DATK.ListView controlPanelListView = new DATK.ListView(myWindowFinder.FindControl());

			DATK.ListViewItem systemItem = controlPanelListView.GetItem(controlPanelListView.FindString("System"));

			systemItem.DoubleClick();
			
		}
		
		
		
		/// <summary>
		/// The main entry point for the application.
		/// </summary>
		static void Main(string[] args)
		{	
			try
			{	
				OpenSystemControlPanel();

				ShowSite("http://msdn.microsoft.com/", "MSDN Home Page - Internet Explorer");
				
			}
			catch (DATK.DatkException de)
			{
				MessageBox.Show("Test failed, raising DATK exception: " + de.Message);
			}
			catch (Exception e)
			{
				MessageBox.Show("Test failed, raising exception: " + e.Message);
			}
			finally
			{
			}
		}		
		
	}
}
