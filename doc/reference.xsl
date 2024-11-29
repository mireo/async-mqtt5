<?xml version="1.0" encoding="utf-8"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">

<!--
    Copyright (c) 2023-2024 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
-->

<xsl:output method="text"/>
<xsl:strip-space elements="*"/>
<xsl:preserve-space elements="para"/>

<xsl:variable name="newline">
<xsl:text>
</xsl:text>
</xsl:variable>

<xsl:variable name="all-compounddefs" select="/doxygen//compounddef"/>
<xsl:variable name="all-memberdefs" select="/doxygen//memberdef"/>

<!--
  Loop over all top-level documentation elements (i.e. classes, functions,
  variables and typedefs at namespace scope). The elements are sorted by name.
  Anything in a "detail" namespace is skipped.
-->
<xsl:template match="/doxygen">
<xsl:text>
[include Error_handling.qbk]
[include concepts/ExecutionContext.qbk]
[include concepts/StreamType.qbk]
[include concepts/TlsContext.qbk]
[include concepts/is_authenticator.qbk]
[include concepts/LoggerType.qbk]
[include reason_codes/Reason_codes.qbk]
[include properties/will_props.qbk]
[include properties/connect_props.qbk]
[include properties/connack_props.qbk]
[include properties/publish_props.qbk]
[include properties/puback_props.qbk]
[include properties/pubrec_props.qbk]
[include properties/pubrel_props.qbk]
[include properties/pubcomp_props.qbk]
[include properties/subscribe_props.qbk]
[include properties/suback_props.qbk]
[include properties/unsubscribe_props.qbk]
[include properties/unsuback_props.qbk]
[include properties/disconnect_props.qbk]
[include properties/auth_props.qbk]

</xsl:text>

  <xsl:for-each select="
      compounddef[@kind = 'class' or @kind = 'struct'] |
      compounddef[@kind = 'namespace']/sectiondef/memberdef">
    <xsl:sort select="concat((. | ancestor::*)/compoundname, '::', name, ':x')"/>
    <xsl:sort select="name"/>
    <xsl:choose>
      <xsl:when test="@kind='class' or @kind='struct'">
        <xsl:if test="
            contains(compoundname, 'async_mqtt5::') and
            not(contains(compoundname, '::detail')) and
            not(contains(compoundname, 'boost::system::'))">
          <xsl:call-template name="class"/>
        </xsl:if>
      </xsl:when>
      <xsl:otherwise>
        <xsl:if test="
            not(contains(ancestor::*/compoundname, '::detail'))">
          <xsl:call-template name="namespace-memberdef"/>
        </xsl:if>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:for-each>

</xsl:template>

<!--========== Utilities ==========-->

<xsl:template name="strip-async-mqtt5-ns">
  <xsl:param name="name"/>
  <xsl:choose>
    <xsl:when test="contains($name, 'async_mqtt5::')">
      <xsl:value-of select="substring-after($name, 'async_mqtt5::')"/>
    </xsl:when>
    <xsl:otherwise>
      <xsl:value-of select="$name"/>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>


<xsl:template name="strip-ns">
  <xsl:param name="name"/>
  <xsl:choose>
    <xsl:when test="contains($name, '::') and contains($name, '&lt;')">
      <xsl:choose>
        <xsl:when test="string-length(substring-before($name, '::')) &lt; string-length(substring-before($name, '&lt;'))">
          <xsl:call-template name="strip-ns">
            <xsl:with-param name="name" select="substring-after($name, '::')"/>
          </xsl:call-template>
        </xsl:when>
        <xsl:otherwise>
          <xsl:value-of select="$name"/>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:when>
    <xsl:when test="contains($name, '::')">
      <xsl:call-template name="strip-ns">
        <xsl:with-param name="name" select="substring-after($name, '::')"/>
      </xsl:call-template>
    </xsl:when>
    <xsl:otherwise>
      <xsl:value-of select="$name"/>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<xsl:template name="cleanup-type">
  <xsl:param name="name"/>
  <xsl:variable name="type">
    <xsl:choose>
      <xsl:when test="$name = 'virtual'"></xsl:when>
      <xsl:otherwise>
        <xsl:value-of select="$name"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:variable>
  <xsl:value-of select="$type"/>
</xsl:template>

<xsl:template name="make-id">
  <xsl:param name="name"/>
  <xsl:param name="static"/>
  <xsl:choose>
    <xsl:when test="$name='query' and $static='yes'">
      <xsl:text>query__static</xsl:text>
    </xsl:when>
    <xsl:when test="contains($name, '::')">
      <xsl:call-template name="make-id">
        <xsl:with-param name="name"
         select="concat(substring-before($name, '::'), '.', substring-after($name, '::'))"/>
      </xsl:call-template>
    </xsl:when>
    <xsl:when test="contains($name, '=')">
      <xsl:call-template name="make-id">
        <xsl:with-param name="name"
         select="concat(substring-before($name, '='), '_eq_', substring-after($name, '='))"/>
      </xsl:call-template>
    </xsl:when>
    <xsl:when test="contains($name, '!')">
      <xsl:call-template name="make-id">
        <xsl:with-param name="name"
         select="concat(substring-before($name, '!'), '_not_', substring-after($name, '!'))"/>
      </xsl:call-template>
    </xsl:when>
    <xsl:when test="contains($name, '-&gt;')">
      <xsl:call-template name="make-id">
        <xsl:with-param name="name"
         select="concat(substring-before($name, '-&gt;'), '_arrow_', substring-after($name, '-&gt;'))"/>
      </xsl:call-template>
    </xsl:when>
    <xsl:when test="contains($name, '&lt;')">
      <xsl:call-template name="make-id">
        <xsl:with-param name="name"
         select="concat(substring-before($name, '&lt;'), '_lt_', substring-after($name, '&lt;'))"/>
      </xsl:call-template>
    </xsl:when>
    <xsl:when test="contains($name, '&gt;')">
      <xsl:call-template name="make-id">
        <xsl:with-param name="name"
         select="concat(substring-before($name, '&gt;'), '_gt_', substring-after($name, '&gt;'))"/>
      </xsl:call-template>
    </xsl:when>
    <xsl:when test="contains($name, '&amp;')">
      <xsl:call-template name="make-id">
        <xsl:with-param name="name"
         select="concat(substring-before($name, '&amp;'), '_amp_', substring-after($name, '&amp;'))"/>
      </xsl:call-template>
    </xsl:when>
    <xsl:when test="contains($name, '[')">
      <xsl:call-template name="make-id">
        <xsl:with-param name="name"
         select="concat(substring-before($name, '['), '_lb_', substring-after($name, '['))"/>
      </xsl:call-template>
    </xsl:when>
    <xsl:when test="contains($name, ']')">
      <xsl:call-template name="make-id">
        <xsl:with-param name="name"
         select="concat(substring-before($name, ']'), '_rb_', substring-after($name, ']'))"/>
      </xsl:call-template>
    </xsl:when>
    <xsl:when test="contains($name, '(')">
      <xsl:call-template name="make-id">
        <xsl:with-param name="name"
         select="concat(substring-before($name, '('), '_lp_', substring-after($name, '('))"/>
      </xsl:call-template>
    </xsl:when>
    <xsl:when test="contains($name, ')')">
      <xsl:call-template name="make-id">
        <xsl:with-param name="name"
         select="concat(substring-before($name, ')'), '_rp_', substring-after($name, ')'))"/>
      </xsl:call-template>
    </xsl:when>
    <xsl:when test="contains($name, '+')">
      <xsl:call-template name="make-id">
        <xsl:with-param name="name"
         select="concat(substring-before($name, '+'), '_plus_', substring-after($name, '+'))"/>
      </xsl:call-template>
    </xsl:when>
    <xsl:when test="contains($name, '-')">
      <xsl:call-template name="make-id">
        <xsl:with-param name="name"
         select="concat(substring-before($name, '-'), '_minus_', substring-after($name, '-'))"/>
      </xsl:call-template>
    </xsl:when>
    <xsl:when test="contains($name, '*')">
      <xsl:call-template name="make-id">
        <xsl:with-param name="name"
         select="concat(substring-before($name, '*'), '_star_', substring-after($name, '*'))"/>
      </xsl:call-template>
    </xsl:when>
    <xsl:when test="contains($name, '~')">
      <xsl:call-template name="make-id">
        <xsl:with-param name="name"
         select="concat(substring-before($name, '~'), '_', substring-after($name, '~'))"/>
      </xsl:call-template>
    </xsl:when>
    <xsl:when test="contains($name, '^')">
      <xsl:call-template name="make-id">
        <xsl:with-param name="name"
         select="concat(substring-before($name, '^'), '_hat_', substring-after($name, '^'))"/>
      </xsl:call-template>
    </xsl:when>
    <xsl:when test="contains($name, '|')">
      <xsl:call-template name="make-id">
        <xsl:with-param name="name"
         select="concat(substring-before($name, '|'), '_pipe_', substring-after($name, '|'))"/>
      </xsl:call-template>
    </xsl:when>
    <xsl:when test="contains($name, ',')">
      <xsl:call-template name="make-id">
        <xsl:with-param name="name"
         select="concat(substring-before($name, ','), '_comma_', substring-after($name, ','))"/>
      </xsl:call-template>
    </xsl:when>
    <xsl:when test="contains($name, '&quot;')">
      <xsl:call-template name="make-id">
        <xsl:with-param name="name"
         select="concat(substring-before($name, '&quot;'), '_quot_', substring-after($name, '&quot;'))"/>
      </xsl:call-template>
    </xsl:when>
    <xsl:when test="contains($name, '...')">
      <xsl:call-template name="make-id">
        <xsl:with-param name="name"
         select="concat(substring-before($name, '...'), '_ellipsis_', substring-after($name, '...'))"/>
      </xsl:call-template>
    </xsl:when>
    <xsl:when test="contains($name, ' ')">
      <xsl:call-template name="make-id">
        <xsl:with-param name="name"
         select="concat(substring-before($name, ' '), '_', substring-after($name, ' '))"/>
      </xsl:call-template>
    </xsl:when>
    <xsl:otherwise>
      <xsl:value-of select="$name"/>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>


<!--========== Markup ==========-->

<xsl:template match="para" mode="markup">
<xsl:text>
</xsl:text><xsl:apply-templates mode="markup"/><xsl:text>
</xsl:text>
</xsl:template>


<xsl:template match="para" mode="markup-nested">
<xsl:apply-templates mode="markup-nested"/>
</xsl:template>


<xsl:template match="title" mode="markup">
  <xsl:variable name="title">
    <xsl:value-of select="."/>
  </xsl:variable>
  <xsl:if test="string-length($title) > 0">
[heading <xsl:value-of select="."/>]
  </xsl:if>
</xsl:template>


<xsl:template match="programlisting" mode="markup">
  <xsl:value-of select="$newline"/>
  <xsl:value-of select="$newline"/>
  <xsl:apply-templates mode="codeline"/>
  <xsl:value-of select="$newline"/>
  <xsl:value-of select="$newline"/>
</xsl:template>


<xsl:template match="programlisting" mode="markup-nested">
  <xsl:value-of select="$newline"/>
  <xsl:text>``</xsl:text>
  <xsl:value-of select="$newline"/>
  <xsl:apply-templates mode="codeline"/>
  <xsl:if test="substring(., string-length(.)) = $newline">
    <xsl:value-of select="$newline"/>
  </xsl:if>
  <xsl:text>``</xsl:text>
  <xsl:value-of select="$newline"/>
</xsl:template>


<xsl:template match="codeline" mode="codeline">
  <xsl:if test="string-length(.) &gt; 0">
    <xsl:text>  </xsl:text>
  </xsl:if>
  <xsl:apply-templates mode="codeline"/>
  <xsl:value-of select="$newline"/>
</xsl:template>


<xsl:template match="sp" mode="markup">
<xsl:text> </xsl:text>
</xsl:template>


<xsl:template match="sp" mode="markup-nested">
<xsl:text> </xsl:text>
</xsl:template>


<xsl:template match="sp" mode="codeline">
  <xsl:text> </xsl:text>
</xsl:template>


<xsl:template match="linebreak" mode="markup">
<xsl:text>

</xsl:text>
</xsl:template>


<xsl:template match="linebreak" mode="markup-nested">
<xsl:text>

</xsl:text>
</xsl:template>


<xsl:template match="computeroutput" mode="markup">
<xsl:text>`</xsl:text><xsl:value-of select="."/><xsl:text>`</xsl:text>
</xsl:template>


<xsl:template match="computeroutput" mode="markup-nested">
<xsl:text>`</xsl:text><xsl:value-of select="."/><xsl:text>`</xsl:text>
</xsl:template>


<xsl:template match="listitem" mode="markup">
* <xsl:call-template name="strip-leading-whitespace">
  <xsl:with-param name="text">
    <xsl:apply-templates mode="markup"/>
  </xsl:with-param>
</xsl:call-template>
</xsl:template>


<xsl:template match="bold" mode="markup">[*<xsl:apply-templates mode="markup"/>]</xsl:template>


<xsl:template match="emphasis" mode="markup">['<xsl:apply-templates mode="markup"/>]</xsl:template>

<xsl:template match="parameterlist" mode="markup">
  <xsl:choose>
    <xsl:when test="@kind='templateparam'">
[heading Template Parameters]
    </xsl:when>
    <xsl:when test="@kind='param'">
[heading Parameters]
    </xsl:when>
    <xsl:when test="@kind='exception'">
[heading Exceptions]
    </xsl:when>
  </xsl:choose>

[table
  [[Name] [Description]]
  <xsl:apply-templates mode="markup"/>
]
</xsl:template>


<xsl:template match="parameteritem" mode="markup">
[[<xsl:value-of select="parameternamelist"/>][<xsl:apply-templates select="parameterdescription" mode="markup-nested"/>]]
</xsl:template>

<xsl:template match="simplesect" mode="markup">
  <xsl:choose>
    <xsl:when test="@kind='return'">
[heading Return Value]
      <xsl:apply-templates mode="markup"/>
    </xsl:when>
    <xsl:when test="@kind='see'">
[heading See More]
      <xsl:apply-templates mode="markup"/>
    </xsl:when>
    <xsl:when test="@kind='note'">
[note
      <xsl:apply-templates mode="markup"/>
]
    </xsl:when>
    <xsl:when test="@kind='attention'">
[important
      <xsl:apply-templates mode="markup"/>
]
    </xsl:when>
    <xsl:when test="@kind='par'">
      <xsl:if test="not(starts-with(title, 'Concepts:'))">
        <xsl:apply-templates mode="markup"/>
      </xsl:if>
    </xsl:when>
    <xsl:otherwise>
      <xsl:apply-templates mode="markup"/>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>


<xsl:template match="text()" mode="markup">
  <xsl:variable name="text" select="."/>
  <xsl:variable name="starts-with-whitespace" select="
      starts-with($text, ' ') or starts-with($text, $newline)"/>
  <xsl:variable name="preceding-node-name">
    <xsl:for-each select="preceding-sibling::*">
      <xsl:if test="position() = last()">
        <xsl:value-of select="local-name()"/>
      </xsl:if>
    </xsl:for-each>
  </xsl:variable>
  <xsl:variable name="after-newline" select="
      $preceding-node-name = 'programlisting' or
      $preceding-node-name = 'linebreak'"/>
  <xsl:choose>
    <xsl:when test="$starts-with-whitespace and $after-newline">
      <xsl:call-template name="strip-leading-whitespace">
        <xsl:with-param name="text" select="$text"/>
      </xsl:call-template>
    </xsl:when>
    <xsl:otherwise>
      <xsl:value-of select="$text"/>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>


<xsl:template match="text()" mode="markup-nested">
  <xsl:variable name="text" select="."/>
  <xsl:variable name="starts-with-whitespace" select="
      starts-with($text, ' ') or starts-with($text, $newline)"/>
  <xsl:variable name="preceding-node-name">
    <xsl:for-each select="preceding-sibling::*">
      <xsl:if test="position() = last()">
        <xsl:value-of select="local-name()"/>
      </xsl:if>
    </xsl:for-each>
  </xsl:variable>
  <xsl:variable name="after-newline" select="
      $preceding-node-name = 'programlisting' or
      $preceding-node-name = 'linebreak'"/>
  <xsl:choose>
    <xsl:when test="$starts-with-whitespace and $after-newline">
      <xsl:call-template name="strip-leading-whitespace">
        <xsl:with-param name="text" select="$text"/>
      </xsl:call-template>
    </xsl:when>
    <xsl:otherwise>
      <xsl:value-of select="$text"/>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>


<xsl:template name="strip-leading-whitespace">
  <xsl:param name="text"/>
  <xsl:choose>
    <xsl:when test="starts-with($text, ' ')">
      <xsl:call-template name="strip-leading-whitespace">
        <xsl:with-param name="text" select="substring($text, 2)"/>
      </xsl:call-template>
    </xsl:when>
    <xsl:when test="starts-with($text, $newline)">
      <xsl:call-template name="strip-leading-whitespace">
        <xsl:with-param name="text" select="substring($text, 2)"/>
      </xsl:call-template>
    </xsl:when>
    <xsl:otherwise>
      <xsl:value-of select="$text"/>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<xsl:template name="escape-name">
  <xsl:param name="text"/>
  <xsl:choose>
    <xsl:when test="contains($text, '[')">
      <xsl:value-of select="substring-before($text, '[')"/>
      <xsl:text>\[</xsl:text>
      <xsl:call-template name="escape-name">
        <xsl:with-param name="text" select="substring-after($text, '[')"/>
      </xsl:call-template>
    </xsl:when>
    <xsl:when test="contains($text, ']')">
      <xsl:value-of select="substring-before($text, ']')"/>
      <xsl:text>\]</xsl:text>
      <xsl:call-template name="escape-name">
        <xsl:with-param name="text" select="substring-after($text, ']')"/>
      </xsl:call-template>
    </xsl:when>
    <xsl:when test="contains($text, '...')">
      <xsl:value-of select="substring-before($text, '...')"/>
      <xsl:text>\.\.\.</xsl:text>
      <xsl:call-template name="escape-name">
        <xsl:with-param name="text" select="substring-after($text, '...')"/>
      </xsl:call-template>
    </xsl:when>
    <xsl:otherwise>
      <xsl:value-of select="$text"/>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>


<xsl:template match="ref[@kindref='compound']" mode="markup">
  <xsl:variable name="name">
    <xsl:value-of select="."/>
  </xsl:variable>
  <xsl:choose>
    <xsl:when test="contains(@refid, 'async__mqtt5') or contains($name, 'async_mqtt5::')">
      <xsl:variable name="dox-ref-id" select="@refid"/>
      <xsl:variable name="ref-name">
        <xsl:call-template name="strip-async-mqtt5-ns">
          <xsl:with-param name="name"
            select="(($all-compounddefs)[@id=$dox-ref-id])[1]/compoundname"/>
        </xsl:call-template>
      </xsl:variable>
      <xsl:variable name="ref-id">
        <xsl:call-template name="make-id">
          <xsl:with-param name="name" select="$ref-name"/>
        </xsl:call-template>
      </xsl:variable>
      <xsl:text>[link async_mqtt5.ref.</xsl:text>
      <xsl:value-of select="$ref-id"/>
      <xsl:text> `</xsl:text>
      <xsl:value-of name="text" select="$ref-name"/>
      <xsl:text>`]</xsl:text>
    </xsl:when>
    <xsl:otherwise>
      <xsl:text>`</xsl:text>
      <xsl:value-of select="."/>
      <xsl:text>`</xsl:text>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>


<xsl:template match="ref[@kindref='compound']" mode="markup-nested">
  <xsl:variable name="name">
    <xsl:value-of select="."/>
  </xsl:variable>
  <xsl:choose>
    <xsl:when test="contains(@refid, 'async__mqtt5') or contains($name, 'async_mqtt5::')">
      <xsl:variable name="dox-ref-id" select="@refid"/>
      <xsl:variable name="ref-name">
        <xsl:call-template name="strip-async-mqtt5-ns">
          <xsl:with-param name="name"
            select="(($all-compounddefs)[@id=$dox-ref-id])[1]/compoundname"/>
        </xsl:call-template>
      </xsl:variable>
      <xsl:variable name="ref-id">
        <xsl:call-template name="make-id">
          <xsl:with-param name="name" select="$ref-name"/>
        </xsl:call-template>
      </xsl:variable>
      <xsl:text>[link async_mqtt5.ref.</xsl:text>
      <xsl:value-of select="$ref-id"/>
      <xsl:text> `</xsl:text>
      <xsl:value-of name="text" select="$ref-name"/>
      <xsl:text>`]</xsl:text>
    </xsl:when>
    <xsl:otherwise>
      <xsl:text>`</xsl:text>
      <xsl:value-of select="."/>
      <xsl:text>`</xsl:text>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>


<xsl:template match="ref[@kindref='member']" mode="markup">
  <xsl:variable name="dox-ref-id" select="@refid"/>
  <xsl:variable name="memberdefs" select="($all-memberdefs)[@id=$dox-ref-id]"/>
  <xsl:choose>
    <xsl:when test="contains(@refid, 'async__mqtt5') and count($memberdefs) &gt; 0">
      <xsl:variable name="dox-compound-name" select="($memberdefs)[1]/../../compoundname"/>
      <xsl:variable name="dox-name" select="($memberdefs)[1]/name"/>
      <xsl:variable name="ref-name">
        <xsl:call-template name="strip-async-mqtt5-ns">
          <xsl:with-param name="name" select="concat($dox-compound-name,'::',$dox-name)"/>
        </xsl:call-template>
      </xsl:variable>
      <xsl:variable name="ref-id">
        <xsl:call-template name="make-id">
          <xsl:with-param name="name" select="$ref-name"/>
        </xsl:call-template>
      </xsl:variable>
      <xsl:text>[link async_mqtt5.ref.</xsl:text>
      <xsl:value-of select="$ref-id"/>
      <xsl:text> `</xsl:text>
      <xsl:value-of name="text" select="$ref-name"/>
      <xsl:text>`]</xsl:text>
    </xsl:when>
    <xsl:when test="contains(@refid, 'async__mqtt5_1_1client') and count($memberdefs) = 0">
      <xsl:text>
      [link async_mqtt5.ref.client.error</xsl:text>
      <xsl:text> `</xsl:text>
      <xsl:value-of select='text()'/>
      <xsl:text>`]</xsl:text>
    </xsl:when>
    <xsl:otherwise>
      <xsl:text>`</xsl:text>
      <xsl:value-of select="."/>
      <xsl:text>`</xsl:text>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>


<xsl:template match="ref[@kindref='member']" mode="markup-nested">
  <xsl:variable name="dox-ref-id" select="@refid"/>
  <xsl:variable name="memberdefs" select="($all-memberdefs)[@id=$dox-ref-id]"/>
  <xsl:choose>
    <xsl:when test="contains(@refid, 'async__mqtt5') and count($memberdefs) &gt; 0">
      <xsl:variable name="dox-compound-name" select="($memberdefs)[1]/../../compoundname"/>
      <xsl:variable name="dox-name" select="($memberdefs)[1]/name"/>
      <xsl:variable name="ref-name">
        <xsl:call-template name="strip-async-mqtt5-ns">
          <xsl:with-param name="name" select="concat($dox-compound-name,'::',$dox-name)"/>
        </xsl:call-template>
      </xsl:variable>
      <xsl:variable name="ref-id">
        <xsl:call-template name="make-id">
          <xsl:with-param name="name" select="$ref-name"/>
        </xsl:call-template>
      </xsl:variable>
      <xsl:text>[link async_mqtt5.ref.</xsl:text>
      <xsl:value-of select="$ref-id"/>
      <xsl:text> `</xsl:text>
      <xsl:value-of name="text" select="$ref-name"/>
      <xsl:text>`]</xsl:text>
    </xsl:when>
    <xsl:otherwise>
      <xsl:text>`</xsl:text>
      <xsl:value-of select="."/>
      <xsl:text>`</xsl:text>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>


<xsl:template name="header-requirements">
  <xsl:param name="file"/>
  <xsl:value-of select="$newline"/>
  <xsl:text>[heading Requirements]</xsl:text>
  <xsl:value-of select="$newline"/>
  <xsl:value-of select="$newline"/>
  <xsl:text>['Header: ]</xsl:text>
  <xsl:text>[^async_mqtt5/</xsl:text>
  <xsl:value-of select="substring-after($file, 'async_mqtt5/include/async_mqtt5/')"/>
  <xsl:text>]</xsl:text>
  <xsl:value-of select="$newline"/>
  <xsl:value-of select="$newline"/>
  <xsl:text>['Convenience header: ]</xsl:text>
  <xsl:text>[^async_mqtt5.hpp]</xsl:text>
  <xsl:value-of select="$newline"/>
</xsl:template>


<!--========== Class ==========-->

<xsl:template name="class">
  <xsl:variable name="class-name">
    <xsl:call-template name="strip-async-mqtt5-ns">
      <xsl:with-param name="name" select="compoundname"/>
    </xsl:call-template>
  </xsl:variable>
  <xsl:variable name="escaped-class-name">
    <xsl:call-template name="escape-name">
      <xsl:with-param name="text" select="$class-name"/>
    </xsl:call-template>
  </xsl:variable>
  <xsl:variable name="unqualified-class-name">
    <xsl:call-template name="strip-ns">
      <xsl:with-param name="name" select="compoundname"/>
    </xsl:call-template>
  </xsl:variable>
  <xsl:variable name="class-id">
    <xsl:call-template name="make-id">
      <xsl:with-param name="name" select="$class-name"/>
    </xsl:call-template>
  </xsl:variable>
  <xsl:variable name="class-file" select="location/@file"/>
[section:<xsl:value-of select="$class-id"/><xsl:text> </xsl:text><xsl:value-of select="$class-name"/>]

[indexterm1 <xsl:value-of select="$escaped-class-name"/>]

<xsl:apply-templates select="briefdescription" mode="markup"/><xsl:text>

</xsl:text>

<xsl:apply-templates select="templateparamlist"
 mode="class-detail"/><xsl:text>  </xsl:text><xsl:value-of select="@kind"/><xsl:text> </xsl:text><xsl:value-of
 select="$unqualified-class-name"/><xsl:if test="count(basecompoundref[not(contains(.,'::detail'))]) > 0"> :</xsl:if><xsl:text>
</xsl:text><xsl:for-each select="basecompoundref[not(contains(.,'::detail'))]">
<xsl:text>    </xsl:text><xsl:if test="@prot='public'">public </xsl:if><xsl:call-template
 name="strip-async-mqtt5-ns"><xsl:with-param name="name" select="."/></xsl:call-template><xsl:if
 test="not(position() = last())">,</xsl:if><xsl:text>
</xsl:text></xsl:for-each><xsl:text>
</xsl:text>

<xsl:call-template name="class-tables">
  <xsl:with-param name="class-name" select="$class-name"/>
  <xsl:with-param name="class-id" select="$class-id"/>
  <xsl:with-param name="unqualified-class-name" select="$unqualified-class-name"/>
</xsl:call-template>

<xsl:apply-templates select="detaileddescription" mode="markup"/>

<xsl:call-template name="header-requirements">
  <xsl:with-param name="file" select="$class-file"/>
</xsl:call-template>

<xsl:call-template name="class-members">
  <xsl:with-param name="class-name" select="$class-name"/>
  <xsl:with-param name="class-id" select="$class-id"/>
  <xsl:with-param name="class-file" select="$class-file"/>
</xsl:call-template>

[endsect]
</xsl:template>


<xsl:template name="class-tables">
<xsl:param name="class-name"/>
<xsl:param name="class-id"/>
<xsl:param name="unqualified-class-name"/>
<xsl:if test="
    count(
      sectiondef[@kind='public-type'] |
      innerclass[@prot='public' and not(contains(., '_handler'))]) &gt; 0">
[heading Types]
[table
  [[Name][Description]]
<xsl:for-each select="
    sectiondef[@kind='public-type']/memberdef |
    innerclass[@prot='public' and not(contains(., '_handler')) and not(contains(., 'thread_function')) and not(contains(., 'initiate_'))]" mode="class-table">
  <xsl:sort select="concat(name, (.)[not(name)])"/>
  [
<xsl:choose>
  <xsl:when test="count(name) &gt; 0">
    [[link async_mqtt5.ref.<xsl:value-of select="$class-id"/>.<xsl:value-of select="name"/>
      <xsl:text> </xsl:text>[*<xsl:value-of select="name"/>]]]
    [<xsl:call-template name="escape-name"><xsl:with-param name="text" select="briefdescription"/></xsl:call-template>]
  </xsl:when>
  <xsl:otherwise>
    <xsl:variable name="type-name">
      <xsl:call-template name="strip-async-mqtt5-ns">
        <xsl:with-param name="name" select="."/>
      </xsl:call-template>
    </xsl:variable>
    <xsl:variable name="unqualified-type-name">
      <xsl:call-template name="strip-ns">
        <xsl:with-param name="name" select="."/>
      </xsl:call-template>
    </xsl:variable>
    <xsl:variable name="type-id">
      <xsl:call-template name="make-id">
        <xsl:with-param name="name" select="$type-name"/>
      </xsl:call-template>
    </xsl:variable>
    <xsl:variable name="type-ref-id" select="@refid"/>
    [[link async_mqtt5.ref.<xsl:value-of select="$type-id"/>
      <xsl:text> </xsl:text>[*<xsl:value-of select="$unqualified-type-name"/>]]]
    [<xsl:value-of select="(($all-compounddefs)[@id=$type-ref-id])[1]/briefdescription"/>]
  </xsl:otherwise>
</xsl:choose>
  ]
</xsl:for-each>
]
</xsl:if>

<xsl:if test="count(sectiondef[@kind='public-func' or @kind='public-static-func']) > 0">
[heading Member Functions]
[table
  [[Name][Description]]
<xsl:for-each select="sectiondef[@kind='public-func' or @kind='public-static-func']/memberdef" mode="class-table">
  <xsl:sort select="name"/>
  <xsl:variable name="name">
    <xsl:value-of select="name"/>
  </xsl:variable>
  <xsl:variable name="escaped-name">
    <xsl:call-template name="escape-name">
      <xsl:with-param name="text" select="$name"/>
    </xsl:call-template>
  </xsl:variable>
  <xsl:variable name="id">
    <xsl:call-template name="make-id">
      <xsl:with-param name="name" select="$name"/>
      <xsl:with-param name="static" select="@static"/>
    </xsl:call-template>
  </xsl:variable>
  <xsl:variable name="doxygen-id">
    <xsl:value-of select="@id"/>
  </xsl:variable>
  <xsl:variable name="overload-count">
    <xsl:value-of select="count(../memberdef[name = $name])"/>
  </xsl:variable>
  <xsl:variable name="overload-position">
    <xsl:for-each select="../memberdef[name = $name]">
      <xsl:if test="@id = $doxygen-id">
        <xsl:value-of select="position()"/>
      </xsl:if>
    </xsl:for-each>
  </xsl:variable>
  <xsl:if test="$overload-position = 1">
  [
    [[link async_mqtt5.ref.<xsl:value-of select="$class-id"/>.<xsl:value-of select="$id"/>
      <xsl:text> </xsl:text>[*<xsl:value-of select="$escaped-name"/><xsl:text>]]</xsl:text>
      <xsl:if test="$name=$unqualified-class-name"> [constructor]</xsl:if>
      <xsl:if test="starts-with($name, '~')"> [destructor]</xsl:if>
      <xsl:if test="@static='yes'"> [static]</xsl:if>
      <xsl:text>]
    [</xsl:text><xsl:apply-templates select="briefdescription" mode="markup"/>
  </xsl:if>
  <xsl:if test="not($overload-position = 1) and not(briefdescription = preceding-sibling::*/briefdescription)">
    <xsl:value-of select="$newline"/>
    <xsl:text>     [hr]</xsl:text>
    <xsl:value-of select="$newline"/>
    <xsl:text>     </xsl:text>
    <xsl:apply-templates select="briefdescription" mode="markup"/>
  </xsl:if>
  <xsl:if test="$overload-position = $overload-count">
  <xsl:text>]
  ]
  </xsl:text>
  </xsl:if>
</xsl:for-each>
]
</xsl:if>

<xsl:if test="count(sectiondef[@kind='protected-func' or @kind='protected-static-func']) > 0">
[heading Protected Member Functions]
[table
  [[Name][Description]]
<xsl:for-each select="sectiondef[@kind='protected-func' or @kind='protected-static-func']/memberdef" mode="class-table">
  <xsl:sort select="name"/>
  <xsl:variable name="name">
    <xsl:value-of select="name"/>
  </xsl:variable>
  <xsl:variable name="id">
    <xsl:call-template name="make-id">
      <xsl:with-param name="name" select="$name"/>
      <xsl:with-param name="static" select="@static"/>
    </xsl:call-template>
  </xsl:variable>
  <xsl:variable name="doxygen-id">
    <xsl:value-of select="@id"/>
  </xsl:variable>
  <xsl:variable name="overload-count">
    <xsl:value-of select="count(../memberdef[name = $name])"/>
  </xsl:variable>
  <xsl:variable name="overload-position">
    <xsl:for-each select="../memberdef[name = $name]">
      <xsl:if test="@id = $doxygen-id">
        <xsl:value-of select="position()"/>
      </xsl:if>
    </xsl:for-each>
  </xsl:variable>
  <xsl:if test="$overload-position = 1">
  [
    [[link async_mqtt5.ref.<xsl:value-of select="$class-id"/>.<xsl:value-of select="$id"/>
      <xsl:text> </xsl:text>[*<xsl:value-of select="$name"/><xsl:text>]]</xsl:text>
      <xsl:if test="$name=$unqualified-class-name"> [constructor]</xsl:if>
      <xsl:if test="starts-with($name, '~')"> [destructor]</xsl:if>
      <xsl:if test="@static='yes'"> [static]</xsl:if>
      <xsl:text>]
    [</xsl:text><xsl:apply-templates select="briefdescription" mode="markup"/>
  </xsl:if>
  <xsl:if test="not($overload-position = 1) and not(briefdescription = preceding-sibling::*/briefdescription)">
    <xsl:value-of select="$newline"/>
    <xsl:text>     [hr]</xsl:text>
    <xsl:value-of select="$newline"/>
    <xsl:text>     </xsl:text>
    <xsl:apply-templates select="briefdescription" mode="markup"/>
  </xsl:if>
  <xsl:if test="$overload-position = $overload-count">
  <xsl:text>]
  ]
  </xsl:text>
  </xsl:if>
</xsl:for-each>
]
</xsl:if>

<xsl:if test="$class-name = 'execution_context::service'">
<xsl:if test="count(sectiondef[@kind='private-func']) > 0">
[heading Private Member Functions]
[table
  [[Name][Description]]
<xsl:for-each select="sectiondef[@kind='private-func']/memberdef" mode="class-table">
  <xsl:sort select="name"/>
  <xsl:variable name="name">
    <xsl:value-of select="name"/>
  </xsl:variable>
  <xsl:variable name="id">
    <xsl:call-template name="make-id">
      <xsl:with-param name="name" select="$name"/>
      <xsl:with-param name="static" select="@static"/>
    </xsl:call-template>
  </xsl:variable>
  <xsl:variable name="doxygen-id">
    <xsl:value-of select="@id"/>
  </xsl:variable>
  <xsl:variable name="overload-count">
    <xsl:value-of select="count(../memberdef[name = $name])"/>
  </xsl:variable>
  <xsl:variable name="overload-position">
    <xsl:for-each select="../memberdef[name = $name]">
      <xsl:if test="@id = $doxygen-id">
        <xsl:value-of select="position()"/>
      </xsl:if>
    </xsl:for-each>
  </xsl:variable>
  <xsl:if test="$overload-position = 1">
  [
    [[link async_mqtt5.ref.<xsl:value-of select="$class-id"/>.<xsl:value-of select="$id"/>
      <xsl:text> </xsl:text>[*<xsl:value-of select="$name"/><xsl:text>]]</xsl:text>
      <xsl:if test="$name=$unqualified-class-name"> [constructor]</xsl:if>
      <xsl:if test="starts-with($name, '~')"> [destructor]</xsl:if>
      <xsl:if test="@static='yes'"> [static]</xsl:if>
      <xsl:text>]
    [</xsl:text><xsl:apply-templates select="briefdescription" mode="markup"/>
  </xsl:if>
  <xsl:if test="not($overload-position = 1) and not(briefdescription = preceding-sibling::*/briefdescription)">
    <xsl:value-of select="$newline"/>
    <xsl:text>     [hr]</xsl:text>
    <xsl:value-of select="$newline"/>
    <xsl:text>     </xsl:text>
    <xsl:apply-templates select="briefdescription" mode="markup"/>
  </xsl:if>
  <xsl:if test="$overload-position = $overload-count">
  <xsl:text>]
  ]
  </xsl:text>
  </xsl:if>
</xsl:for-each>
]
</xsl:if>
</xsl:if>

<xsl:if test="count(sectiondef[@kind='public-attrib' or @kind='public-static-attrib']) > 0">
[heading Data Members]
[table
  [[Name][Description]]
<xsl:for-each select="sectiondef[@kind='public-attrib' or @kind='public-static-attrib']/memberdef" mode="class-table">
  <xsl:sort select="name"/>
  [
    [[link async_mqtt5.ref.<xsl:value-of select="$class-id"/>.<xsl:value-of select="name"/>
      <xsl:text> </xsl:text>[*<xsl:value-of select="name"/><xsl:text>]]</xsl:text>
      <xsl:if test="@static='yes'"> [static]</xsl:if>]
    [<xsl:apply-templates select="briefdescription" mode="markup"/>]
  ]
</xsl:for-each>
]
</xsl:if>

<xsl:if test="count(sectiondef[@kind='protected-attrib' or @kind='protected-static-attrib']/memberdef[not(name='impl_')]) > 0">
[heading Protected Data Members]
[table
  [[Name][Description]]
<xsl:for-each select="sectiondef[@kind='protected-attrib' or @kind='protected-static-attrib']/memberdef[not(name='impl_')]" mode="class-table">
  <xsl:sort select="name"/>
  [
    [[link async_mqtt5.ref.<xsl:value-of select="$class-id"/>.<xsl:value-of select="name"/>
      <xsl:text> </xsl:text>[*<xsl:value-of select="name"/><xsl:text>]]</xsl:text>
      <xsl:if test="@static='yes'"> [static]</xsl:if>]
    [<xsl:value-of select="briefdescription"/>]
  ]
</xsl:for-each>
]
</xsl:if>

<xsl:if test="count(sectiondef[@kind='friend']/memberdef[not(type = 'friend class') and not(contains(name, '_helper'))]) &gt; 0">
[heading Friends]
[table
  [[Name][Description]]
<xsl:for-each select="sectiondef[@kind='friend']/memberdef[not(type = 'friend class') and not(contains(name, '_helper'))]" mode="class-table">
  <xsl:sort select="name"/>
  <xsl:variable name="name">
    <xsl:value-of select="name"/>
  </xsl:variable>
  <xsl:variable name="id">
    <xsl:call-template name="make-id">
      <xsl:with-param name="name" select="$name"/>
    </xsl:call-template>
  </xsl:variable>
  <xsl:variable name="doxygen-id">
    <xsl:value-of select="@id"/>
  </xsl:variable>
  <xsl:variable name="overload-count">
    <xsl:value-of select="count(../memberdef[name = $name])"/>
  </xsl:variable>
  <xsl:variable name="overload-position">
    <xsl:for-each select="../memberdef[name = $name]">
      <xsl:if test="@id = $doxygen-id">
        <xsl:value-of select="position()"/>
      </xsl:if>
    </xsl:for-each>
  </xsl:variable>
  <xsl:if test="$overload-position = 1">
  [
    [[link async_mqtt5.ref.<xsl:value-of select="$class-id"/>.<xsl:value-of select="$id"/>
      <xsl:text> </xsl:text>[*<xsl:value-of select="$name"/><xsl:text>]]]
    [</xsl:text><xsl:value-of select="briefdescription"/>
  </xsl:if>
  <xsl:if test="not($overload-position = 1) and not(briefdescription = preceding-sibling::*/briefdescription)">
    <xsl:value-of select="$newline"/>
    <xsl:text>     [hr]</xsl:text>
    <xsl:value-of select="$newline"/>
    <xsl:text>     </xsl:text>
    <xsl:value-of select="briefdescription"/>
  </xsl:if>
  <xsl:if test="$overload-position = $overload-count">
  <xsl:text>]
  ]
  </xsl:text>
  </xsl:if>
</xsl:for-each>
]
</xsl:if>

<xsl:if test="count(sectiondef[@kind='related']/memberdef) &gt; 0">
[heading Related Functions]
[table
  [[Name][Description]]
<xsl:for-each select="sectiondef[@kind='related']/memberdef" mode="class-table">
  <xsl:sort select="name"/>
  <xsl:variable name="name">
    <xsl:value-of select="name"/>
  </xsl:variable>
  <xsl:variable name="id">
    <xsl:call-template name="make-id">
      <xsl:with-param name="name" select="$name"/>
    </xsl:call-template>
  </xsl:variable>
  <xsl:variable name="doxygen-id">
    <xsl:value-of select="@id"/>
  </xsl:variable>
  <xsl:variable name="overload-count">
    <xsl:value-of select="count(../memberdef[name = $name])"/>
  </xsl:variable>
  <xsl:variable name="overload-position">
    <xsl:for-each select="../memberdef[name = $name]">
      <xsl:if test="@id = $doxygen-id">
        <xsl:value-of select="position()"/>
      </xsl:if>
    </xsl:for-each>
  </xsl:variable>
  <xsl:if test="$overload-position = 1">
  [
    [[link async_mqtt5.ref.<xsl:value-of select="$class-id"/>.<xsl:value-of select="$id"/>
      <xsl:text> </xsl:text>[*<xsl:value-of select="$name"/><xsl:text>]]]
    [</xsl:text><xsl:value-of select="briefdescription"/>
  </xsl:if>
  <xsl:if test="not($overload-position = 1) and not(briefdescription = preceding-sibling::*/briefdescription)">
    <xsl:value-of select="$newline"/>
    <xsl:value-of select="$newline"/>
    <xsl:text>     </xsl:text>
    <xsl:value-of select="briefdescription"/>
  </xsl:if>
  <xsl:if test="$overload-position = $overload-count">
  <xsl:text>]
  ]
  </xsl:text>
  </xsl:if>
</xsl:for-each>
]
</xsl:if>

</xsl:template>


<xsl:template name="class-members">
<xsl:param name="class-name"/>
<xsl:param name="class-id"/>
<xsl:param name="class-file"/>
<xsl:apply-templates select="sectiondef[@kind='public-type' or @kind='public-func' or @kind='public-static-func' or @kind='public-attrib' or @kind='public-static-attrib' or @kind='protected-func' or @kind='protected-static-func' or @kind='protected-attrib' or @kind='protected-static-attrib' or @kind='friend' or @kind='related']/memberdef[not(type = 'friend class') and not(contains(name, '_helper')) and not(name = 'impl_')]" mode="class-detail">
  <xsl:sort select="name"/>
  <xsl:with-param name="class-name" select="$class-name"/>
  <xsl:with-param name="class-id" select="$class-id"/>
  <xsl:with-param name="class-file" select="$class-file"/>
</xsl:apply-templates>
<xsl:if test="$class-name = 'execution_context::service'">
  <xsl:apply-templates select="sectiondef[@kind='private-func']/memberdef[not(type = 'friend class') and not(contains(name, '_helper'))]" mode="class-detail">
    <xsl:sort select="name"/>
    <xsl:with-param name="class-name" select="$class-name"/>
    <xsl:with-param name="class-id" select="$class-id"/>
    <xsl:with-param name="class-file" select="$class-file"/>
  </xsl:apply-templates>
</xsl:if>
</xsl:template>


<!-- Class detail -->

<xsl:template match="memberdef" mode="class-detail">
  <xsl:param name="class-name"/>
  <xsl:param name="class-id"/>
  <xsl:param name="class-file"/>
  <xsl:variable name="name">
    <xsl:value-of select="name"/>
  </xsl:variable>
  <xsl:variable name="escaped-name">
    <xsl:call-template name="escape-name">
      <xsl:with-param name="text" select="$name"/>
    </xsl:call-template>
  </xsl:variable>
  <xsl:variable name="escaped-class-name">
    <xsl:call-template name="escape-name">
      <xsl:with-param name="text" select="$class-name"/>
    </xsl:call-template>
  </xsl:variable>
  <xsl:variable name="id">
    <xsl:call-template name="make-id">
      <xsl:with-param name="name" select="$name"/>
      <xsl:with-param name="static" select="@static"/>
    </xsl:call-template>
  </xsl:variable>
  <xsl:variable name="doxygen-id">
    <xsl:value-of select="@id"/>
  </xsl:variable>
  <xsl:variable name="overload-count">
    <xsl:value-of select="count(../memberdef[name = $name])"/>
  </xsl:variable>
  <xsl:variable name="overload-position">
    <xsl:for-each select="../memberdef[name = $name]">
      <xsl:if test="@id = $doxygen-id">
        <xsl:value-of select="position()"/>
      </xsl:if>
    </xsl:for-each>
  </xsl:variable>

<xsl:if test="$overload-count &gt; 1 and $overload-position = 1">
[section:<xsl:value-of select="$id"/><xsl:text> </xsl:text>
<xsl:value-of select="$class-name"/>::<xsl:value-of select="$escaped-name"/>]

<xsl:text>[indexterm2 </xsl:text>
<xsl:value-of select="$escaped-name"/>
<xsl:text>..</xsl:text>
<xsl:value-of select="$escaped-class-name"/>
<xsl:text>] </xsl:text>

<xsl:apply-templates select="briefdescription" mode="markup"/><xsl:text>
</xsl:text>

<xsl:for-each select="../memberdef[name = $name]">
<xsl:if test="position() &gt; 1 and not(briefdescription = preceding-sibling::*/briefdescription)">
  <xsl:value-of select="$newline"/>
  <xsl:apply-templates select="briefdescription" mode="markup"/>
  <xsl:value-of select="$newline"/>
</xsl:if>
<xsl:text>
</xsl:text><xsl:apply-templates select="templateparamlist" mode="class-detail"/>
<xsl:text>  </xsl:text>
 <xsl:if test="@explicit='yes'">explicit </xsl:if>
 <xsl:if test="@static='yes'">static </xsl:if>
 <xsl:if test="@virt='virtual'">virtual </xsl:if>
 <xsl:variable name="stripped-type">
  <xsl:call-template name="cleanup-type">
    <xsl:with-param name="name" select="type"/>
  </xsl:call-template>
 </xsl:variable>
<xsl:variable name="impl-keyword">
 <xsl:call-template name="implementation-keyword">
   <xsl:with-param name="argsstring" select="argsstring"/>
 </xsl:call-template>
</xsl:variable>
 <xsl:if test="string-length($stripped-type) &gt; 0">
 <xsl:value-of select="$stripped-type"/><xsl:text> </xsl:text>
</xsl:if>``[link async_mqtt5.ref.<xsl:value-of select="$class-id"/>.<xsl:value-of
 select="$id"/>.overload<xsl:value-of select="position()"/><xsl:text> </xsl:text><xsl:value-of
 select="name"/>]``(<xsl:apply-templates select="param"
 mode="class-detail"/><xsl:if test="count(param) &gt; 0"><xsl:text>
  </xsl:text></xsl:if>)<xsl:if test="@const='yes'"> const</xsl:if><xsl:value-of select="$impl-keyword"/>;
<xsl:text>  ``  [''''&amp;raquo;'''</xsl:text>
<xsl:text> [link async_mqtt5.ref.</xsl:text>
<xsl:value-of select="$class-id"/>.<xsl:value-of
 select="$id"/>.overload<xsl:value-of select="position()"/>
<xsl:text> more...]]``
</xsl:text>
</xsl:for-each>
</xsl:if>

[section:<xsl:if test="$overload-count = 1"><xsl:value-of select="$id"/></xsl:if>
<xsl:if test="$overload-count &gt; 1">overload<xsl:value-of select="$overload-position"/></xsl:if>
<xsl:text> </xsl:text><xsl:value-of select="$class-name"/>::<xsl:value-of select="$escaped-name"/>
<xsl:if test="$overload-count &gt; 1"> (<xsl:value-of
 select="$overload-position"/> of <xsl:value-of select="$overload-count"/> overloads)</xsl:if>]

<xsl:if test="not(starts-with($doxygen-id, ../../@id))">
<xsl:variable name="inherited-from" select="($all-compounddefs)[starts-with($doxygen-id, @id)]/compoundname"/>
<xsl:if test="not(contains($inherited-from, '::detail'))">
['Inherited from <xsl:call-template name="strip-async-mqtt5-ns">
<xsl:with-param name="name" select="$inherited-from"/>
</xsl:call-template>.]<xsl:text>

</xsl:text></xsl:if></xsl:if>

<xsl:if test="$overload-count = 1">
  <xsl:text>[indexterm2 </xsl:text>
  <xsl:value-of select="$escaped-name"/>
  <xsl:text>..</xsl:text>
  <xsl:value-of select="$escaped-class-name"/>
  <xsl:text>] </xsl:text>
</xsl:if>

<xsl:apply-templates select="briefdescription" mode="markup"/><xsl:text>
</xsl:text>

  <xsl:choose>
    <xsl:when test="@kind='typedef'">
      <xsl:call-template name="typedef" mode="class-detail">
        <xsl:with-param name="class-name" select="$class-name"/>
      </xsl:call-template>
    </xsl:when>
    <xsl:when test="@kind='variable'">
      <xsl:call-template name="variable" mode="class-detail"/>
    </xsl:when>
    <xsl:when test="@kind='enum'">
      <xsl:call-template name="enum" mode="class-detail">
        <xsl:with-param name="enum-name" select="$class-name"/>
        <xsl:with-param name="id" select="concat($class-id, '.', $id)"/>
      </xsl:call-template>
    </xsl:when>
    <xsl:when test="@kind='function'">
      <xsl:call-template name="function" mode="class-detail"/>
    </xsl:when>
    <xsl:when test="@kind='friend'">
      <xsl:call-template name="function" mode="class-detail"/>
    </xsl:when>
  </xsl:choose>

<xsl:if test="count(detaileddescription/*) &gt; 0">
[heading Description]
</xsl:if>
<xsl:text>
</xsl:text><xsl:apply-templates select="detaileddescription" mode="markup"/>

<xsl:if test="@kind='typedef' or @kind='friend'">
  <xsl:call-template name="header-requirements">
    <xsl:with-param name="file" select="$class-file"/>
  </xsl:call-template>
</xsl:if>

[endsect]

<xsl:if test="$overload-count &gt; 1 and $overload-position = $overload-count">
[endsect]
</xsl:if>
</xsl:template>


<xsl:template name="typedef">
<xsl:param name="class-name"/>
<xsl:text>
  </xsl:text>typedef <xsl:value-of select="type"/><xsl:text> </xsl:text><xsl:value-of select="name"/>;<xsl:text>

</xsl:text>
<xsl:if test="count(type/ref) &gt; 0 and not(contains(type, '*')) and not(contains(name, 'polymorphic_query_result_type'))">
  <xsl:variable name="class-refid">
     <xsl:for-each select="type/ref[1]">
       <xsl:value-of select="@refid"/>
     </xsl:for-each>
  </xsl:variable>
  <xsl:variable name="name" select="name"/>
  <xsl:for-each select="($all-compounddefs)[@id=$class-refid]">
    <xsl:call-template name="class-tables">
      <xsl:with-param name="class-name">
        <xsl:value-of select="concat($class-name, '::', $name)"/>
      </xsl:with-param>
      <xsl:with-param name="class-id">
        <xsl:call-template name="make-id">
          <xsl:with-param name="name">
            <xsl:call-template name="strip-async-mqtt5-ns">
              <xsl:with-param name="name" select="compoundname"/>
            </xsl:call-template>
          </xsl:with-param>
        </xsl:call-template>
      </xsl:with-param>
      <xsl:with-param name="unqualified-class-name">
        <xsl:call-template name="strip-ns">
          <xsl:with-param name="name" select="compoundname"/>
        </xsl:call-template>
      </xsl:with-param>
    </xsl:call-template>
    <xsl:apply-templates select="detaileddescription" mode="markup"/>
  </xsl:for-each>
</xsl:if>
</xsl:template>


<xsl:template name="variable">
<xsl:if test="contains(name, 'is_applicable_property_v')">
<xsl:text>
  template &lt;typename T&gt;</xsl:text>
</xsl:if>
<xsl:if test="contains(name, 'context_as')">
<xsl:text>
  template &lt;typename U&gt;</xsl:text>
</xsl:if>
<xsl:text>
  </xsl:text><xsl:if test="@static='yes'">static </xsl:if><xsl:value-of
 select="type"/><xsl:text> </xsl:text><xsl:value-of select="name"/>
 <xsl:if test="count(initializer) = 1"><xsl:text> = </xsl:text>
 <xsl:value-of select="initializer"/></xsl:if>;
</xsl:template>


<xsl:template name="enum">
<xsl:param name="enum-name"/>
<xsl:param name="id"/>
<xsl:text>  enum </xsl:text>
<xsl:if test="name='cancellation_type'">
  <xsl:text>class </xsl:text>
</xsl:if>
<xsl:value-of select="name"/>
<xsl:if test="name='cancellation_type'">
  <xsl:text> : unsigned int</xsl:text>
</xsl:if>
<xsl:text>
</xsl:text><xsl:if test="count(enumvalue) &gt; 0">
<xsl:value-of select="$newline"/>
<xsl:for-each select="enumvalue">
  <xsl:text>[indexterm2 </xsl:text>
  <xsl:value-of select="name"/>
  <xsl:text>..</xsl:text>
  <xsl:value-of select="$enum-name"/>
  <xsl:text>]</xsl:text>
  <xsl:value-of select="$newline"/>
</xsl:for-each>
[heading Values]
[table
  [[Name] [Description]]
<xsl:for-each select="enumvalue">
  [[<xsl:value-of select="name"/>][<xsl:apply-templates select="detaileddescription" mode="markup"/>]]
</xsl:for-each>
]
</xsl:if>
</xsl:template>

<xsl:template name="implementation-keyword">
<xsl:param name="argsstring"/>
  <xsl:choose>
    <xsl:when test="contains($argsstring, 'delete')">
      <xsl:text> = delete</xsl:text>
    </xsl:when>
    <xsl:when test="contains($argsstring, 'default')">
      <xsl:text> = default</xsl:text>
    </xsl:when>
  </xsl:choose>
</xsl:template>

<xsl:template name="function">
<xsl:text>
</xsl:text>
<xsl:variable name="doxygen-id">
  <xsl:value-of select="@id"/>
</xsl:variable>
<xsl:choose>
  <xsl:when test="count(($all-memberdefs)[@id=$doxygen-id]/templateparamlist) = 1">
    <xsl:apply-templates select="($all-memberdefs)[@id=$doxygen-id]/templateparamlist" mode="class-detail"/>
  </xsl:when>
  <xsl:otherwise>
    <xsl:apply-templates select="templateparamlist" mode="class-detail"/>
  </xsl:otherwise>
</xsl:choose>
<xsl:variable name="stripped-type">
 <xsl:call-template name="cleanup-type">
   <xsl:with-param name="name" select="type"/>
 </xsl:call-template>
</xsl:variable>
<xsl:variable name="impl-keyword">
 <xsl:call-template name="implementation-keyword">
   <xsl:with-param name="argsstring" select="argsstring"/>
 </xsl:call-template>
</xsl:variable>
<xsl:text>  </xsl:text><xsl:if test="@static='yes'">static </xsl:if><xsl:if 
 test="@virt='virtual'">virtual </xsl:if><xsl:if
 test="string-length($stripped-type) &gt; 0"><xsl:value-of select="$stripped-type"/><xsl:text> </xsl:text></xsl:if>
<xsl:value-of select="name"/>(<xsl:apply-templates select="param"
 mode="class-detail"/><xsl:if test="count(param) &gt; 0"><xsl:text>
  </xsl:text></xsl:if>)<xsl:if test="@const='yes'"> const</xsl:if><xsl:value-of select="$impl-keyword"/>;
</xsl:template>


<xsl:template match="templateparamlist" mode="class-detail">
<xsl:text>  </xsl:text>template &lt;<xsl:apply-templates select="param" mode="class-detail-template"/>
<xsl:text>
  </xsl:text>&gt;
</xsl:template>

<xsl:template name="mqtt-property">
<xsl:param name="qualified-name"/>
<xsl:variable name="property-name">
  <xsl:choose>
    <xsl:when test="contains($qualified-name, 'will_props')">will_props</xsl:when>
    <!-- disconnect props must be above connect props -->
    <xsl:when test="contains($qualified-name, 'disconnect_props')">disconnect_props</xsl:when>
    <xsl:when test="contains($qualified-name, 'connect_props')">connect_props</xsl:when>
    <xsl:when test="contains($qualified-name, 'connack_props')">connack_props</xsl:when>
    <xsl:when test="contains($qualified-name, 'publish_props')">publish_props</xsl:when>
    <xsl:when test="contains($qualified-name, 'puback_props')">puback_props</xsl:when>
    <xsl:when test="contains($qualified-name, 'pubrec_props')">pubrec_props</xsl:when>
    <xsl:when test="contains($qualified-name, 'pubrel_props')">pubrel_props</xsl:when>
    <xsl:when test="contains($qualified-name, 'pubcomp_props')">pubcomp_props</xsl:when>
    <!-- unsubscribe & unsuback props must be above subscribe & suback props -->
    <xsl:when test="contains($qualified-name, 'unsubscribe_props')">unsubscribe_props</xsl:when>
    <xsl:when test="contains($qualified-name, 'unsuback_props')">unsuback_props</xsl:when>
    <xsl:when test="contains($qualified-name, 'subscribe_props')">subscribe_props</xsl:when>
    <xsl:when test="contains($qualified-name, 'suback_props')">suback_props</xsl:when>
    <xsl:when test="contains($qualified-name, 'auth_props')">auth_props</xsl:when>
    <xsl:otherwise></xsl:otherwise>
  </xsl:choose>
</xsl:variable>
<xsl:if test="string-length($property-name) &gt; 0">
  <xsl:value-of select="substring-before($qualified-name, $property-name)"/>
  <xsl:text>``[link async_mqtt5.ref.</xsl:text>
  <xsl:value-of select="$property-name"/>
  <xsl:text> </xsl:text>
  <xsl:value-of select="$property-name"/>
  <xsl:text>]``</xsl:text>
  <xsl:value-of select="substring-after($qualified-name, $property-name)"/>
  <xsl:text> </xsl:text>
  <xsl:value-of select="declname"/>
</xsl:if>
</xsl:template>

<xsl:template name="mqtt-template">
<xsl:param name="qualified-name"/>
<xsl:variable name="template-type">
  <xsl:choose>
    <xsl:when test="contains($qualified-name, 'CompletionToken')">CompletionToken</xsl:when>
    <xsl:when test="contains($qualified-name, 'Executor')">Executor</xsl:when>
    <xsl:when test="contains($qualified-name, 'ExecutionContext')">ExecutionContext</xsl:when>
    <xsl:when test="contains($qualified-name, 'StreamType')">StreamType</xsl:when>
    <xsl:when test="contains($qualified-name, 'TlsContext')">TlsContext</xsl:when>
    <xsl:when test="contains($qualified-name, 'is_authenticator')">is_authenticator</xsl:when>
    <xsl:when test="contains($qualified-name, 'LoggerType')">LoggerType</xsl:when>
    <xsl:otherwise></xsl:otherwise>
  </xsl:choose>
</xsl:variable>
<xsl:choose>
  <xsl:when test="string-length($template-type) &gt; 0">
    <xsl:value-of select="substring-before($qualified-name, $template-type)"/>
      <xsl:text>__</xsl:text><xsl:value-of select="$template-type"/><xsl:text>__</xsl:text>
  <xsl:value-of select="substring-after($qualified-name, $template-type)"/>
  <xsl:if test="count(declname) &gt; 0">
    <xsl:text> </xsl:text>
    <xsl:value-of select="declname"/>
  </xsl:if>
  </xsl:when>
  <xsl:otherwise>
    <xsl:value-of select="$qualified-name"/>
  </xsl:otherwise>
</xsl:choose>
</xsl:template>

<xsl:template name="mqtt-ref-id">
<xsl:param name="ref"/>
<xsl:choose>
  <xsl:when test="$ref = 'executor_type'">mqtt_client.<xsl:value-of select="$ref"/></xsl:when>
  <xsl:when test="$ref = 'error'">client.<xsl:value-of select="$ref"/></xsl:when>
  <xsl:otherwise><xsl:value-of select="$ref"/></xsl:otherwise>
</xsl:choose>
</xsl:template>

<xsl:template name="mqtt-type">
<xsl:param name="type"/>
<xsl:choose>
  <xsl:when test="contains(type, '_props')">
    <xsl:call-template name="mqtt-property">
      <xsl:with-param name="qualified-name" select="$type"/>
    </xsl:call-template>
  </xsl:when>
  <xsl:when test="count(type/ref) &gt; 0">
    <xsl:variable name="typename" select="type/ref"/>
    <xsl:value-of select="substring-before($type, $typename)"/>
    <xsl:text>``[link async_mqtt5.ref.</xsl:text>
    <xsl:call-template name="mqtt-ref-id">
      <!-- a single type can have either 0 or 1 refs -->
      <xsl:with-param name="ref" select="type/ref[1]"/>
    </xsl:call-template>
    <xsl:text> </xsl:text>
    <xsl:value-of select="$typename"/>
    <xsl:text>]``</xsl:text>
    <xsl:value-of select="substring-after($type, $typename)"/>
    <xsl:if test="count(declname) &gt; 0">
      <xsl:text> </xsl:text>
      <xsl:value-of select="declname"/>
    </xsl:if>
  </xsl:when>
  <!-- unfortunately, there is no better way to differentiate between template types and non-documented types -->
  <xsl:when test="contains(type, 'CompletionToken')
    or contains(type, 'Executor')
    or contains(type, 'ExecutionContext')
    or contains(type, 'TlsContext')
    or contains(type, 'StreamType')
    or contains(type, 'is_authenticator')
    or contains(type, 'LoggerType')">
    <xsl:call-template name="mqtt-template">
      <xsl:with-param name="qualified-name" select="$type"/>
    </xsl:call-template>
  </xsl:when>
  <xsl:otherwise>
    <xsl:value-of select="type"/>
    <xsl:if test="count(declname) &gt; 0">
      <xsl:text> </xsl:text>
      <xsl:value-of select="declname"/>
    </xsl:if>
  </xsl:otherwise>
</xsl:choose>
</xsl:template>

<xsl:template match="param" mode="class-detail-template">
<xsl:text>
      </xsl:text>
      <xsl:choose>
        <xsl:when test="contains(type, 'typename')">
          <xsl:call-template name="mqtt-template">
            <xsl:with-param name="qualified-name" select="type"/>
          </xsl:call-template>
        </xsl:when>
        <xsl:otherwise>
          <xsl:call-template name="mqtt-type">
            <xsl:with-param name="type" select="type"/>
          </xsl:call-template>
        </xsl:otherwise>
      </xsl:choose>
    <xsl:if test="count(defval) > 0"> = <xsl:value-of select="defval"/>
    </xsl:if>
    <xsl:if test="not(position() = last())">,</xsl:if>
</xsl:template>


<xsl:template match="param" mode="class-detail">
<xsl:text>
      </xsl:text>
  <xsl:choose>
    <xsl:when test="string-length(array) &gt; 0">
      <xsl:value-of select="substring-before(type, '&amp;')"/>
      <xsl:text>(&amp;</xsl:text>
      <xsl:value-of select="declname"/>
      <xsl:text>)</xsl:text>
      <xsl:value-of select="array"/>
    </xsl:when>
    <xsl:otherwise>
      <xsl:call-template name="mqtt-type">
        <xsl:with-param name="type" select="type"/>
      </xsl:call-template>
    </xsl:otherwise>
  </xsl:choose>
  <xsl:if test="count(defval) > 0"> = <xsl:value-of select="defval"/>
  </xsl:if>
  <xsl:if test="not(position() = last())">,</xsl:if>
</xsl:template>


<xsl:template match="*" mode="class-detail"/>


<!--========== Namespace ==========-->

<xsl:template name="namespace-memberdef">
  <xsl:variable name="name">
    <xsl:call-template name="strip-async-mqtt5-ns">
      <xsl:with-param name="name" select="concat(../../compoundname, '::', name)"/>
    </xsl:call-template>
  </xsl:variable>
  <xsl:variable name="unqualified-name">
    <xsl:call-template name="strip-ns">
      <xsl:with-param name="name" select="$name"/>
    </xsl:call-template>
  </xsl:variable>
  <xsl:variable name="id">
    <xsl:call-template name="make-id">
      <xsl:with-param name="name" select="$name"/>
    </xsl:call-template>
  </xsl:variable>
  <xsl:variable name="doxygen-id">
    <xsl:value-of select="@id"/>
  </xsl:variable>
  <xsl:variable name="overload-count">
    <xsl:value-of select="count(../memberdef[name = $unqualified-name])"/>
  </xsl:variable>
  <xsl:variable name="overload-position">
    <xsl:for-each select="../memberdef[name = $unqualified-name]">
      <xsl:if test="@id = $doxygen-id">
        <xsl:value-of select="position()"/>
      </xsl:if>
    </xsl:for-each>
  </xsl:variable>

<xsl:if test="$overload-count &gt; 1 and $overload-position = 1">
[section:<xsl:value-of select="$id"/><xsl:text> </xsl:text><xsl:value-of select="$name"/>]

<xsl:text>[indexterm1 </xsl:text>
<xsl:value-of select="$name"/>
<xsl:text>]</xsl:text>
<xsl:value-of select="$newline"/>

<xsl:for-each select="($all-compounddefs)[@kind='group' and compoundname=$name]">
  <xsl:apply-templates select="briefdescription" mode="markup"/>
  <xsl:value-of select="$newline"/>
</xsl:for-each>

<xsl:for-each select="../memberdef[name = $unqualified-name]">
<xsl:variable name="stripped-type">
 <xsl:call-template name="cleanup-type">
   <xsl:with-param name="name" select="type"/>
 </xsl:call-template>
</xsl:variable>
<xsl:if test="position() = 1 or not(briefdescription = preceding-sibling::memberdef[1]/briefdescription)">
  <xsl:apply-templates select="briefdescription" mode="markup"/>
</xsl:if>
<xsl:text>
</xsl:text><xsl:apply-templates select="templateparamlist" mode="class-detail"/>
<xsl:text>  </xsl:text><xsl:if test="string-length($stripped-type) &gt; 0"><xsl:value-of
 select="$stripped-type"/><xsl:text> </xsl:text></xsl:if>``[link async_mqtt5.ref.<xsl:value-of
 select="$id"/>.overload<xsl:value-of select="position()"/><xsl:text> </xsl:text>
<xsl:value-of select="name"/>]``(<xsl:apply-templates select="param" mode="class-detail"/>);
<xsl:text>  ``  [''''&amp;raquo;'''</xsl:text>
<xsl:text> [link async_mqtt5.ref.</xsl:text>
<xsl:value-of select="$id"/>.overload<xsl:value-of select="position()"/>
<xsl:text> more...]]``
</xsl:text>
</xsl:for-each>

<xsl:for-each select="($all-compounddefs)[@kind='group' and compoundname=$name]">
  <xsl:apply-templates select="detaileddescription" mode="markup"/>
</xsl:for-each>

<xsl:call-template name="header-requirements">
  <xsl:with-param name="file" select="location/@file"/>
</xsl:call-template>

</xsl:if>

[section:<xsl:if test="$overload-count = 1"><xsl:value-of select="$id"/></xsl:if>
<xsl:if test="$overload-count &gt; 1">overload<xsl:value-of select="$overload-position"/>
</xsl:if><xsl:text> </xsl:text><xsl:value-of select="$name"/>
<xsl:if test="$overload-count &gt; 1"> (<xsl:value-of
 select="$overload-position"/> of <xsl:value-of select="$overload-count"/> overloads)</xsl:if>]

<xsl:if test="$overload-count = 1">
  <xsl:text>[indexterm1 </xsl:text>
  <xsl:value-of select="$name"/>
  <xsl:text>]</xsl:text>
  <xsl:value-of select="$newline"/>
</xsl:if>

<xsl:apply-templates select="briefdescription" mode="markup"/><xsl:text>
</xsl:text>

  <xsl:choose>
    <xsl:when test="@kind='typedef'">
      <xsl:call-template name="typedef">
        <xsl:with-param name="class-name" select="$name"/>
      </xsl:call-template>
    </xsl:when>
    <xsl:when test="@kind='variable'">
      <xsl:call-template name="variable"/>
    </xsl:when>
    <xsl:when test="@kind='enum'">
      <xsl:call-template name="enum">
        <xsl:with-param name="enum-name" select="$name"/>
        <xsl:with-param name="id" select="$id"/>
      </xsl:call-template>
    </xsl:when>
    <xsl:when test="@kind='function'">
      <xsl:call-template name="function"/>
    </xsl:when>
  </xsl:choose>

<xsl:text>
</xsl:text><xsl:apply-templates select="detaileddescription" mode="markup"/>

<xsl:if test="$overload-count = 1">
  <xsl:call-template name="header-requirements">
    <xsl:with-param name="file" select="location/@file"/>
  </xsl:call-template>
</xsl:if>

[endsect]
<xsl:if test="$overload-count &gt; 1 and $overload-position = $overload-count">
[endsect]
</xsl:if>
</xsl:template>


</xsl:stylesheet>
