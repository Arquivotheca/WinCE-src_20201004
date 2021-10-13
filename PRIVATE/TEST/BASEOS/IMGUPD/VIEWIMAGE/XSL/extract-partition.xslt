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

<xsl:param name="nk" select="0" />
<xsl:param name="imgfs" select="0" />
<xsl:param name="reserve" select="0" />
<xsl:param name="page" select="1" />

<!--
    This transform extracts a lite version of the desktop.xml or device.xml document:
    it removes all the data.
-->

<xsl:template match="/">
    <xsl:comment>processed with extract-partition.xsl</xsl:comment>
    <xsl:apply-templates select="@*|node()"/>
</xsl:template>

<xsl:template match="page">
    <xsl:if test="$page != 0">
        <xsl:copy>
            <xsl:apply-templates select="@*|node()"/>
        </xsl:copy>
    </xsl:if>
</xsl:template>

<!-- this is an identity copy: -->
<xsl:template match="@*|node()">
    <xsl:choose>
        <xsl:when test="name() = 'nk'">
            <xsl:if test="$nk!=0">
                <xsl:copy>
                    <xsl:apply-templates select="@*|node()"/>
                </xsl:copy>
            </xsl:if>
        </xsl:when>
        <xsl:when test="name() = 'imgfs'">
            <xsl:if test="$imgfs!=0">
                <xsl:copy>
                    <xsl:apply-templates select="@*|node()"/>
                </xsl:copy>
            </xsl:if>
        </xsl:when>
        <xsl:when test="name() = 'reserve'">
            <xsl:if test="$reserve!=0">
                <xsl:copy>
                    <xsl:apply-templates select="@*|node()"/>
                </xsl:copy>
            </xsl:if>
        </xsl:when>
        <xsl:otherwise>
            <xsl:copy>
                <xsl:apply-templates select="@*|node()"/>
            </xsl:copy>
        </xsl:otherwise>
    </xsl:choose>
</xsl:template>

</xsl:transform>
