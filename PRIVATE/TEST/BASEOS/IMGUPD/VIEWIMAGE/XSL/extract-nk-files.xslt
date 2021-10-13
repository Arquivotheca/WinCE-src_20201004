<?xml version="1.0"?>
<!--
 Copyright (c) Microsoft Corporation.  All rights reserved.


 This source code is licensed under Microsoft Shared Source License
 Version 1.0 for Windows CE.
 For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
-->
<xsl:transform version="1.0"
                xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

<xsl:output method="xml" indent="yes" encoding="utf-8"
    cdata-section-elements="page"
    />

<xsl:template match="/">
    <xsl:comment> Nk files </xsl:comment>
    <xsl:element name="files">
        <xsl:apply-templates select="//nk//file">
            <xsl:sort select="name" />
        </xsl:apply-templates>
    </xsl:element>
</xsl:template>

<xsl:template match="file">
    <xsl:element name="file">
        <!-- moving the name to an attribute helps make diffs more readable-->
        <xsl:attribute name="name"><xsl:value-of select="./name/text()" /></xsl:attribute>
        <xsl:apply-templates select="@*|node()" />
    </xsl:element>
</xsl:template>

<!-- strip the name element -->
<xsl:template match="name">
</xsl:template>

<!-- strip the ID attributes -->
<xsl:template match="@ID">
</xsl:template>

<!-- this is an identity copy: -->
<xsl:template match="@*|node()">
    <xsl:copy>
        <xsl:apply-templates select="@*|node()"/>
    </xsl:copy>
</xsl:template>

		
</xsl:transform>
