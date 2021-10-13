<?xml version="1.0"?>
<!--
 Copyright (c) Microsoft Corporation.  All rights reserved.


 This source code is licensed under Microsoft Shared Source License
 Version 1.0 for Windows CE.
 For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
-->
<xsl:transform version="1.0"
                xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns:cfg="http://tempuri.org/MemoryCFG.xsd" >
<xsl:output method="text" encoding="utf-8" />

<xsl:param name="nb0Path" select="'please supply an nb0Path parameter'" />
<xsl:param name="part" select="'none'" />

<!--
    This transform extracts the NOR/NAND-ness, VASTART, BLOCKSIZE and SECTORSIZE
    from a memory.cfg.xml file, as appropriate to its NOR/NAND-ness.
-->

<xsl:template match="/">
    <xsl:choose>
        <xsl:when test="$part='none'">
            <xsl:variable name="partNodes" select="/cfg:MEMORYCFG/cfg:HARDWARE/cfg:NAND|/cfg:MEMORYCFG/cfg:HARDWARE/cfg:NOR" />
            <xsl:call-template name="validate-part">
                <xsl:with-param name="parts" select="$partNodes" />
            </xsl:call-template>
        
            <xsl:apply-templates select="/cfg:MEMORYCFG/cfg:HARDWARE/cfg:NAND|/cfg:MEMORYCFG/cfg:HARDWARE/cfg:NOR" />
        </xsl:when>
        <xsl:otherwise>
            <xsl:variable name="partNodes" select="/cfg:MEMORYCFG/cfg:HARDWARE/cfg:NAND[@ID=$part]|/cfg:MEMORYCFG/cfg:HARDWARE/cfg:NOR[@ID=$part]" />
            <xsl:call-template name="validate-part">
                <xsl:with-param name="parts" select="$partNodes" />
            </xsl:call-template>
        
            <xsl:apply-templates select="/cfg:MEMORYCFG/cfg:HARDWARE/cfg:NAND[@ID=$part]|/cfg:MEMORYCFG/cfg:HARDWARE/cfg:NOR[@ID=$part]" />
        </xsl:otherwise>
    </xsl:choose>
</xsl:template>

<xsl:template name="validate-part">
    <xsl:param name="parts" />
    <xsl:variable name="cParts" select="count($parts)" />
    <xsl:choose>
        <xsl:when test="$cParts=1">
            <!-- that's okay -->
        </xsl:when>
        <xsl:when test="$cParts=0">
            <xsl:text>**get-part-info.xslt:NO.PARTS.FOUND** </xsl:text>
        </xsl:when>
        <xsl:otherwise>
            <xsl:text>**get-part-info.xslt:TOO.MANY.PARTS.FOUND:</xsl:text><xsl:value-of select="$cParts"/>
            <xsl:text>(</xsl:text>
            <xsl:for-each select="$parts">
                <xsl:text>/</xsl:text>
                <xsl:value-of select="name()" />
                <xsl:text>[@='</xsl:text>
                <xsl:value-of select="@ID" />
                <xsl:text>']</xsl:text>
                <xsl:text>,</xsl:text>
            </xsl:for-each>
            <xsl:text>)** </xsl:text>
        </xsl:otherwise>
    </xsl:choose>
</xsl:template>

<!-- either NAND or NOR, choose one! -->
<xsl:template match="/cfg:MEMORYCFG/cfg:HARDWARE/cfg:NAND|/cfg:MEMORYCFG/cfg:HARDWARE/cfg:NOR">
    <xsl:variable name="partType" select="translate(name(), 'NORAD', 'norad')" /><!-- to lower case -->
    <!-- output -nand or -nor -->
    <xsl:text>-</xsl:text><xsl:value-of select="$partType" /><xsl:text>
    </xsl:text>
    
    <xsl:if test="$partType = 'nor'">
        <xsl:text>-vastart </xsl:text>
        <xsl:value-of select="./@VASTART" />
        <xsl:text>
        </xsl:text>
    </xsl:if>
    
    <xsl:text>-sectorsize </xsl:text>
    <xsl:value-of select="./@SECTORSIZE" /><xsl:text>
    </xsl:text>
    
    <xsl:text>-blocksize </xsl:text>
    <xsl:value-of select="./@BLOCKSIZE" /><xsl:text>
    </xsl:text>
    <xsl:text>-nb0 </xsl:text>
    <xsl:variable name="nb0File" select="./@ID" />
    <xsl:variable name="nb0FileUpper" select="translate($nb0File, 'abcdefghijklmnopqrstuvwxyz', 'ABCDEFGHIJKLMNOPQRSTUVWXYZ')" />
    <xsl:value-of select="$nb0Path" />
    <xsl:value-of select="$nb0FileUpper" /><xsl:text>.nb0
    </xsl:text>
</xsl:template>

</xsl:transform>
