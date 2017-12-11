# Put user-specific changes in your own Makefile.user.
# Make will silently continue if that file does not exist.
-include Makefile.user

TARGET   = paper

STYFILES = $(wildcard *.sty)

PACKAGE=acmart

PSTOPDF ?= pstopdf

.SUFFIXES:	.plot .eps .pdf

all: $(TARGET).pdf 

$(TARGET).pdf: ${PDF_FIGURES} $(TARGET).bbl $(PACKAGE).cls $(wildcard *.tex) $(STYFILES) 
	pdflatex $(TARGET).tex
	pdflatex $(TARGET).tex

$(TARGET).aux: $(TARGET).tex $(wildcard *.tex)
	pdflatex $(TARGET)

%.cls: %.ins %.dtx
	pdflatex $<

$(TARGET).bbl: $(TARGET).bib $(TARGET).aux $(PACKAGE).cls ACM-Reference-Format.bst
	pdflatex $(TARGET).tex
	bibtex $(TARGET)
	pdflatex $(TARGET).tex
	pdflatex $(TARGET).tex
	while ( grep -q '^LaTeX Warning: Label(s) may have changed' $*.log) \
	do pdflatex $(TARGET); done

tags: TAGS
.PHONY: tags TAGS
TAGS: 
	etags `latex-process-inputs --list paper.tex`

clean:
	rm -f $(TARGET).pdf
	rm -f $(TARGET).log
	rm -f $(TARGET).blg
	rm -f $(TARGET).aux
	rm -f $(TARGET).bbl
	rm -f $(TARGET).dvi
	rm -f $(TARGET).out
	rm -f $(TARGET).toc
	rm -f ./fig/*.eps
