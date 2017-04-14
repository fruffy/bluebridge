package main

import (
	"bufio"
	"fmt"
	"log"
	"os"
	"runtime/pprof"
	"sort"
	"strconv"
	"time"
)

const DAMP = 0.85

var (
	itt     = 20
	tests   = 20
	profile = false
)

type page struct {
	incomming_rank float32
	edges          []int
}

type page2 struct {
	incomming_rank float32
	edge_offset    int
	num_edges      int
}

func newPage(pid int) *page {
	return &page{incomming_rank: 0.0, edges: make([]int, 0)}
}

func (p *page) String() string {
	var s string
	s += fmt.Sprintf("I-Rank: %f ", p.incomming_rank)
	s += "Edges: ["
	for _, e := range p.edges {
		s += fmt.Sprintf("%d,", e)
	}
	s += "] "
	return s
}

func printPages() {
	for i, id := range ids {
		fmt.Printf("Page: %d,", id)
		fmt.Printf("Num_Edges: %d,", apages[id].num_edges)
		fmt.Printf("Edge_Offset: %d,", apages[id].edge_offset)
		fmt.Printf("Rank: %f,", rank[i])
		fmt.Printf("Incomming-Rank: %f,", apages[id].incomming_rank)
		fmt.Printf("EdgeNorm: %f,", edgenorm[i])
		fmt.Print("Edges: [")
		for j := 0; j < apages[id].num_edges; j++ {
			fmt.Printf("%d,", edges[apages[id].edge_offset+j])
		}
		fmt.Println("]")
	}
}

/*
func (p page2) String() string {
	var s string
	s += fmt.Sprintf("I-Rank: %f ", p.incomming_rank)
	s += "Edges: ["
	for _, e := range p.edges {
		s += fmt.Sprintf("%d,", e)
	}
	s += "] "
	return s
}*/

func (p page2) MemoryString() string {
	var s string
	s += fmt.Sprintf("---------page----------\n")
	s += fmt.Sprintf("---------rank----------\n")
	s += fmt.Sprintf("%+v\n", &p.incomming_rank)
	s += fmt.Sprintf("---offset----#edges----\n")
	s += fmt.Sprintf("%d-%d\n", p.edge_offset, p.num_edges)
	s += fmt.Sprintf("---------edges----------\n")
	for i := 0; i < p.num_edges; i++ {
		s += fmt.Sprintf("%+v\n", &(edges[p.edge_offset+i]))
	}
	s += "]\n"
	return s
}

var pages map[int]*page

//apages is a memory map for vertex id's and is potentially sparse
//index i of apages corresponds to the id of the vertex it represents
//It is similar to a hash
var apages []page2
var ids []int
var edgenorm []float32
var rank []float32
var edges []int

func main() {
	graphFile := os.Args[1]
	if profile {
		f, err := os.Create("cpu.prof")
		if err != nil {
			log.Fatal("could not create CPU profile: ", err)
		}
		if err := pprof.StartCPUProfile(f); err != nil {
			log.Fatal("could not start CPU profile: ", err)
		}
		defer pprof.StopCPUProfile()
	}
	pages = make(map[int]*page, 0)
	start := time.Now()
	parseFile(graphFile)
	dir := time.Now().Sub(start)
	fmt.Printf("Parse Duration %f\n", dir.Seconds())
	//parseFile("../tiny.txt")
	//fmt.Println("parsed")
	total := 0.0
	for i := 0; i < tests; i++ {
		start := time.Now()
		//pageRank2(itt)
		pageRank3(itt)
		dir := time.Now().Sub(start)
		fmt.Printf("Rounds %d Duration %f\n", itt, dir.Seconds())
		total += dir.Seconds()
	}
	fmt.Printf("Average time %f\n", total/float64(tests))

	//for i := range pages {
	//	fmt.Print((*pages[i]).String())
	//}
}
func pageRank3(rounds int) {
	var outrank float32
	var alpha = (1 - DAMP) / float32(len(pages))
	for i := 0; i < rounds; i++ {
		//printPages()
		for j := 0; j < len(ids); j++ {
			outrank = rank[j] / edgenorm[j]
			for k := 0; k < apages[j].num_edges; k++ {
				//send message to another page
				apages[edges[apages[j].edge_offset+k]].incomming_rank += outrank
			}
		}
		for j := 0; j < len(ids); j++ {
			rank[j] = alpha + (DAMP * apages[j].incomming_rank)
			//apages[j].incomming_rank[j] = 0.0
		}
	}
	//printPages()
}

/*
func pageRank2(rounds int) {
	var outrank float32
	var alpha = (1 - DAMP) * (1.0 / float32(len(pages)))
	for i := 0; i < rounds; i++ {
		for j := 0; j < len(ids); j++ {
			outrank = rank[j] / edgenorm[j]

			for k := 0; k < len(apages[j].edges); k++ {
				apages[apages[j].edges[k]].incomming_rank += outrank
			}
		}
		for j := 0; j < len(ids); j++ {
			rank[j] = alpha + (DAMP * apages[j].incomming_rank)
		}
	}
}*/
/*
func pageRank(rounds int) {
	for i := 0; i < rounds; i++ {
		for j := 0; j < len(ids); j++ {
			sendMessages(ids[j])
		}
		for j := 0; j < len(ids); j++ {
			update(ids[j])
		}
	}
}

func sendMessages(id int) {
	var outrank float32
	var p page2
	p = apages[id]
	outrank = rank[id] / float32((p.num_edges + 1))
	for i := 0; i < p.num_edges; i++ {
		apages[p.edges[i]].incomming_rank += outrank
	}
}

func update(id int) {
	apages[id].rank = (1-DAMP)*(1.0/float32(len(pages))) + (DAMP * apages[id].incomming_rank)
}
*/

func parseFile(filename string) {
	f, err := os.Open(filename)
	if err != nil {
		log.Fatalf("%s", err.Error)
	}
	defer f.Close()
	scanner := bufio.NewScanner(f)

	var i int
	var buf string
	var max int
	var e int
	for scanner.Scan() {
		buf = scanner.Text()
		//fmt.Println(buf)

		for i = 0; i < len(buf); i++ {
			if buf[i] == '\t' {
				a, err := strconv.Atoi(buf[0:i])
				if err != nil {
					log.Fatal(err)
				}
				b, err := strconv.Atoi(buf[i+1 : len(buf)])
				if err != nil {
					log.Fatal(err)
				}
				//fmt.Printf("%d -> %d\n", a, b)
				if _, ok := pages[a]; !ok {
					pages[a] = newPage(a)
				}
				if _, ok := pages[b]; !ok {
					pages[b] = newPage(b)
				}
				if a > max {
					max = a
				}
				if b > max {
					max = b
				}
				pages[a].edges = append(pages[a].edges, b)
				pages[b].incomming_rank = 0.0
				e++
			}
		}
	}
	ids = make([]int, len(pages))
	//apages and edges are the main structures
	apages = make([]page2, max+1)
	edges = make([]int, e)
	//edgenorm and rank are optimizations
	edgenorm = make([]float32, len(pages))
	rank = make([]float32, len(pages))
	j := 0 // id index
	k := 0 //edge index
	for i := range pages {
		ids[j] = i //id[index] = vertex-id
		j++
	}
	sort.Ints(ids)
	for i, id := range ids {
		apages[id].num_edges = len(pages[id].edges)
		//fmt.Printf("#edges %d %d", id, len(pages[id].edges))
		rank[i] = 1.0
		edgenorm[i] = float32(apages[id].num_edges + 1)
		apages[id].edge_offset = k
		for l := range pages[id].edges {
			edges[k] = pages[id].edges[l]
			k++
		}
	}

	//for i := range apages {
	//	fmt.Println(apages[i].MemoryString())
	//}

	if err := scanner.Err(); err != nil {
		log.Fatal(err)
	}
}

/*
type pageHash [][]*page

func NewPageHash(size int) *pageHash {
	p := make([][]*page, size)
	logsize := math.Sqrt(float32(size))
	for i := range p {
		p[i] = make([]*page, int(logsize))
	}
	return *pageHash(p)
}

func (ph *pageHash) Put(id int) {
	var l1 int
	var l2 int
*/
