#!/bin/bash -x

# http://ploticus.sourceforge.net/gallery/bars8.htm

out="$(basename "$0" .sh)"
here="$(cd $(dirname "$0") && pwd)"

data="
n logging vector_clock
1  2.9 2.3
4  2.9 2.3
16 2.9 2.3
64 2.9 2.3
256 3.0 2.3
1K 3.1 2.4
4K 3.3 2.6
16K 3.7 3.2
64K 4.8 4.4
"

# http://ploticus.sourceforge.net/doc/lineplot.html
script="
#proc getdata
  fieldnameheader: yes
  nfields: 3
  showdata: yes
  data:$data

#proc areadef
  rectangle: 1 1 5 3
  xrange: 1 9
  yrange: 0 5
  xaxis.stubs: datafields=1
  xaxis.label: # Points
  yaxis.stubs: incremental 0.5
  yaxis.stubformat: %2.1f
  yaxis.label: Time (s)

#proc lineplot
  yfield: 2
  linedetails: color=red width=1.0
  sort: no
  legendlabel: Logging instrumentation
  pointsymbol: shape=pixcircle fillcolor=red style=solid radius=0.03

#proc lineplot
  yfield: 3
  linedetails: color=blue width=1.0
  sort: no
  legendlabel: Vector clock instrumentation
  pointsymbol: shape=pixtriangle fillcolor=blue style=solid radius=0.03

#proc legend
  format: down
  separation: 0.05
  textdetails: adjust=0,0.03
  location: min+0.5 max
"

(
    set -e
    cd "$here"
    ploticus -croprel "-0.08,0,+0.05,0" -eps <(echo "$script") -o "$out".eps && \
	epstopdf "$out".eps && \
	rm "$out".eps
    echo "Generated ${here}/${out}.pdf"
)


