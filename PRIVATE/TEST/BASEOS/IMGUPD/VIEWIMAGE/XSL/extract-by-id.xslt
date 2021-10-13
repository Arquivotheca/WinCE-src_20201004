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

<!-- this is an identity copy, except that it only copies elements that match by ID: -->
<xsl:template match="@*|node()">
    <xsl:variable name="idlist" select="document('test.out')" /> <!-- //$ TODO -->
    <xsl:variable name="myid" select="@ID" />
    <xsl:variable name="matched" select="count($idlist//el[@ID=$myid]) > 0" />
    <xsl:if test="$matched">
        <xsl:copy>
            <xsl:apply-templates select="@*|node()"/>
        </xsl:copy>
    </xsl:if>
    <xsl:if test="not($matched)">
        <xsl:apply-templates select="@*|node()"/>
    </xsl:if>
    <!--
    <xsl:comment><xsl:value-of select="$matched"/></xsl:comment>
    -->
    <!--
        <xsl:copy>
            <xsl:apply-templates select="@*|node()"/>
        </xsl:copy>
    -->
</xsl:template>

</xsl:transform>
