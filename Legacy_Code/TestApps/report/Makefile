# Put user-specific changes in your own Makefile.user.
# Make will silently continue if that file does not exist.
-include Makefile.user

TARGET   = paper

STYFILES = $(wildcard *.sty)

PSTOPDF ?= pstopdf

.SUFFIXES:	.plot .eps .pdf

all: $(TARGET).pdf 

$(TARGET).pdf: ${PDF_FIGURES} $(TARGET).bbl $(wildcard *.tex) $(STYFILES) 
	pdflatex $(TARGET).tex
	pdflatex $(TARGET).tex

$(TARGET).aux: $(TARGET).tex $(wildcard *.tex)
	pdflatex $(TARGET)

$(TARGET).bbl: $(TARGET).bib $(TARGET).aux
	pdflatex $(TARGET).tex
	bibtex $(TARGET)

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
