<?xml version="1.0"?>
<!--
 Copyright (c) Microsoft Corporation.  All rights reserved.


 This source code is licensed under Microsoft Shared Source License
 Version 1.0 for Windows CE.
 For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
-->
<xsl:transform version="1.0"
                xmlns:xsl="http://www.w3.org/1999/XSL/Transform" >
<xsl:output method="xml" indent="yes" encoding="utf-8"
    cdata-section-elements="page"
    />


<!--
    This transform extracts a lite version of the desktop.xml or device.xml document:
    it removes all the data.
-->

<xsl:template match="/">
    <xsl:comment>processed with extract-lite-version.xsl</xsl:comment>
    <xsl:apply-templates select="@*|node()"/>
</xsl:template>

<xsl:template match="page">
</xsl:template>

<!-- this is an identity copy: -->
<xsl:template match="@*|node()">
    <xsl:copy>
        <xsl:apply-templates select="@*|node()"/>
    </xsl:copy>
</xsl:template>

</xsl:transform>
