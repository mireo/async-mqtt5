#! /bin/bash

doxygen reference.dox
cd xml
xsltproc combine.xslt index.xml > all.xml
cd ..
xsltproc reference.xsl xml/all.xml > qbk/reference/reference.qbk
rm -rf xml
