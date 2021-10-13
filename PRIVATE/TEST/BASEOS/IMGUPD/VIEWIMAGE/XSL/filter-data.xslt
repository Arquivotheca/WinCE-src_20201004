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
    <xsl:apply-templates select="@*|node()" />
</xsl:template>

<!-- this is an identity copy, except that it strips "el" attributes: -->
<xsl:template match="@*|node()">
    <xsl:if test="name()!='el'">
        <xsl:copy>
            <xsl:apply-templates select="@*|node()"/>
        </xsl:copy>
    </xsl:if>
</xsl:template>

<xsl:template match="//imgfs//module">
    <!-- ignore kernel modules, they show up in NK -->
    <xsl:variable name="myname" select="name/text()" />

    <xsl:variable name="ignore" select="count(//nk/modules/module/name[text()=$myname]) > 0"/>
    <xsl:if test="not($ignore)">
        <xsl:element name="module">
            <!-- moving the name to an attribute helps make diffs more readable-->
            <xsl:attribute name="name"><xsl:value-of select="./name/text()" /></xsl:attribute>
            <xsl:apply-templates select="@*|node()" />
        </xsl:element>
    </xsl:if>
</xsl:template>

<xsl:template match="file">
    <!-- ignore files whose names appear in ignore-imgfs-files.xml -->
    <xsl:variable name="myname" select="name/text()" />
    <xsl:variable name="ignorables" select="document('ignore-imgfs-files.xml')" />
    <xsl:variable name="ignore" select="count($ignorables//ignore-name[.=$myname]) > 0" />

    <xsl:if test="not($ignore)">
        <xsl:element name="file">
            <!-- moving the name to an attribute helps make diffs more readable-->
            <xsl:attribute name="name"><xsl:value-of select="./name/text()" /></xsl:attribute>
            <xsl:apply-templates select="@*|node()" />
        </xsl:element>
    </xsl:if>
</xsl:template>

<xsl:template match="nk-options"></xsl:template>
<xsl:template match="imgfs-options"></xsl:template>
<xsl:template match="reserved-options"></xsl:template>

</xsl:transform>
