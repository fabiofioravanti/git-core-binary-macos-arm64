MAN1_TXT=$(wildcard git-*.txt) gitk.txt
MAN7_TXT=git.txt

DOC_HTML=$(patsubst %.txt,%.html,$(MAN1_TXT) $(MAN7_TXT))

ARTICLES = tutorial
ARTICLES += cvs-migration
ARTICLES += diffcore
ARTICLES += howto-index
ARTICLES += repository-layout
ARTICLES += hooks
# with their own formatting rules.
SP_ARTICLES = glossary howto/revert-branch-rebase

DOC_HTML += $(patsubst %,%.html,$(ARTICLES) $(SP_ARTICLES))

DOC_MAN1=$(patsubst %.txt,%.1,$(MAN1_TXT))
DOC_MAN7=$(patsubst %.txt,%.7,$(MAN7_TXT))

prefix=$(HOME)
bin=$(prefix)/bin
mandir=$(prefix)/man
man1=$(mandir)/man1
man7=$(mandir)/man7
# DESTDIR=

INSTALL=install

#
# Please note that there is a minor bug in asciidoc.
# The version after 6.0.3 _will_ include the patch found here:
#   http://marc.theaimsgroup.com/?l=git&m=111558757202243&w=2
#
# Until that version is released you may have to apply the patch
# yourself - yes, all 6 characters of it!
#

all: html man

html: $(DOC_HTML)


man: man1 man7
man1: $(DOC_MAN1)
man7: $(DOC_MAN7)

install:
	$(INSTALL) -m755 -d $(DESTDIR)/$(man1) $(DESTDIR)/$(man7)
	$(INSTALL) $(DOC_MAN1) $(DESTDIR)/$(man1)
	$(INSTALL) $(DOC_MAN7) $(DESTDIR)/$(man7)

# 'include' dependencies
git-diff-%.txt: diff-format.txt diff-options.txt
	touch $@

clean:
	rm -f *.xml *.html *.1 *.7 howto-index.txt howto/*.html

%.html : %.txt
	asciidoc -b xhtml11 -d manpage $<

%.1 %.7 : %.xml
	xmlto man $<

%.xml : %.txt
	asciidoc -b docbook -d manpage $<

git.html: git.txt ../README

glossary.html : glossary.txt sort_glossary.pl
	cat $< | \
	perl sort_glossary.pl | \
	asciidoc -b xhtml11 - > glossary.html

howto-index.txt: howto-index.sh $(wildcard howto/*.txt)
	rm -f $@+ $@
	sh ./howto-index.sh $(wildcard howto/*.txt) >$@+
	mv $@+ $@

$(patsubst %,%.html,$(ARTICLES)) : %.html : %.txt
	asciidoc -b xhtml11 $*.txt

WEBDOC_DEST = /pub/software/scm/git/docs

$(patsubst %.txt,%.html,$(wildcard howto/*.txt)): %.html : %.txt
	rm -f $@+ $@
	sed -e '1,/^$$/d' $? | asciidoc -b xhtml11 - >$@+
	mv $@+ $@

install-webdoc : html
	sh ./install-webdoc.sh $(WEBDOC_DEST)
