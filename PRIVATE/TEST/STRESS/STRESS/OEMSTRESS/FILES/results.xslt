<?xml version="1.0" encoding="UTF-8" ?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
	<xsl:template match="/">
		<html>
			<head>
				<title>OEMStress Results</title>
			</head>
			<body>
				<h1>OEMStress Results<hr></hr></h1>
				<xsl:choose>
					<xsl:when test="result/error/no_runnable_modules">
						<!--	No Runnable Modules Begin	-->
						<table border="0" cellSpacing="0" cellPadding="0" width="100%">
							<tr bgcolor="#FFFFBB">
								<font face="Verdana" color="#FF0000" size="+2">
									<b>ERROR: None of the available modules can run on current platform.</b>
								</font> 
							</tr>
						</table>
						<!--	No Runnable Modules End	-->
					</xsl:when>
					<xsl:otherwise>
						<xsl:if test="result/error/missing_files">
							<!--	Missing Helper Files Begin	-->
							<table border="0" cellSpacing="0" cellPadding="0" width="100%">
								<tr bgcolor="#FFFFBB">
									<font face="Verdana" color="#FF0000" size="+2">
										<b>ERROR: Some of the runnable modules need helper file(s).</b>
									</font> 
								</tr>
							</table>					
							<!--	Missing Helper Files End	-->
						</xsl:if>
					</xsl:otherwise>
				</xsl:choose>
				
				<font face="Verdana">
					<!--    Missing Files Begin	-->
					<xsl:if test="result/error/missing_files">
						<table border="0" cellSpacing="2" cellPadding="2">
							<br></br>
							<font color="#ff0000" size="+1">Following files must be present at the device root: </font>
							<font color="#000000" size="+1">
								<xsl:for-each select="result/error/missing_files/filename">
									<xsl:apply-templates/>;
								</xsl:for-each>
							</font>
						</table>
					</xsl:if>
					<!--    Missing Files End	-->
				</font>
				
				<!--	Completed Message	-->
				<xsl:if test="result/completed">
					<table border="0" cellSpacing="0" cellPadding="0" width="100%">
						<tr bgcolor="#FFFFBB">
							<font face="Verdana" color="#007f00" size="+2">
								<b>OEMStress was completed successfully.</b>
							</font> 
						</tr>
					</table>
				</xsl:if>
				
				<!--	Harness Runtime	-->
				<xsl:if test="result/runtime">
					<h3><font face="Verdana" color="#7F7F7F">OEMStress has run for 

					<!--	Hour Value	-->					
					<xsl:value-of select="floor(/result/runtime div 3600)" /> hour(s),
					
					<!--	Minute Value	-->					
					<xsl:value-of select="floor(round(/result/runtime mod 3600) div 60)" /> minute(s),
					
					<!--	Second Value	-->					
					<xsl:value-of select="floor(round(/result/runtime mod 3600) mod 60) "/> second(s).
					</font></h3>
				</xsl:if>
				
				<!--	Hang	-->
				<xsl:if test="result/hangstatus">
					<h4><font face="Verdana" color="#ff0000">
						OEMStress has detected module hang. <xsl:if test="result/hangstatus/hung_module">Please refer to the <i>Hang Status</i> table for more details.</xsl:if>
					</font></h4>			
				</xsl:if>
				
				<!--	MemTracking Threshold Hit	-->
				<xsl:if test="result/memtrack_threshold_hit">
					<xsl:choose>
						<xsl:when test="result/params/usage_percent">
							<h4> <font face="Verdana" color="#ff0000">
								Total memory usage has exceeded <xsl:value-of select="result/params/memtrack" />%; Please refer to the <i>System Health</i> table for more details.
							</font></h4>
						</xsl:when>
						<xsl:otherwise>
							<h4> <font face="Verdana" color="#ff0000">
								Available physical memory is less then <xsl:value-of select="round(result/params/memtrack div 1024)" /> KB; Please refer to the <i>System Health</i> table for more details.
							</font></h4>						
						</xsl:otherwise>
					</xsl:choose>
				</xsl:if>
				
				<br></br>
				<!--    Formatting table for Launch Parameters, System Details, System Health & Hang Status -->
				<table border="0" cellSpacing="2" cellPadding="8">
					<tr>
						<td align="left" valign="top">
							<font face="Verdana">
								<!--    Launch Parameters -->
								<table border="0" cellSpacing="2" cellPadding="2">
									<tr bgcolor="#000000">
										<th align="center" colspan="2">
											<font color="#ffffff" size="+1">Launch Parameters</font>
										</th>
									</tr>
									<tr bgcolor="#E3E1E3">
										<td>
											<font size="-1">Debugger Attached</font>
										</td>
										<td>
											<font size="-1">
												<xsl:value-of select="result/params/debugger" />
											</font>
										</td>
									</tr>
									<tr bgcolor="#E3E1E3">
										<td>
											<font size="-1">Hang Detectection Delay</font>
										</td>
										<td>
											<font size="-1">
												<xsl:choose>
													<xsl:when test="(normalize-space(result/params/hang_time))">
													<xsl:value-of select="result/params/hang_time" /> minute(s)
													</xsl:when>
													<xsl:otherwise>
														<b>Hang Detection Disabled</b>
													</xsl:otherwise>
												</xsl:choose>
											</font>
										</td>
									</tr>
									<xsl:if test="(normalize-space(result/params/hang_time))">
										<tr bgcolor="#E3E1E3">
											<td>
												<font size="-1">Hang Detection Option</font>
											</td>
											<td>
												<font size="-1">
													<xsl:value-of select="result/params/hang_option" />
												</font>
											</td>
										</tr>
									</xsl:if>
									<tr bgcolor="#E3E1E3">
										<td>
											<font size="-1">Requested Run Time</font>
										</td>
										<td>
											<font size="-1">
												<xsl:choose>
													<xsl:when test="(normalize-space(result/params/total_runtime))">
													<xsl:value-of select="result/params/total_runtime" /> minute(s)
													</xsl:when>
													<xsl:otherwise>
														<b>FOREVER</b>
													</xsl:otherwise>
												</xsl:choose>
											</font>
										</td>
									</tr>
									<tr bgcolor="#E3E1E3">
										<td>
											<font size="-1">Wait for Module Completion</font>
										</td>
										<td>
											<font size="-1">
												<xsl:value-of select="result/params/wait_modcompletion" /> 
											</font>
										</td>
									</tr>	
									<tr bgcolor="#E3E1E3">
										<td>
											<font size="-1">Result Refresh Interval</font>
										</td>
										<td>
											<font size="-1">
												<xsl:value-of select="result/params/refresh_interval" /> minute(s)
											</font>
										</td>
									</tr>
									<tr bgcolor="#E3E1E3">
										<td>
											<font size="-1">Module Launch Duration</font>
										</td>
										<td>
											<font size="-1">
												<xsl:value-of select="result/params/module_duration" />
												<xsl:choose>
													<xsl:when test="(normalize-space(result/params/module_duration) = 'RANDOM')"></xsl:when>
													<xsl:when test="(normalize-space(result/params/module_duration) = 'MINIMUM')"></xsl:when>
													<xsl:when test="(normalize-space(result/params/module_duration) = 'INDEFINITE')"></xsl:when>
													<xsl:otherwise> minute(s)</xsl:otherwise>
												</xsl:choose>
											</font>
										</td>
									</tr>
									<tr bgcolor="#E3E1E3">
										<td>
											<font size="-1">Maximum Running Modules</font>
										</td>
										<td>
											<font size="-1">
												<xsl:value-of select="result/params/module_count" />
											</font>
										</td>
									</tr>
									<tr bgcolor="#E3E1E3">
										<td>
											<font size="-1">Random Seed</font>
										</td>
										<td>
											<font size="-1">
												<xsl:value-of select="result/params/random_seed" />
											</font>
										</td>
									</tr>
									<tr bgcolor="#E3E1E3">
										<td>
											<font size="-1">Auto OOM</font>
										</td>
										<td>
											<font size="-1">
												<xsl:value-of select="result/params/autooom" />
											</font>
										</td>
									</tr>
									<tr bgcolor="#E3E1E3">
										<td>
											<font size="-1">Memory Tracking Threshold</font>
										</td>
										<td>
										<xsl:choose>
											<xsl:when test="round(result/params/memtrack)">
												<font size="-1">
													<xsl:choose>
														<xsl:when test="result/params/usage_percent">
															When memory usage exceeds <xsl:value-of select="result/params/memtrack" />%
														</xsl:when>
														<xsl:otherwise>
															When available memory is less then <xsl:value-of select="result/params/memtrack" /> bytes
														</xsl:otherwise>
													</xsl:choose>
												</font>
											</xsl:when>
											<xsl:otherwise>
												<font size="-1">DISABLED</font>
											</xsl:otherwise>
										</xsl:choose>
										</td>
									</tr>								
									<tr bgcolor="#E3E1E3">
										<td>
											<font size="-1">Logging Verbosity</font>
										</td>
										<td>
											<font size="-1">
												<xsl:value-of select="result/params/verbosity" />
											</font>
										</td>
									</tr>																		
									<xsl:if test="(normalize-space(result/params/server))">
										<tr bgcolor="#E3E1E3">
											<td>
												<font size="-1">Network Server</font>
											</td>
											<td>
												<font size="-1">
													<xsl:value-of select="result/params/server" />
												</font>
											</td>
										</tr>
									</xsl:if>
									<xsl:if test="(normalize-space(result/params/results_file))">
										<tr bgcolor="#E3E1E3">
											<td>
												<font size="-1">Results File</font>
											</td>
											<td>
												<font size="-1">
													<xsl:value-of select="result/params/results_file" />
												</font>
											</td>
										</tr>
									</xsl:if>
									<xsl:if test="(normalize-space(result/params/history_file))">
										<tr bgcolor="#E3E1E3">
											<td>
												<font size="-1">History File</font>
											</td>
											<td>
												<font size="-1">
													<xsl:value-of select="result/params/history_file" />
												</font>
											</td>
										</tr>
									</xsl:if>
									<tr bgcolor="#E3E1E3">
										<td>
											<font size="-1">Initialization File</font>
										</td>
										<td>
											<font size="-1">
												<xsl:value-of select="result/params/ini_file" />
											</font>
										</td>
									</tr>
								</table>
							</font>
						</td>
						<td align="left" valign="top">
							<font face="Verdana">
								<!--    System Details Begin	-->
								<xsl:if test="result/system">
									<table border="0" cellSpacing="2" cellPadding="2">
										<tr bgcolor="#000000">
											<th align="center" colspan="2">
												<font color="#ffffff" size="+1">System Details</font>
											</th>
											</tr>
										<tr bgcolor="#E3E1E3">
											<td>
												<font size="-1">CPU</font>
											</td>
											<td>
												<font size="-1">
													<xsl:value-of select="result/system/cpu" />
												</font>
											</td>
										</tr>
										<tr bgcolor="#E3E1E3">
											<td>
												<font size="-1">Platform</font>
											</td>
											<td>
												<font size="-1">
													<xsl:value-of select="result/system/platform" />
												</font>
											</td>
										</tr>
										<tr bgcolor="#E3E1E3">
											<td>
												<font size="-1">Windows CE Version</font>
											</td>
											<td>
												<font size="-1">
													<xsl:value-of select="result/system/osversion" />
												</font>
											</td>
										</tr>
										<tr bgcolor="#E3E1E3">
											<td>
												<font size="-1">Build No.</font>
											</td>
											<td>
												<font size="-1">
													<xsl:value-of select="result/system/buildno" />
												</font>
											</td>
										</tr>
										<tr bgcolor="#E3E1E3">
											<td>
												<font size="-1">Additional</font>
											</td>
											<td>
												<font size="-1">
													<xsl:value-of select="result/system/additional" />
												</font>
											</td>
										</tr>
										<tr bgcolor="#E3E1E3">
											<td>
												<font size="-1">OS Language</font>
											</td>
											<td>
												<font size="-1">
													<xsl:value-of select="result/system/locale" />
												</font>
											</td>
										</tr>
										<tr bgcolor="#E3E1E3">
											<td>
												<font size="-1">Total Physical Memory</font>
											</td>
											<td>
												<font size="-1">
													<xsl:value-of select="round(result/system/memory/totalphys div 1024)" /> KB
												</font>
											</td>
										</tr>
										<tr bgcolor="#E3E1E3">
											<td>
												<font size="-1">Total Pagefile Size</font>
											</td>
											<td>
												<font size="-1">
													<xsl:value-of select="round(result/system/memory/totalpagefile div 1024)" /> KB
												</font>
											</td>
										</tr>
										<tr bgcolor="#E3E1E3">
											<td>
												<font size="-1">
													Total Virtual Memory
												</font>
											</td>
											<td>
												<font size="-1">
													<xsl:value-of select="round(result/system/memory/totalvirtual div 1024)" /> KB
												</font>
											</td>
										</tr>
										<tr bgcolor="#E3E1E3">
											<td>
												<font size="-1">Total Object Store</font>
											</td>
											<td>
												<font size="-1">
													<xsl:value-of select="round(result/system/objstore/storesize div 1024)" /> KB
												</font>
											</td>
										</tr>
									</table>
								</xsl:if>
								<!--    System Details End	-->
							</font>
						</td>
					</tr>
					<tr>
						<td align="left" valign="top">
							<font face="Verdana">
								<!--    System Health Begin	-->
								<xsl:if test="result/system/start">
									<table border="0" cellSpacing="2" cellPadding="2">
										<tr bgcolor="#000000">
											<th align="center" colspan="4">
												<font color="#ffffff" size="+1">System Health</font>
											</th>
										</tr>
										<tr bgcolor="#808080">
											<th align="left">
												<font color="#ffffff" size="-1">Item</font>
											</th>
											<th align="left">
												<font color="#ffffff" size="-1">At Start</font>
											</th>
											<th align="left">
												<font color="#ffffff" size="-1">Current</font>
											</th>
											<th align="left">
												<font color="#ffffff" size="-1">Probable <br></br>Leak (red)</font>
											</th>
										</tr>
										<xsl:for-each select="result/system">
											<tr bgcolor="#E3E1E3">
												<td>
													<font size="-1">Memory Usage</font>
												</td>
												<td>
													<font size="-1">
														<xsl:value-of select="start/memory/load" />%
													</font>
												</td>
												<td>
													<font size="-1">
														<xsl:value-of select="now/memory/load" />%
													</font>
												</td>
												<td>
													<xsl:choose>
														<xsl:when test="now/memory/load -   start/memory/load">
															<font size="-1" color="#ff0000">
																<xsl:value-of select="now/memory/load - start/memory/load" />%
															</font>
														</xsl:when>
														<xsl:otherwise>
															<font size="-1">
																<xsl:value-of select="now/memory/load - start/memory/load" />%
															</font>
														</xsl:otherwise>
													</xsl:choose>
												</td>
											</tr>
											<tr bgcolor="#E3E1E3">
												<td>
													<font size="-1">Available Physical Memory</font>
												</td>
												<td>
													<font size="-1">
														<xsl:value-of select="round(start/memory/availphys div 1024)" /> KB
													</font>
												</td>
												<td>
													<font size="-1">
														<xsl:value-of select="round(now/memory/availphys div 1024)" /> KB
													</font>
												</td>
												<td>
													<xsl:choose>
														<xsl:when test="round((start/memory/availphys - now/memory/availphys) div 1024)">
															<font size="-1" color="#ff0000">
																<b>
																	<xsl:value-of select="round((start/memory/availphys - now/memory/availphys) div 1024)" /> KB
																</b>
															</font>
														</xsl:when>
														<xsl:otherwise>
															<font size="-1">
																<xsl:value-of select="round((start/memory/availphys - now/memory/availphys) div 1024)" /> KB
															</font>
														</xsl:otherwise>
													</xsl:choose>
												</td>
											</tr>
											<tr bgcolor="#E3E1E3">
												<td>
													<font size="-1">Available Pagefile Size</font>
												</td>
												<td>
													<font size="-1">
														<xsl:value-of select="round(start/memory/availpagefile div 1024)" /> KB
													</font>
												</td>
												<td>
													<font size="-1">	
														<xsl:value-of select="round(now/memory/availpagefile div 1024)" /> KB
													</font>
												</td>
												<td>
													<xsl:choose>
														<xsl:when test="round((start/memory/availpagefile   - now/memory/availpagefile) div 1024)">
															<font size="-1" color="#ff0000">													
																<b>
																	<xsl:value-of select="round((start/memory/availpagefile - now/memory/availpagefile) div 1024)" /> KB
																</b>
															</font>
														</xsl:when>
														<xsl:otherwise>
															<font size="-1">
																<xsl:value-of select="round((start/memory/availpagefile - now/memory/availpagefile) div 1024)" /> KB
															</font>
														</xsl:otherwise>
													</xsl:choose>
												</td>
											</tr>
											<tr bgcolor="#E3E1E3">
												<td>
													<font size="-1">Available Virtual Memory</font>
												</td>
												<td>
													<font size="-1">
														<xsl:value-of select="round(start/memory/availvirtual div 1024)" /> KB
													</font>
												</td>
												<td>
													<font size="-1">
														<xsl:value-of select="round(now/memory/availvirtual div 1024)" /> KB
													</font>
												</td>
												<td>
													<xsl:choose>
														<xsl:when test="round((start/memory/availvirtual - now/memory/availvirtual) div 1024)">
															<font size="-1" color="#ff0000">
																<b>
																	<xsl:value-of select="round((start/memory/availvirtual - now/memory/availvirtual) div 1024)" /> KB
																</b>
															</font>
														</xsl:when>
														<xsl:otherwise>
															<font size="-1">
																<xsl:value-of select="round((start/memory/availvirtual - now/memory/availvirtual) div 1024)" /> KB
															</font>
														</xsl:otherwise>
													</xsl:choose>
												</td>
											</tr>
											<tr bgcolor="#E3E1E3">
												<td>
													<font size="-1">Available Object Store</font>
												</td>
												<td>
													<font size="-1">
														<xsl:value-of select="round(start/objstore/freesize div 1024)" /> KB
													</font>
												</td>
												<td>
													<font size="-1">
														<xsl:value-of select="round(now/objstore/freesize div 1024)" /> KB
													</font>
												</td>
												<td>
													<xsl:choose>
														<xsl:when test="round((start/objstore/freesize - now/objstore/freesize) div 1024)">
															<font size="-1" color="#ff0000">
																<b>
																	<xsl:value-of select="round((start/objstore/freesize - now/objstore/freesize) div 1024)" /> KB
																</b>
															</font>
														</xsl:when>
														<xsl:otherwise>
															<font size="-1">
																<xsl:value-of select="round((start/objstore/freesize - now/objstore/freesize) div 1024)" /> KB
															</font>
														</xsl:otherwise>
													</xsl:choose>
												</td>
											</tr>
										</xsl:for-each>
									</table>
								</xsl:if>									
								<!--    System Health End	-->
							</font>
						</td>
						<td align="left" valign="top">
							<font face="Verdana">
								<!--    Hang Status Begin	-->
								<xsl:if test="result/hangstatus/hung_module">
									<table border="0" cellSpacing="2" cellPadding="2">
										<tr bgcolor="#ff000000">
											<th align="center" colspan="4">
												<font color="#ffffff" size="+1">Hang Status</font>
											</th>
										</tr>
										<tr bgcolor="#808080">
											<th align="left">
												<font color="#ffffff" size="-1">Process Handle</font>
											</th>
											<th align="left">
												<font color="#ffffff" size="-1">Module Name</font>
											</th>
											<th align="left">
												<font color="#ffffff" size="-1">Command Line</font>
											</th>											
											<th align="left">
												<font color="#ffffff" size="-1">Continuously <br></br>Running For</font>
											</th>
										</tr>
										<xsl:for-each select="result/hangstatus/hung_module">
											<tr bgcolor="#E3E1E3">
												<td>
													<font size="-1">
														<xsl:value-of select="hproc" />
													</font>
												</td>											
												<td>
													<font size="-1">
														<xsl:value-of select="name" />
													</font>
												</td>
												<td>
													<font size="-1">
														<xsl:value-of select="cmdline" />
													</font>
												</td>												
												<td>
													<font size="-1">
														<xsl:value-of select="notexitedfor" /> minute(s)
													</font>
												</td>
											</tr>
										</xsl:for-each>
									</table>
								</xsl:if>
								<!--    Hang Status End	-->
							</font>
						</td>
					</tr>
				</table>
				<br></br>
				<br></br>
				<font face="Verdana">
					<!--    Module Status Begin	-->
					<xsl:if test="result/modules">
						<table border="0" cellSpacing="2" cellPadding="2">
							<tr bgcolor="#000000">
								<th align="center" colspan="13">
									<font color="#ffffff" size="+1">Module Status</font>
								</th>
							</tr>
							<tr bgcolor="#808080">
								<th align="left">
									<font color="#ffffff" size="-1">Module</font>
								</th>
								<th align="left">
									<font color="#ffffff" size="-1">Success<br></br>(%)</font>
								</th>
								<th align="left">
									<font color="#ffffff" size="-1">Launched</font>
								</th>
								<th align="left">
									<font color="#ffffff" size="-1">Iterations</font>
								</th>
								<th align="left">
									<font color="#ffffff" size="-1">Passed</font>
								</th>
								<th align="left">
									<font color="#ffffff" size="-1">Warnings <br></br>(Level 1)</font>
								</th>
								<th align="left">
									<font color="#ffffff" size="-1">Warnings <br></br>(Level 2)</font>
								</th>
								<th align="left">
									<font color="#ffffff" size="-1">Failed</font>
								</th>
								<th align="left">
									<font color="#ffffff" size="-1">Aborted</font>
								</th>
								<th align="left">
									<font color="#ffffff" size="-1">Terminated</font>
								</th>
								<th align="left">
									<font color="#ffffff" size="-1">Average <br></br>Runtime <br></br>(sec)</font>
								</th>
								<th align="left">
									<font color="#ffffff" size="-1">Maximum <br></br>Runtime <br></br>(sec)</font>
								</th>
								<th align="left">
									<font color="#ffffff" size="-1">Minimum <br></br>Runtime <br></br>(sec)</font>
								</th>
							</tr>
							<xsl:for-each select="result/modules/module">
								<tr bgcolor="#E3E1E3">
									<td>
										<font size="-1">
											<xsl:value-of select="name" />
										</font>
									</td>
									<td>
										<center>
											<font size="-1">
												<xsl:choose>
													<xsl:when test="number(iteration)">
														<xsl:value-of select="round((((iteration - fail) div iteration) * 100) * 100) div 100" />%
													</xsl:when>		
													<xsl:otherwise>			
														N.A.
													</xsl:otherwise>
												</xsl:choose>
											</font>
										</center>
									</td>
									<td>
										<center>
											<font size="-1">
												<xsl:value-of select="launch" />
											</font>
										</center>
									</td>
									<td>
										<center>
											<font size="-1">
												<xsl:value-of select="iteration" />
											</font>
										</center>
									</td>
									<td>
										<center>
											<font size="-1">
												<xsl:value-of select="pass" />
											</font>
										</center>
									</td>
									<td>
										<center>
											<font size="-1">
												<xsl:value-of select="warning1" />
											</font>
										</center>
									</td>
									<td>
										<center>
											<font size="-1">
												<xsl:value-of select="warning2" />
											</font>
										</center>
									</td>
									<td>
										<center>
											<font size="-1">
												<xsl:value-of select="fail" />
											</font>
										</center>
									</td>
									<td>
										<center>
											<font size="-1">
												<xsl:value-of select="abort" />
											</font>
										</center>
									</td>
									<td>
										<center>
											<font size="-1">
												<xsl:value-of select="terminate" />
											</font>
										</center>
									</td>
									<td>
										<center>
											<font size="-1">
												<xsl:value-of select="avg_duration" />
											</font>
										</center>
									</td>
									<td>
										<center>
											<font size="-1">
												<xsl:value-of select="max_duration" />
											</font>
										</center>
									</td>
									<td>
										<center>
											<font size="-1">
												<xsl:value-of select="min_duration" />
											</font>
										</center>
									</td>
								</tr>
							</xsl:for-each>
						</table>
					</xsl:if>
					<!--    Module Status End	-->
				</font>
				<br></br>
				<hr align="center" width="40%" noshade="true"></hr>
			</body>
		</html>
	</xsl:template>
</xsl:stylesheet>
